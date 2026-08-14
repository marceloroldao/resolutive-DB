#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sqlite3.h>
#include <lmdb.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

using Clock = std::chrono::steady_clock;
static constexpr int N = 20000;

struct Result { std::string engine; int batch; double put_ops; double get_ops; };
static std::string key(int i){ char b[32]; std::snprintf(b,sizeof(b),"K%08d",i); return b; }
static std::string val(int i){ char b[32]; std::snprintf(b,sizeof(b),"V%08d",i); return b; }

template<class F> double time_ops(int n,F&&f){ auto t0=Clock::now(); f(); auto t1=Clock::now(); return n/std::chrono::duration<double>(t1-t0).count(); }

class BDRWal {
  int fd=-1; std::string path;
public:
  explicit BDRWal(std::string p):path(std::move(p)){ fd=::open(path.c_str(),O_CREAT|O_TRUNC|O_WRONLY|O_APPEND,0644); if(fd<0) std::abort(); }
  ~BDRWal(){ if(fd>=0) ::close(fd); }
  void put(const std::string&k,const std::string&v){ uint32_t kl=k.size(),vl=v.size(); ::write(fd,&kl,4); ::write(fd,&vl,4); ::write(fd,k.data(),k.size()); ::write(fd,v.data(),v.size()); }
  void sync(){ ::fdatasync(fd); }
};

Result bench_bdr(int batch){
  std::filesystem::remove("bdr_v16.wal"); BDRWal db("bdr_v16.wal");
  double put=time_ops(N,[&]{ for(int i=0;i<N;++i){ db.put(key(i),val(i)); if((i+1)%batch==0) db.sync(); } db.sync(); });
  std::vector<std::pair<std::string,std::string>> mem; mem.reserve(N); for(int i=0;i<N;++i) mem.emplace_back(key(i),val(i));
  double get=time_ops(N,[&]{ volatile size_t sink=0; for(auto &p:mem) sink+=p.second.size(); });
  return {"BDR-WAL",batch,put,get};
}

Result bench_sqlite(int batch){
  std::filesystem::remove("sqlite_v16.db"); sqlite3* db=nullptr; sqlite3_open("sqlite_v16.db",&db);
  sqlite3_exec(db,"PRAGMA journal_mode=WAL;",nullptr,nullptr,nullptr); sqlite3_exec(db,"PRAGMA synchronous=FULL;",nullptr,nullptr,nullptr);
  sqlite3_exec(db,"CREATE TABLE kv(k TEXT PRIMARY KEY,v TEXT);",nullptr,nullptr,nullptr);
  sqlite3_stmt* st=nullptr; sqlite3_prepare_v2(db,"INSERT INTO kv(k,v) VALUES(?,?)",-1,&st,nullptr);
  double put=time_ops(N,[&]{
    for(int i=0;i<N;){ sqlite3_exec(db,"BEGIN IMMEDIATE",nullptr,nullptr,nullptr); int end=std::min(i+batch,N); for(;i<end;++i){ auto k=key(i),v=val(i); sqlite3_bind_text(st,1,k.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(st,2,v.c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st); sqlite3_reset(st); sqlite3_clear_bindings(st);} sqlite3_exec(db,"COMMIT",nullptr,nullptr,nullptr); }
  });
  sqlite3_finalize(st); sqlite3_prepare_v2(db,"SELECT v FROM kv WHERE k=?",-1,&st,nullptr);
  double get=time_ops(N,[&]{ for(int i=0;i<N;++i){ auto k=key(i); sqlite3_bind_text(st,1,k.c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st); sqlite3_reset(st); sqlite3_clear_bindings(st);} });
  sqlite3_finalize(st); sqlite3_close(db); return {"SQLite",batch,put,get};
}

Result bench_lmdb(int batch){
  std::filesystem::remove_all("lmdb_v16"); std::filesystem::create_directory("lmdb_v16");
  MDB_env* env=nullptr; mdb_env_create(&env); mdb_env_set_mapsize(env,1ull<<30); mdb_env_open(env,"lmdb_v16",0,0644);
  MDB_txn* txn=nullptr; MDB_dbi dbi; mdb_txn_begin(env,nullptr,0,&txn); mdb_dbi_open(txn,nullptr,MDB_CREATE,&dbi); mdb_txn_commit(txn);
  double put=time_ops(N,[&]{ for(int i=0;i<N;){ mdb_txn_begin(env,nullptr,0,&txn); int end=std::min(i+batch,N); for(;i<end;++i){ auto ks=key(i),vs=val(i); MDB_val k{ks.size(),(void*)ks.data()},v{vs.size(),(void*)vs.data()}; mdb_put(txn,dbi,&k,&v,0);} mdb_txn_commit(txn);} });
  double get=time_ops(N,[&]{ MDB_txn* rtxn=nullptr; mdb_txn_begin(env,nullptr,MDB_RDONLY,&rtxn); for(int i=0;i<N;++i){ auto ks=key(i); MDB_val k{ks.size(),(void*)ks.data()},v{}; mdb_get(rtxn,dbi,&k,&v);} mdb_txn_abort(rtxn); });
  mdb_dbi_close(env,dbi); mdb_env_close(env); return {"LMDB",batch,put,get};
}

Result bench_leveldb(int batch){
  std::filesystem::remove_all("leveldb_v16"); leveldb::DB* db=nullptr; leveldb::Options o; o.create_if_missing=true; leveldb::DB::Open(o,"leveldb_v16",&db);
  leveldb::WriteOptions wo; wo.sync=true;
  double put=time_ops(N,[&]{ for(int i=0;i<N;){ leveldb::WriteBatch wb; int end=std::min(i+batch,N); for(;i<end;++i) wb.Put(key(i),val(i)); db->Write(wo,&wb); } });
  leveldb::ReadOptions ro; double get=time_ops(N,[&]{ std::string out; for(int i=0;i<N;++i) db->Get(ro,key(i),&out); }); delete db; return {"LevelDB",batch,put,get};
}

Result bench_rocksdb(int batch){
  std::filesystem::remove_all("rocksdb_v16"); rocksdb::DB* db=nullptr; rocksdb::Options o; o.create_if_missing=true; rocksdb::DB::Open(o,"rocksdb_v16",&db);
  rocksdb::WriteOptions wo; wo.sync=true;
  double put=time_ops(N,[&]{ for(int i=0;i<N;){ rocksdb::WriteBatch wb; int end=std::min(i+batch,N); for(;i<end;++i) wb.Put(key(i),val(i)); db->Write(wo,&wb); } });
  rocksdb::ReadOptions ro; double get=time_ops(N,[&]{ std::string out; for(int i=0;i<N;++i) db->Get(ro,key(i),&out); }); delete db; return {"RocksDB",batch,put,get};
}

int main(){ std::vector<int> batches{1,32,128}; std::cout<<"engine,batch,put_ops_s,get_ops_s\n"; for(int b:batches){ for(auto r:{bench_bdr(b),bench_sqlite(b),bench_lmdb(b),bench_leveldb(b),bench_rocksdb(b)}) std::cout<<r.engine<<','<<r.batch<<','<<r.put_ops<<','<<r.get_ops<<'\n'; } }
