import os, random, sqlite3, struct, tempfile, time, zlib, json, shutil
from pathlib import Path
MAGIC=b'BDRW'; HDR=struct.Struct('>4sBQQII'); PUT=1; DEL=2

def crc_record(op,seq,k,v): return zlib.crc32(bytes([op])+seq.to_bytes(8,'big')+k+v)&0xffffffff
class SegWAL:
    def __init__(self, root, seg_bytes=1<<20, durable=False):
        self.root=Path(root); self.root.mkdir(parents=True, exist_ok=True); self.seg_bytes=seg_bytes; self.durable=durable; self.seq=0; self.seg=0; self.f=None; self._open()
    def _open(self):
        if self.f: self.f.close()
        self.f=open(self.root/f'wal-{self.seg:06d}.log','ab',buffering=0)
    def append(self,op,key,val=b''):
        self.seq+=1; k=key.encode(); v=val if isinstance(val,bytes) else str(val).encode(); crc=crc_record(op,self.seq,k,v); rec=HDR.pack(MAGIC,op,self.seq,len(k),len(v),crc)+k+v
        if self.f.tell()+len(rec)>self.seg_bytes and self.f.tell()>0: self.seg+=1; self._open()
        self.f.write(rec)
        if self.durable: os.fsync(self.f.fileno())
    def close(self):
        if self.f: self.f.close(); self.f=None

def replay(root):
    state={}; last=0; bad=0; gaps=0; records=0
    for p in sorted(Path(root).glob('wal-*.log')):
        data=p.read_bytes(); off=0
        while off+HDR.size<=len(data):
            magic,op,seq,kl,vl,crc=HDR.unpack_from(data,off); off+=HDR.size
            if magic!=MAGIC or off+kl+vl>len(data): bad+=1; break
            k=data[off:off+kl]; v=data[off+kl:off+kl+vl]; off+=kl+vl
            if crc_record(op,seq,k,v)!=crc: bad+=1; break
            if last and seq!=last+1: gaps+=1
            last=seq; records+=1; ks=k.decode()
            if op==PUT: state[ks]=v
            elif op==DEL: state.pop(ks,None)
        if off!=len(data) and bad==0: bad+=1
    return state, {'last_seq':last,'bad':bad,'gaps':gaps,'records':records}

def fuzz(seed,ops=200_000,keys_n=20_000):
    rng=random.Random(seed); ref={}; td=tempfile.mkdtemp(); wal=SegWAL(td,seg_bytes=128*1024)
    for _ in range(ops):
        k=f'k{rng.randrange(keys_n):06d}'
        if rng.random()<0.72:
            v=f'v{rng.getrandbits(64):016x}'.encode(); ref[k]=v; wal.append(PUT,k,v)
        else: ref.pop(k,None); wal.append(DEL,k)
    wal.close(); got,meta=replay(td); ok=(got==ref and meta['bad']==0 and meta['gaps']==0); shutil.rmtree(td); return ok,len(ref),meta

def durability(n=2000,group=100):
    td=tempfile.mkdtemp();
    w=SegWAL(Path(td)/'b1',durable=True); t=time.perf_counter()
    for i in range(n): w.append(PUT,f'k{i}',f'v{i}')
    w.close(); b_each=time.perf_counter()-t
    w=SegWAL(Path(td)/'b2'); t=time.perf_counter()
    for i in range(n):
        w.append(PUT,f'k{i}',f'v{i}')
        if (i+1)%group==0: os.fsync(w.f.fileno())
    os.fsync(w.f.fileno()); w.close(); b_group=time.perf_counter()-t
    p=Path(td)/'per.db'; c=sqlite3.connect(p); c.execute('PRAGMA journal_mode=WAL'); c.execute('PRAGMA synchronous=FULL'); c.execute('CREATE TABLE kv(k TEXT PRIMARY KEY,v BLOB)'); t=time.perf_counter()
    for i in range(n): c.execute('INSERT INTO kv VALUES(?,?)',(f'k{i}',f'v{i}'.encode())); c.commit()
    s_each=time.perf_counter()-t; c.close()
    p=Path(td)/'grp.db'; c=sqlite3.connect(p); c.execute('PRAGMA journal_mode=WAL'); c.execute('PRAGMA synchronous=FULL'); c.execute('CREATE TABLE kv(k TEXT PRIMARY KEY,v BLOB)'); t=time.perf_counter()
    for base in range(0,n,group):
        c.execute('BEGIN')
        for i in range(base,min(base+group,n)): c.execute('INSERT INTO kv VALUES(?,?)',(f'k{i}',f'v{i}'.encode()))
        c.commit()
    s_group=time.perf_counter()-t; c.close(); shutil.rmtree(td)
    return {'n':n,'group':group,'bdr_fsync_each_s':b_each,'bdr_group_s':b_group,'sqlite_commit_each_s':s_each,'sqlite_group_s':s_group}

if __name__=='__main__':
    print(json.dumps({'fuzz':[fuzz(s) for s in [1,2,3,42,99]],'durability':durability()},indent=2))
