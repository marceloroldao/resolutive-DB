#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sqlite3.h>
#include <lmdb.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
using Clock=std::chrono::steady_clock;
static inline uint64_t mix64(uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);} 
static inline uint64_t fnv(const std::string&s){uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
static std::string K(int i){char b[32];std::snprintf(b,sizeof(b),"K%08d",i);return b;} static std::string V(int i){char b[32];std::snprintf(b,sizeof(b),"V%08d",i);return b;}
struct Adr{uint32_t rho;uint64_t fp;}; static Adr enc(const std::string&k,uint32_t M){uint64_t h=mix64(fnv(k));return{uint32_t(h%M),mix64(h^0xD6E8FEB86659FD93ULL)};}
struct Slot{uint64_t k=0,v=0;uint32_t d=0;bool used=false;};
class RH{std::vector<Slot>s;size_t mask;public:RH(){} explicit RH(size_t n){size_t c=8;while(c<n*2)c<<=1;s.resize(c);mask=c-1;}void put(uint64_t k,uint64_t v){size_t i=mix64(k)&mask;Slot cur{k,v,0,true};for(;;){auto&x=s[i];if(!x.used){x=cur;return;}if(x.k==cur.k){x.v=v;return;}if(x.d<cur.d)std::swap(x,cur);i=(i+1)&mask;++cur.d;}}bool get(uint64_t k,uint64_t&v)const{if(s.empty())return false;size_t i=mix64(k)&mask;uint32_t d=0;for(;;){auto&x=s[i];if(!x.used||x.d<d)return false;if(x.k==k){v=x.v;return true;}i=(i+1)&mask;++d;}}};
class BDR{uint32_t M;std::vector<RH>p;int fd=-1;public:BDR(int N,const char*path):M(std::max(1,N/256)),p(M){std::vector<size_t>cnt(M);for(int i=0;i<N;++i)cnt[enc(K(i),M).rho]++;for(uint32_t i=0;i<M;++i)p[i]=RH(cnt[i]);fd=::open(path,O_CREAT|O_TRUNC|O_WRONLY|O_APPEND,0644);}~BDR(){if(fd>=0)::close(fd);}void put(const std::string&k,const std::string&v,int id){auto a=enc(k,M);p[a.rho].put(a.fp,id);uint32_t kl=k.size(),vl=v.size();::write(fd,&kl,4);::write(fd,&vl,4);::write(fd,k.data(),kl);::write(fd,v.data(),vl);}void sync(){::fdatasync(fd);}bool get(const std::string&k,uint64_t&v)const{auto a=enc(k,M);return p[a.rho].get(a.fp,v);}};
struct R{std::string e;int b;double put,get;}; template<class F>double rate(int n,F&&f){auto a=Clock::now();f();auto z=Clock::now();return n/std::chrono::duration<double>(z-a).count();}
R bdr(int N,int b){std::filesystem::remove("v21.bdr");BDR d(N,"v21.bdr");double wp=rate(N,[&]{for(int i=0;i<N;++i){d.put(K(i),V(i),i);if((i+1)%b==0)d.sync();}d.sync();});volatile uint64_t sink=0;double rd=rate(N,[&]{for(int i=0;i<N;++i){uint64_t v=0;d.get(K(i),v);sink^=v;}});return{"BDR-v21",b,wp,rd};}
R sqlite(int N,int b){std::filesystem::remove("v21.sqlite");sqlite3*d=nullptr;sqlite3_open("v21.sqlite",&d);sqlite3_exec(d,"PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;CREATE TABLE kv(k TEXT PRIMARY KEY,v TEXT);",0,0,0);sqlite3_stmt*s=nullptr;sqlite3_prepare_v2(d,"INSERT INTO kv VALUES(?,?)",-1,&s,0);double wp=rate(N,[&]{for(int i=0;i<N;){sqlite3_exec(d,"BEGIN IMMEDIATE",0,0,0);int e=std::min(i+b,N);for(;i<e;++i){auto k=K(i),v=V(i);sqlite3_bind_text(s,1,k.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,v.c_str(),-1,SQLITE_TRANSIENT);sqlite3_step(s);sqlite3_reset(s);sqlite3_clear_bindings(s);}sqlite3_exec(d,"COMMIT",0,0,0);}});sqlite3_finalize(s);sqlite3_prepare_v2(d,"SELECT v FROM kv WHERE k=?",-1,&s,0);double rd=rate(N,[&]{for(int i=0;i<N;++i){auto k=K(i);sqlite3_bind_text(s,1,k.c_str(),-1,SQLITE_TRANSIENT);sqlite3_step(s);sqlite3_reset(s);sqlite3_clear_bindings(s);}});sqlite3_finalize(s);sqlite3_close(d);return{"SQLite",b,wp,rd};}
R lmdb(int N,int b){std::filesystem::remove_all("v21.lmdb");std::filesystem::create_directory("v21.lmdb");MDB_env*e;MDB_txn*t;MDB_dbi db;mdb_env_create(&e);mdb_env_set_mapsize(e,1ull<<30);mdb_env_open(e,"v21.lmdb",0,0644);mdb_txn_begin(e,0,0,&t);mdb_dbi_open(t,0,MDB_CREATE,&db);mdb_txn_commit(t);double wp=rate(N,[&]{for(int i=0;i<N;){mdb_txn_begin(e,0,0,&t);int z=std::min(i+b,N);for(;i<z;++i){auto ks=K(i),vs=V(i);MDB_val k{ks.size(),(void*)ks.data()},v{vs.size(),(void*)vs.data()};mdb_put(t,db,&k,&v,0);}mdb_txn_commit(t);}});double rd=rate(N,[&]{mdb_txn_begin(e,0,MDB_RDONLY,&t);for(int i=0;i<N;++i){auto ks=K(i);MDB_val k{ks.size(),(void*)ks.data()},v{};mdb_get(t,db,&k,&v);}mdb_txn_abort(t);});mdb_dbi_close(e,db);mdb_env_close(e);return{"LMDB",b,wp,rd};}
R level(int N,int b){std::filesystem::remove_all("v21.level");leveldb::DB*d;leveldb::Options o;o.create_if_missing=true;leveldb::DB::Open(o,"v21.level",&d);leveldb::WriteOptions w;w.sync=true;double wp=rate(N,[&]{for(int i=0;i<N;){leveldb::WriteBatch x;int z=std::min(i+b,N);for(;i<z;++i)x.Put(K(i),V(i));d->Write(w,&x);}});leveldb::ReadOptions ro;double rd=rate(N,[&]{std::string out;for(int i=0;i<N;++i)d->Get(ro,K(i),&out);});delete d;return{"LevelDB",b,wp,rd};}
R rocks(int N,int b){std::filesystem::remove_all("v21.rocks");rocksdb::DB*d;rocksdb::Options o;o.create_if_missing=true;rocksdb::DB::Open(o,"v21.rocks",&d);rocksdb::WriteOptions w;w.sync=true;double wp=rate(N,[&]{for(int i=0;i<N;){rocksdb::WriteBatch x;int z=std::min(i+b,N);for(;i<z;++i)x.Put(K(i),V(i));d->Write(w,&x);}});rocksdb::ReadOptions ro;double rd=rate(N,[&]{std::string out;for(int i=0;i<N;++i)d->Get(ro,K(i),&out);});delete d;return{"RocksDB",b,wp,rd};}
int main(int argc,char**argv){int N=argc>1?std::stoi(argv[1]):10000;std::cout<<"engine,batch,put_ops_s,get_ops_s\n";for(int b:{1,32,128})for(auto&r:{bdr(N,b),sqlite(N,b),lmdb(N,b),level(N,b),rocks(N,b)})std::cout<<r.e<<','<<r.b<<','<<r.put<<','<<r.get<<'\n';}
