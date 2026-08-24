#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sys/uio.h>
#include <sqlite3.h>

using Clock = std::chrono::steady_clock;
static constexpr uint32_t MAGIC=0x32445242;
#pragma pack(push,1)
struct RecHdr { uint32_t magic; uint32_t klen; uint32_t vlen; uint64_t seq; };
#pragma pack(pop)

static double pct(std::vector<double> v,double q){
  if(v.empty()) return 0.0;
  std::sort(v.begin(),v.end());
  double idx=q*(v.size()-1); size_t lo=(size_t)idx, hi=std::min(lo+1,v.size()-1); double f=idx-lo;
  return v[lo]*(1-f)+v[hi]*f;
}
struct Result { double sec, ops_s, amort_us, batch_p50_us, batch_p99_us; };

Result bench_bdr(const std::vector<std::string>& keys,const std::vector<std::string>& vals,size_t batch){
  const char* path="bdr_v14.wal"; ::unlink(path);
  int fd=::open(path,O_CREAT|O_TRUNC|O_WRONLY|O_APPEND,0644);
  if(fd<0){ perror("open"); std::exit(2); }
  std::unordered_map<std::string,std::string> map; map.reserve(keys.size()*2);
  uint64_t seq=0; std::vector<double> batch_lat;
  auto all0=Clock::now();
  for(size_t i=0;i<keys.size();i+=batch){
    auto t0=Clock::now(); size_t e=std::min(keys.size(),i+batch);
    for(size_t j=i;j<e;++j){
      RecHdr h{MAGIC,(uint32_t)keys[j].size(),(uint32_t)vals[j].size(),++seq};
      iovec iov[3]={{&h,sizeof(h)},{(void*)keys[j].data(),keys[j].size()},{(void*)vals[j].data(),vals[j].size()}};
      ssize_t want=sizeof(h)+keys[j].size()+vals[j].size();
      if(::writev(fd,iov,3)!=want){ perror("writev"); std::exit(3); }
      map[keys[j]]=vals[j];
    }
    if(::fdatasync(fd)!=0){ perror("fdatasync"); std::exit(4); }
    batch_lat.push_back(std::chrono::duration<double,std::micro>(Clock::now()-t0).count());
  }
  double sec=std::chrono::duration<double>(Clock::now()-all0).count();
  ::close(fd);
  return {sec,keys.size()/sec,sec*1e6/keys.size(),pct(batch_lat,.5),pct(batch_lat,.99)};
}

static void check(int rc,sqlite3* db,const char* where){
  if(rc!=SQLITE_OK && rc!=SQLITE_DONE){ std::cerr<<where<<": "<<sqlite3_errmsg(db)<<"\n"; std::exit(5); }
}
Result bench_sqlite(const std::vector<std::string>& keys,const std::vector<std::string>& vals,size_t batch){
  const char* path="bdr_v14.sqlite"; ::unlink(path); ::unlink("bdr_v14.sqlite-wal"); ::unlink("bdr_v14.sqlite-shm");
  sqlite3* db=nullptr; check(sqlite3_open(path,&db),db,"open"); char* err=nullptr;
  auto exec=[&](const char*s){ int rc=sqlite3_exec(db,s,nullptr,nullptr,&err); if(rc!=SQLITE_OK){ std::cerr<<(err?err:"")<<"\n"; sqlite3_free(err); std::exit(6);} };
  exec("PRAGMA journal_mode=WAL;");
  exec("PRAGMA synchronous=FULL;");
  exec("PRAGMA wal_autocheckpoint=0;");
  exec("CREATE TABLE kv(k TEXT PRIMARY KEY, v TEXT) WITHOUT ROWID;");
  sqlite3_stmt* st=nullptr; check(sqlite3_prepare_v2(db,"INSERT OR REPLACE INTO kv(k,v) VALUES(?,?)",-1,&st,nullptr),db,"prepare");
  std::vector<double> batch_lat;
  auto all0=Clock::now();
  for(size_t i=0;i<keys.size();i+=batch){
    auto t0=Clock::now(); exec("BEGIN IMMEDIATE;"); size_t e=std::min(keys.size(),i+batch);
    for(size_t j=i;j<e;++j){
      sqlite3_bind_text(st,1,keys[j].c_str(),-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(st,2,vals[j].c_str(),-1,SQLITE_TRANSIENT);
      if(sqlite3_step(st)!=SQLITE_DONE){ std::cerr<<sqlite3_errmsg(db)<<"\n"; std::exit(7); }
      sqlite3_reset(st); sqlite3_clear_bindings(st);
    }
    exec("COMMIT;");
    batch_lat.push_back(std::chrono::duration<double,std::micro>(Clock::now()-t0).count());
  }
  double sec=std::chrono::duration<double>(Clock::now()-all0).count();
  sqlite3_finalize(st); sqlite3_close(db);
  return {sec,keys.size()/sec,sec*1e6/keys.size(),pct(batch_lat,.5),pct(batch_lat,.99)};
}

int main(){
  const size_t N=8192;
  std::vector<std::string> keys(N),vals(N);
  for(size_t i=0;i<N;++i){ keys[i]="K"+std::to_string(i); vals[i]="value_payload_"+std::to_string(i)+"_abcdefghijklmnopqrstuvwxyz"; }
  std::cout<<"engine,batch,seconds,ops_s,amort_us_op,batch_p50_us,batch_p99_us\n";
  for(size_t b: {1ul,8ul,16ul,32ul,64ul,128ul}){
    auto r=bench_bdr(keys,vals,b);
    std::cout<<"BDR,"<<b<<","<<r.sec<<","<<r.ops_s<<","<<r.amort_us<<","<<r.batch_p50_us<<","<<r.batch_p99_us<<"\n";
    auto s=bench_sqlite(keys,vals,b);
    std::cout<<"SQLite,"<<b<<","<<s.sec<<","<<s.ops_s<<","<<s.amort_us<<","<<s.batch_p50_us<<","<<s.batch_p99_us<<"\n";
  }
}
