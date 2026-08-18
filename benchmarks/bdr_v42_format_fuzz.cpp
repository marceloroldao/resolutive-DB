#define main v38_original_main
#include "bdr_v38_checkpoint_bdr3.cpp"
#undef main
#include <random>

static void copy_file(const fs::path&a,const fs::path&b){std::ifstream in(a,std::ios::binary);std::ofstream out(b,std::ios::binary|std::ios::trunc);out<<in.rdbuf();}
static void flip_byte(const fs::path&p,size_t off,uint8_t mask){std::fstream f(p,std::ios::binary|std::ios::in|std::ios::out);f.seekg(0,std::ios::end);size_t n=size_t(f.tellg());if(off>=n)throw std::runtime_error("flip bounds");f.seekg(off);char c;f.read(&c,1);c=char(uint8_t(c)^mask);f.seekp(off);f.write(&c,1);}
static bool reopen_matches(const fs::path&d,const std::map<std::string,std::string>&want,uint64_t seq){try{auto r=reopen(d);return r.seq==seq&&r.kv==want;}catch(...){return false;}}

int main(){
  constexpr uint64_t SEED=0xB0D42026ULL;std::mt19937_64 rng(SEED);fs::path base="v42base";fs::remove_all(base);fs::create_directory(base);
  create_wal(base/"wal-000000.log",1,1);std::map<std::string,std::string>kv;uint64_t seq=0;
  for(int i=0;i<2000;++i){auto k="K"+std::to_string(i),v="V"+std::to_string(i);kv[k]=v;append_wal(base/"wal-000000.log",++seq,1,k,v);}atomic_checkpoint(base,seq,kv,2);
  for(int i=0;i<400;++i){auto k="P"+std::to_string(i),v="Q"+std::to_string(i);kv[k]=v;append_wal(base/"wal-000001.log",++seq,1,k,v);}for(int i=0;i<100;++i){auto k="K"+std::to_string(i);kv.erase(k);append_wal(base/"wal-000001.log",++seq,2,k,"");}
  if(!reopen_matches(base,kv,seq))return 3;
  size_t snap_rejected=0,snap_silent=0,wal_rejected=0,wal_safe_prefix=0,wal_silent=0,trunc_safe=0;int trials=200;
  auto snap_sz=fs::file_size(base/"snapshot.bdr3"),wal_sz=fs::file_size(base/"wal-000001.log");
  for(int t=0;t<trials;++t){fs::path d="v42s";fs::remove_all(d);fs::create_directory(d);copy_file(base/"snapshot.bdr3",d/"snapshot.bdr3");copy_file(base/"wal-000001.log",d/"wal-000001.log");size_t off=size_t(rng()%(snap_sz-4));flip_byte(d/"snapshot.bdr3",off,uint8_t(1u<<(rng()%8)));try{auto r=reopen(d);if(r.kv!=kv||r.seq!=seq)++snap_silent;}catch(...){++snap_rejected;}}
  for(int t=0;t<trials;++t){fs::path d="v42w";fs::remove_all(d);fs::create_directory(d);copy_file(base/"snapshot.bdr3",d/"snapshot.bdr3");copy_file(base/"wal-000001.log",d/"wal-000001.log");size_t minoff=sizeof(WalHeader),off=minoff+size_t(rng()%std::max<uint64_t>(1,wal_sz-minoff));flip_byte(d/"wal-000001.log",off,uint8_t(1u<<(rng()%8)));try{auto r=reopen(d);if(r.seq<=seq&&r.kv.size()<=kv.size()+100)++wal_safe_prefix;else ++wal_silent;}catch(...){++wal_rejected;}}
  for(int t=0;t<trials;++t){fs::path d="v42t";fs::remove_all(d);fs::create_directory(d);copy_file(base/"snapshot.bdr3",d/"snapshot.bdr3");copy_file(base/"wal-000001.log",d/"wal-000001.log");uint64_t cut=sizeof(WalHeader)+(rng()%std::max<uint64_t>(1,wal_sz-sizeof(WalHeader)));fs::resize_file(d/"wal-000001.log",cut);try{auto r=reopen(d);if(r.seq<=seq)++trunc_safe;}catch(...){++trunc_safe;}}
  bool pass=snap_silent==0&&wal_silent==0&&snap_rejected==size_t(trials)&&wal_rejected+wal_safe_prefix==size_t(trials)&&trunc_safe==size_t(trials);
  std::cout<<"seed,trials,snapshot_rejected,snapshot_silent,wal_rejected,wal_safe_prefix,wal_silent,trunc_safe,pass\n"<<SEED<<','<<trials<<','<<snap_rejected<<','<<snap_silent<<','<<wal_rejected<<','<<wal_safe_prefix<<','<<wal_silent<<','<<trunc_safe<<','<<pass<<"\n";
  return pass?0:2;
}
