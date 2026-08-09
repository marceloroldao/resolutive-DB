import os,tempfile,struct,zlib,json,shutil
from pathlib import Path
HDR=struct.Struct('>8sQII'); MAGIC=b'BDRSNAP1'

def write_snapshot_atomic(path,state,crash_stage=None):
    path=Path(path); tmp=path.with_suffix('.tmp'); items=sorted(state.items()); body=bytearray()
    for k,v in items:
        kb=k.encode(); vb=v if isinstance(v,bytes) else str(v).encode(); body+=struct.pack('>II',len(kb),len(vb))+kb+vb
    crc=zlib.crc32(body)&0xffffffff
    with open(tmp,'wb',buffering=0) as f:
        f.write(HDR.pack(MAGIC,len(items),len(body),crc)); f.write(body); os.fsync(f.fileno())
    if crash_stage=='before_rename': return
    os.replace(tmp,path)
    dfd=os.open(str(path.parent),os.O_RDONLY)
    try: os.fsync(dfd)
    finally: os.close(dfd)

def read_snapshot(path):
    b=Path(path).read_bytes(); magic,n,bl,crc=HDR.unpack_from(b,0); body=b[HDR.size:]
    if magic!=MAGIC or len(body)!=bl or zlib.crc32(body)&0xffffffff!=crc: raise ValueError('bad snapshot')
    off=0; st={}
    for _ in range(n):
        kl,vl=struct.unpack_from('>II',body,off); off+=8; k=body[off:off+kl].decode(); off+=kl; v=body[off:off+vl]; off+=vl; st[k]=v
    return st

def main():
    td=tempfile.mkdtemp(); p=Path(td)/'snap.bin'; old={f'k{i}':f'v{i}'.encode() for i in range(10000)}; new={**old,**{f'k{i}':b'new' for i in range(5000)}}
    write_snapshot_atomic(p,old); a=read_snapshot(p)==old
    write_snapshot_atomic(p,new,'before_rename'); b=read_snapshot(p)==old; orphan=p.with_suffix('.tmp').exists()
    write_snapshot_atomic(p,new); c=read_snapshot(p)==new
    with open(p.with_suffix('.tmp'),'wb') as f:f.write(os.urandom(257))
    d=read_snapshot(p)==new
    print(json.dumps({'initial_valid':a,'crash_before_rename_preserved_old':b,'orphan_tmp':orphan,'post_rename_valid':c,'corrupt_orphan_tmp_ignored':d},indent=2)); shutil.rmtree(td)
if __name__=='__main__': main()
