#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
using Clock=std::chrono::steady_clock;
static inline uint64_t mix64(uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);} 
static inline uint64_t fnv1a64(const std::string&s,uint64_t seed=1469598103934665603ULL){uint64_t h=seed;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
struct Address{uint32_t rho,phi;uint64_t sig;};
struct Encoder{uint32_t M,P;Address encode(const std::string&k)const{uint64_t h1=mix64(fnv1a64(k,1469598103934665603ULL));uint64_t h2=mix64(fnv1a64(k,1099511628211ULL));uint64_t h3=mix64(h1^(h2<<1));return{uint32_t(h1%M),uint32_t(h2%P),h3};}};
static inline uint64_t flatkey(const Address&a){return mix64((uint64_t(a.rho)<<32)^(uint64_t(a.phi)<<16)^a.sig);} 
struct Shard{std::unordered_map<uint64_t,uint64_t> map;mutable std::shared_mutex mu;};
enum class Mode{Generic,Stripe,Band};
struct ShardedDB{Encoder enc;std::vector<Shard> shards;size_t S;Mode mode;ShardedDB(uint32_t M,uint32_t P,size_t n,size_t s,Mode m):enc{M,P},shards(s),S(s),mode(m){for(auto&x:shards)x.map.reserve(n/s*2+64);}size_t shard_of(const Address&a)const{if(mode==Mode::Generic)return mix64(flatkey(a))%S;if(mode==Mode::Stripe)return a.phi%S;return std::min<size_t>(S-1,(uint64_t(a.phi)*S)/enc.P);}void insert(const std::string&k,uint64_t v){auto a=enc.encode(k);auto&sh=shards[shard_of(a)];std::unique_lock lk(sh.mu);sh.map[flatkey(a)]=v;}bool get(const std::string&k,uint64_t&v)const{auto a=enc.encode(k);auto&sh=shards[shard_of(a)];std::shared_lock lk(sh.mu);auto it=sh.map.find(flatkey(a));if(it==sh.map.end())return false;v=it->second;return true;}};
struct GlobalDB{Encoder enc;std::unordered_map<uint64_t,uint64_t> map;mutable std::shared_mutex mu;GlobalDB(uint32_t M,uint32_t P,size_t n):enc{M,P}{map.reserve(n*2);}void insert(const std::string&k,uint64_t v){auto a=enc.encode(k);std::unique_lock lk(mu);map[flatkey(a)]=v;}bool get(const std::string&k,uint64_t&v)const{auto a=enc.encode(k);std::shared_lock lk(mu);auto it=map.find(flatkey(a));if(it==map.end())return false;v=it->second;return true;}};
static double median(std::vector<double> v){std::sort(v.begin(),v.end());return v[v.size()/2];}
template<class DB,class Router> double run(DB&db,const std::vector<std::string>&keys,int T,size_t ops,bool mixed,Router route){Encoder enc{10000,65536};std::vector<std::vector<size_t>> pools(T);for(size_t i=0;i<keys.size();++i){auto a=enc.encode(keys[i]);pools[route(a)%T].push_back(i);}for(int t=0;t<T;++t)if(pools[t].empty())pools[t].push_back(t%keys.size());std::atomic<uint64_t>sink{0};auto t0=Clock::now();std::vector<std::thread>ws;for(int t=0;t<T;++t)ws.emplace_back([&,t]{std::mt19937_64 rng(1234+t);uint64_t v=0;auto&p=pools[t];for(size_t j=0;j<ops;++j){size_t idx=p[rng()%p.size()];if(mixed&&(rng()%10==0))db.insert(keys[idx],idx+j+1);else if(db.get(keys[idx],v))sink.fetch_xor(v,std::memory_order_relaxed);}});for(auto&th:ws)th.join();double sec=std::chrono::duration<double>(Clock::now()-t0).count();return double(T)*ops/sec/1e6;}
int main(){const size_t N=300000,OPS=50000;const uint32_t M=10000,P=65536;std::vector<std::string>keys;keys.reserve(N);char b[64];for(size_t i=0;i<N;++i){std::snprintf(b,sizeof(b),"ETBRA_RESOLUTIVE_KEY_%08zu",i);keys.emplace_back(b);}std::cout<<"threads,workload,global,generic,phase_stripe,phase_band\n";for(int T:{1,2,4,5}){for(bool mixed:{false,true}){std::vector<double>G,S,PS,PB;for(int r=0;r<3;++r){GlobalDB g(M,P,N);ShardedDB s(M,P,N,T,Mode::Generic),ps(M,P,N,T,Mode::Stripe),pb(M,P,N,T,Mode::Band);for(size_t i=0;i<N;++i){g.insert(keys[i],i);s.insert(keys[i],i);ps.insert(keys[i],i);pb.insert(keys[i],i);}auto rg=[](const Address&a){return mix64(flatkey(a));};auto rs=rg;auto rps=[](const Address&a){return uint64_t(a.phi);};auto rpb=[T,P](const Address&a){return std::min<uint64_t>(T-1,(uint64_t(a.phi)*T)/P);};G.push_back(run(g,keys,T,OPS,mixed,rg));S.push_back(run(s,keys,T,OPS,mixed,rs));PS.push_back(run(ps,keys,T,OPS,mixed,rps));PB.push_back(run(pb,keys,T,OPS,mixed,rpb));}std::cout<<T<<','<<(mixed?"mixed90r10w":"read")<<','<<median(G)<<','<<median(S)<<','<<median(PS)<<','<<median(PB)<<"\n";}}}
