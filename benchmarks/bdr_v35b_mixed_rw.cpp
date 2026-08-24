#define main v20_unused_main
#include "bdr_v20_density_target_engine.cpp"
#undef main
#define main v30_unused_main
#include "bdr_v30_ticketed_pipeline.cpp"
#undef main
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

struct M35{std::string engine,profile;int write_pct;double ops_s;uint64_t errors;size_t writes;};
static inline uint64_t r35(uint64_t x){return mix64(x+0x3500350035003500ULL);} 
static size_t read_id(uint64_t&r,size_t N,bool hot){r=r35(r);if(!hot)return r%N;bool h=(r%100)<80;r=r35(r);return h?r%std::max<size_t>(1,N/100):r%N;}
static bool is_write(uint64_t&r,int pct){r=r35(r);return int(r%100)<pct;}

template<class F>static double sec35(F&&f){auto a=Clock::now();f();return std::chrono::duration<double>(Clock::now()-a).count();}

static M35 bdr35(size_t N,size_t OPS,int T,int wp,bool hot){uint32_t M=pow2ceil(std::max<uint64_t>(64,(N+255)/256));auto ks=keys("sequential",N,M);std::vector<A>as;as.reserve(N);for(auto&k:ks)as.push_back(enc(k,M));LocalDB db(M,as,16);for(size_t i=0;i<N;++i)db.put(as[i],i);db.finish();std::filesystem::remove("v35b.wal");TicketWal wal("v35b.wal",OPS*96+4096,512);std::atomic<size_t>next{0},nw{0};std::atomic<uint64_t>err{0};double s=sec35([&]{std::vector<std::thread>ts;for(int t=0;t<T;++t)ts.emplace_back([&,t]{uint64_t rng=0xB035B000ULL+t,last=0;int pend=0;for(;;){size_t op=next.fetch_add(1);if(op>=OPS)break;if(is_write(rng,wp)){size_t i=nw.fetch_add(1);last=wal.submit(K((int)i),V((int)i));if(++pend>=128){wal.wait(last);pend=0;}}else{size_t i=read_id(rng,N,hot);uint64_t v=~0ULL;if(!db.get(ks[i],v)||v!=i)err++;}}if(pend)wal.wait(last);});for(auto&t:ts)t.join();});auto rr=recover("v35b.wal",(int)nw.load());err+=rr.bad+rr.missing+rr.dup+(rr.good!=nw.load());return{"BDR-v35b",hot?"hot80_1pct":"uniform",wp,OPS/s,err.load(),nw.load()};}

static M35 level35(size_t N,size_t OPS,int T,int wp,bool hot){std::filesystem::remove_all("v35b.level");leveldb::DB*d=nullptr;leveldb::Options o;o.create_if_missing=true;leveldb::DB::Open(o,"v35b.level",&d);leveldb::WriteOptions iw;for(size_t i=0;i<N;i+=2000){leveldb::WriteBatch b;for(size_t j=i;j<std::min(N,i+2000);++j)b.Put("K_"+std::to_string(j),std::to_string(j));d->Write(iw,&b);}leveldb::ReadOptions ro;leveldb::WriteOptions wo;wo.sync=true;std::atomic<size_t>next{0},nw{0};std::atomic<uint64_t>err{0};double s=sec35([&]{std::vector<std::thread>ts;for(int t=0;t<T;++t)ts.emplace_back([&,t]{uint64_t rng=0x1E353500ULL+t;leveldb::WriteBatch b;int bn=0;auto flush=[&]{if(!bn)return;if(!d->Write(wo,&b).ok())err+=bn;b.Clear();bn=0;};for(;;){size_t op=next.fetch_add(1);if(op>=OPS)break;if(is_write(rng,wp)){size_t i=nw.fetch_add(1);b.Put(K((int)i),V((int)i));if(++bn>=128)flush();}else{size_t i=read_id(rng,N,hot);std::string v;if(!d->Get(ro,"K_"+std::to_string(i),&v).ok()||v!=std::to_string(i))err++;}}flush();});for(auto&t:ts)t.join();});delete d;return{"LevelDB",hot?"hot80_1pct":"uniform",wp,OPS/s,err.load(),nw.load()};}

static M35 rocks35(size_t N,size_t OPS,int T,int wp,bool hot){std::filesystem::remove_all("v35b.rocks");rocksdb::DB*d=nullptr;rocksdb::Options o;o.create_if_missing=true;rocksdb::DB::Open(o,"v35b.rocks",&d);rocksdb::WriteOptions iw;for(size_t i=0;i<N;i+=2000){rocksdb::WriteBatch b;for(size_t j=i;j<std::min(N,i+2000);++j)b.Put("K_"+std::to_string(j),std::to_string(j));d->Write(iw,&b);}rocksdb::ReadOptions ro;rocksdb::WriteOptions wo;wo.sync=true;std::atomic<size_t>next{0},nw{0};std::atomic<uint64_t>err{0};double s=sec35([&]{std::vector<std::thread>ts;for(int t=0;t<T;++t)ts.emplace_back([&,t]{uint64_t rng=0xA0353500ULL+t;rocksdb::WriteBatch b;int bn=0;auto flush=[&]{if(!bn)return;if(!d->Write(wo,&b).ok())err+=bn;b.Clear();bn=0;};for(;;){size_t op=next.fetch_add(1);if(op>=OPS)break;if(is_write(rng,wp)){size_t i=nw.fetch_add(1);b.Put(K((int)i),V((int)i));if(++bn>=128)flush();}else{size_t i=read_id(rng,N,hot);std::string v;if(!d->Get(ro,"K_"+std::to_string(i),&v).ok()||v!=std::to_string(i))err++;}}flush();});for(auto&t:ts)t.join();});delete d;return{"RocksDB",hot?"hot80_1pct":"uniform",wp,OPS/s,err.load(),nw.load()};}

int main(int argc,char**argv){size_t N=argc>1?std::stoull(argv[1]):500000,OPS=argc>2?std::stoull(argv[2]):200000;int T=argc>3?std::stoi(argv[3]):8;std::cout<<"engine,profile,write_pct,threads,base_records,mixed_ops,throughput_ops_s,writes,errors\n";for(int wp:{10,50})for(bool hot:{false,true}){M35 rows[]={bdr35(N,OPS,T,wp,hot),level35(N,OPS,T,wp,hot),rocks35(N,OPS,T,wp,hot)};for(auto&r:rows){std::cout<<r.engine<<','<<r.profile<<','<<r.write_pct<<','<<T<<','<<N<<','<<OPS<<','<<r.ops_s<<','<<r.writes<<','<<r.errors<<"\n";if(r.errors)return 2;}}return 0;}
