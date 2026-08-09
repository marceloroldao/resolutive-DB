// BDR V5 compact-memory benchmark
// Goal: retain rho_R partitioning while reducing V4's eager per-partition reservation.
// Reproducible standalone C++17 benchmark.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using Clock=std::chrono::steady_clock;
static inline uint64_t mix64(uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
static inline uint64_t fnv(const std::string&s,uint64_t h){for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
struct A{uint32_t rho,phi;uint64_t sig;};
struct Enc{uint32_t m,p;A operator()(const std::string&k)const{uint64_t a=mix64(fnv(k,1469598103934665603ULL)),b=mix64(fnv(k,1099511628211ULL));return{uint32_t(a%m),uint32_t(b%p),mix64(a^(b<<1))};}};
struct E{uint64_t key,val;};
class CompactBDR{
 Enc enc; std::vector<std::vector<E>> parts;
 static uint64_t lk(const A&a){return mix64((uint64_t(a.phi)<<48)^a.sig);}
public:
 CompactBDR(uint32_t m,uint32_t p):enc{m,p},parts(m){}
 void build(const std::vector<std::string>&ks){
   std::vector<uint32_t> counts(parts.size()); for(auto&k:ks) counts[enc(k).rho]++;
   for(size_t i=0;i<parts.size();++i) parts[i].reserve(counts[i]);
   for(size_t i=0;i<ks.size();++i){A a=enc(ks[i]);parts[a.rho].push_back({lk(a),i});}
   for(auto&v:parts) std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.key<b.key;});
 }
 bool get(const std::string&k,uint64_t&v)const{A a=enc(k);uint64_t x=lk(a);auto&b=parts[a.rho];auto it=std::lower_bound(b.begin(),b.end(),x,[](const E&e,uint64_t q){return e.key<q;});if(it==b.end()||it->key!=x)return false;v=it->val;return true;}
 size_t bytes()const{size_t z=parts.capacity()*sizeof(std::vector<E>);for(auto&v:parts)z+=v.capacity()*sizeof(E);return z;}
};
class Flat{Enc enc;std::unordered_map<uint64_t,uint64_t> h;public:Flat(uint32_t m,uint32_t p,size_t n):enc{m,p}{h.reserve(n*2);}uint64_t fk(const std::string&k)const{A a=enc(k);return mix64((uint64_t(a.rho)<<32)^(uint64_t(a.phi)<<16)^a.sig);}void build(const std::vector<std::string>&ks){for(size_t i=0;i<ks.size();++i)h[fk(ks[i])]=i;}bool get(const std::string&k,uint64_t&v)const{auto it=h.find(fk(k));if(it==h.end())return false;v=it->second;return true;}size_t bytes()const{return h.bucket_count()*sizeof(void*)+h.size()*(sizeof(std::pair<const uint64_t,uint64_t>)+2*sizeof(void*));}};
template<class D>double bench(D&d,const std::vector<std::string>&ks,size_t q){std::mt19937_64 r(42);std::uniform_int_distribution<size_t>pick(0,ks.size()-1);volatile uint64_t sink=0;auto t=Clock::now();for(size_t i=0;i<q;++i){uint64_t v=0;d.get(ks[pick(r)],v);sink^=v;}return std::chrono::duration<double,std::micro>(Clock::now()-t).count()/q;}
int main(){constexpr size_t N=300000,Q=200000;constexpr uint32_t M=10000,P=65536;std::vector<std::string>ks;ks.reserve(N);for(size_t i=0;i<N;++i){char b[48];std::snprintf(b,sizeof(b),"ETBRA_RESOLUTIVE_KEY_%08zu",i);ks.emplace_back(b);}CompactBDR b(M,P);Flat f(M,P,N);auto t0=Clock::now();b.build(ks);auto t1=Clock::now();f.build(ks);auto t2=Clock::now();std::cout<<"engine,build_s,lookup_us,bytes,bytes_per_record\n";std::cout<<"BDRCompact,"<<std::chrono::duration<double>(t1-t0).count()<<','<<bench(b,ks,Q)<<','<<b.bytes()<<','<<double(b.bytes())/N<<'\n';std::cout<<"FlatHash,"<<std::chrono::duration<double>(t2-t1).count()<<','<<bench(f,ks,Q)<<','<<f.bytes()<<','<<double(f.bytes())/N<<'\n';}
