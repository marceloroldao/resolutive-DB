#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;
static constexpr uint64_t SEED = 0xB0D2A019ULL;

static inline uint64_t mix64(uint64_t x){
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static inline uint64_t fnv1a(const std::string& s, uint64_t h=1469598103934665603ULL){
    for(unsigned char c: s){ h ^= c; h *= 1099511628211ULL; }
    return h;
}

struct Address { uint32_t rho, phi, theta; uint64_t fp; };
static inline Address enc(const std::string& k, uint32_t M){
    const uint64_t h0=mix64(fnv1a(k));
    const uint64_t h1=mix64(h0^0xD6E8FEB86659FD93ULL);
    const uint64_t h2=mix64(h1^0xA5A35625D3E7E5B9ULL);
    const uint64_t h3=mix64(h2^0x9E3779B97F4A7C15ULL);
    return {uint32_t(h0%M), uint32_t(h1&0xffffu), uint32_t(h2&0xffffu), mix64(h0^(h1<<1)^(h2>>1)^h3)};
}

struct Slot { uint64_t key=0,val=0; uint32_t dist=0; bool used=false; };
class RH {
    std::vector<Slot> s; size_t mask=0;
public:
    explicit RH(size_t n=1){ size_t c=8; while(c<std::max<size_t>(8,n*2)) c<<=1; s.resize(c); mask=c-1; }
    void put(uint64_t k,uint64_t v){ size_t i=mix64(k)&mask; Slot cur{k,v,0,true}; for(;;){ auto &x=s[i]; if(!x.used){x=cur;return;} if(x.key==k){x.val=v;return;} if(x.dist<cur.dist)std::swap(x,cur); i=(i+1)&mask; ++cur.dist; } }
    bool get(uint64_t k,uint64_t&v)const{ size_t i=mix64(k)&mask; uint32_t d=0; for(;;){ const auto &x=s[i]; if(!x.used||x.dist<d)return false; if(x.key==k){v=x.val;return true;} i=(i+1)&mask; ++d; } }
    size_t bytes()const{return s.capacity()*sizeof(Slot);} 
};

struct Compact {
    std::vector<std::pair<uint64_t,uint64_t>> a;
    explicit Compact(size_t n=0){a.reserve(n);} 
    void put(uint64_t k,uint64_t v){a.emplace_back(k,v);} 
    void finish(){std::sort(a.begin(),a.end(),[](auto&x,auto&y){return x.first<y.first;});}
    bool get(uint64_t k,uint64_t&v)const{auto it=std::lower_bound(a.begin(),a.end(),k,[](auto&x,uint64_t y){return x.first<y;}); if(it==a.end()||it->first!=k)return false; v=it->second; return true;}
    size_t bytes()const{return a.capacity()*sizeof(a[0]);}
};

enum class Kind:uint8_t{Empty,Compact,Robin,PhiRobin};
struct Part {
    Kind kind=Kind::Empty;
    std::unique_ptr<Compact> compact;
    std::unique_ptr<RH> robin;
    std::unordered_map<uint32_t,std::unique_ptr<RH>> phi;
    size_t occupancy=0;
};

class AdaptiveDB {
    uint32_t M; std::vector<Part> p;
    size_t compact_limit, split_limit;
public:
    AdaptiveDB(uint32_t m,const std::vector<Address>& as,size_t compact_lim=24,size_t split_lim=512):M(m),p(m),compact_limit(compact_lim),split_limit(split_lim){
        std::vector<size_t> cnt(M); for(auto&a:as)cnt[a.rho]++;
        for(uint32_t i=0;i<M;++i){ auto &x=p[i]; x.occupancy=cnt[i]; if(!cnt[i])continue; if(cnt[i]<=compact_limit){x.kind=Kind::Compact;x.compact=std::make_unique<Compact>(cnt[i]);} else if(cnt[i]<=split_limit){x.kind=Kind::Robin;x.robin=std::make_unique<RH>(cnt[i]);} else x.kind=Kind::PhiRobin; }
        if(split_limit){
            std::vector<std::unordered_map<uint32_t,size_t>> pc(M);
            for(auto&a:as) if(p[a.rho].kind==Kind::PhiRobin) pc[a.rho][a.phi]++;
            for(uint32_t r=0;r<M;++r) if(p[r].kind==Kind::PhiRobin){ p[r].phi.reserve(pc[r].size()*2+1); for(auto &kv:pc[r])p[r].phi.emplace(kv.first,std::make_unique<RH>(kv.second)); }
        }
    }
    void put(const Address&a,uint64_t v){auto &x=p[a.rho]; if(x.kind==Kind::Compact)x.compact->put(a.fp,v); else if(x.kind==Kind::Robin)x.robin->put(a.fp,v); else if(x.kind==Kind::PhiRobin)x.phi[a.phi]->put(a.fp,v);}
    void finish(){for(auto &x:p)if(x.kind==Kind::Compact)x.compact->finish();}
    bool get(const std::string&k,uint64_t&v)const{auto a=enc(k,M); auto &x=p[a.rho]; if(x.kind==Kind::Compact)return x.compact->get(a.fp,v); if(x.kind==Kind::Robin)return x.robin->get(a.fp,v); if(x.kind==Kind::PhiRobin){auto it=x.phi.find(a.phi);return it!=x.phi.end()&&it->second->get(a.fp,v);} return false;}
    size_t bytes()const{size_t b=p.capacity()*sizeof(Part); for(auto &x:p){if(x.compact)b+=x.compact->bytes();if(x.robin)b+=x.robin->bytes(); if(x.kind==Kind::PhiRobin){b+=x.phi.bucket_count()*sizeof(void*);for(auto&kv:x.phi)b+=sizeof(kv)+kv.second->bytes();}} return b;}
    void counts(size_t&c,size_t&r,size_t&s)const{c=r=s=0;for(auto &x:p){if(x.kind==Kind::Compact)c++;else if(x.kind==Kind::Robin)r++;else if(x.kind==Kind::PhiRobin)s++;}}
};

class PartitionedRH {
    uint32_t M; std::vector<std::unique_ptr<RH>> p;
public:
    PartitionedRH(uint32_t m,const std::vector<Address>&as):M(m),p(m){std::vector<size_t>c(M);for(auto&a:as)c[a.rho]++;for(uint32_t i=0;i<M;++i)if(c[i])p[i]=std::make_unique<RH>(c[i]);}
    void put(const Address&a,uint64_t v){p[a.rho]->put(a.fp,v);} 
    bool get(const std::string&k,uint64_t&v)const{auto a=enc(k,M);auto &x=p[a.rho];return x&&x->get(a.fp,v);} 
    size_t bytes()const{size_t b=p.capacity()*sizeof(void*);for(auto &x:p)if(x)b+=x->bytes();return b;}
};

struct Stat{double mean,p50,p95,p99,max,tput;};
template<class F> Stat bench(F&&f,const std::vector<std::string>&ks,const std::vector<size_t>&q){std::vector<double>l;l.reserve(q.size());volatile uint64_t sink=0;auto A=Clock::now();for(auto i:q){auto a=Clock::now();uint64_t v=0;f(ks[i],v);auto b=Clock::now();sink^=v;l.push_back(std::chrono::duration<double,std::micro>(b-a).count());}auto B=Clock::now();std::sort(l.begin(),l.end());auto at=[&](double p){return l[size_t(p*(l.size()-1))];};return{std::accumulate(l.begin(),l.end(),0.0)/l.size(),at(.5),at(.95),at(.99),l.back(),q.size()/std::chrono::duration<double>(B-A).count()};}

static std::vector<std::string> make_keys(const std::string&w,size_t N,uint32_t M){std::vector<std::string>o;o.reserve(N);if(w=="hotrho"){uint32_t hot=std::max(1u,M/32);uint64_t c=0;while(o.size()<N){auto k="H_"+std::to_string(c++);if(enc(k,M).rho<hot)o.push_back(k);}}else if(w=="prefix"){for(size_t i=0;i<N;++i)o.push_back("TENANT_SHARED_PREFIX_RECORD_"+std::to_string(i));}else{for(size_t i=0;i<N;++i)o.push_back("K_"+std::to_string(i));}return o;}

int main(int argc,char**argv){size_t N=argc>1?std::stoull(argv[1]):100000,Q=50000;uint32_t M=argc>2?std::stoul(argv[2]):2048;std::cout<<"workload,engine,mean_us,p50_us,p95_us,p99_us,max_us,throughput_ops_s,approx_bytes_per_record,compact_parts,robin_parts,split_parts\n";for(std::string w:{"sequential","prefix","hotrho"}){auto ks=make_keys(w,N,M);std::vector<Address>as;as.reserve(N);for(auto&k:ks)as.push_back(enc(k,M));std::mt19937_64 rng(SEED);std::uniform_int_distribution<size_t>d(0,N-1);std::vector<size_t>q(Q);for(auto&x:q)x=d(rng);
RH global(N);for(size_t i=0;i<N;++i)global.put(as[i].fp,i);auto sg=bench([&](auto&k,uint64_t&v){return global.get(enc(k,M).fp,v);},ks,q);std::cout<<w<<",global_robin_hood,"<<sg.mean<<','<<sg.p50<<','<<sg.p95<<','<<sg.p99<<','<<sg.max<<','<<sg.tput<<','<<double(global.bytes())/N<<",0,0,0\n";
PartitionedRH prh(M,as);for(size_t i=0;i<N;++i)prh.put(as[i],i);auto sp=bench([&](auto&k,uint64_t&v){return prh.get(k,v);},ks,q);std::cout<<w<<",rho_partitioned_robin_hood,"<<sp.mean<<','<<sp.p50<<','<<sp.p95<<','<<sp.p99<<','<<sp.max<<','<<sp.tput<<','<<double(prh.bytes())/N<<",0,0,0\n";
AdaptiveDB adb(M,as,24,512);for(size_t i=0;i<N;++i)adb.put(as[i],i);adb.finish();size_t c,r,s;adb.counts(c,r,s);auto sa=bench([&](auto&k,uint64_t&v){return adb.get(k,v);},ks,q);std::cout<<w<<",adaptive_compact_robin_phi,"<<sa.mean<<','<<sa.p50<<','<<sa.p95<<','<<sa.p99<<','<<sa.max<<','<<sa.tput<<','<<double(adb.bytes())/N<<','<<c<<','<<r<<','<<s<<'\n';
}}
