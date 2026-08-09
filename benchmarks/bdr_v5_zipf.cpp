// BDR V5 compact-memory high-density and Zipf benchmark.
#include <algorithm>
#include <chrono>
#include <cmath>
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
struct A{uint32_t rho,phi;uint64_t sig;}; struct Enc{uint32_t m,p;A operator()(const std::string&k)const{uint64_t a=mix64(fnv(k,1469598103934665603ULL)),b=mix64(fnv(k,1099511628211ULL));return{uint32_t(a%m),uint32_t(b%p),mix64(a^(b<<1))};}};
struct E{uint64_t key,val;};
class BDR{Enc enc;std::vector<std::vector<E>>parts;static uint64_t lk(const A&a){return mix64((uint64_t(a.phi)<<48)^a.sig);}public:BDR(uint32_t m,uint32_t p):enc{m,p},parts(m){}void build(const std::vector<std::string>&ks){std::vector<uint32_t>c(parts.size());for(auto&k:ks)c[enc(k).rho]++;for(size_t i=0;i<parts.size();++i)parts[i].reserve(c[i]);for(size_t i=0;i<ks.size();++i){A a=enc(ks[i]);parts[a.rho].push_back({lk(a),i});}for(auto&v:parts)std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.key<b.key;});}bool get(const std::string&k,uint64_t&v)const{A a=enc(k);uint64_t x=lk(a);auto&b=parts[a.rho];auto it=std::lower_bound(b.begin(),b.end(),x,[](const E&e,uint64_t q){return e.key<q;});if(it==b.end()||it->key!=x)return false;v=it->val;return true;}size_t bytes()const{size_t z=parts.capacity()*sizeof(std::vector<E>);for(auto&v:parts)z+=v.capacity()*sizeof(E);return z;}};
class Flat{Enc enc;std::unordered_map<uint64_t,uint64_t>h;public:Flat(uint32_t m,uint32_t p,size_t n):enc{m,p}{h.reserve(n*2);}uint64_t fk(const std::string&k)const{A a=enc(k);return mix64((uint64_t(a.rho)<<32)^(uint64_t(a.phi)<<16)^a.sig);}void build(const std::vector<std::string>&ks){for(size_t i=0;i<ks.size();++i)h[fk(ks[i])]=i;}bool get(const std::string&k,uint64_t&v)const{auto it=h.find(fk(k));if(it==h.end())return false;v=it->second;return true;}size_t bytes()const{return h.bucket_count()*sizeof(void*)+h.size()*(sizeof(std::pair<const uint64_t,uint64_t>)+2*sizeof(void*));}};
static double pct(std::vector<double>v,double p){std::sort(v.begin(),v.end());size_t i=(size_t)(p*v.size());if(i>=v.size())i=v.size()-1;return v[i];}
template<class DB>void bench(const char*name,DB&db,const std::vector<std::string>&ks,bool zipf){std::mt19937_64 r(42);std::uniform_real_distribution<double>u(0,1);std::uniform_int_distribution<size_t>uni(0,ks.size()-1);std::vector<double>lat;lat.reserve(50000);volatile uint64_t sink=0;auto s=Clock::now();for(int i=0;i<200000;i++){size_t idx;if(zipf){double x=u(r);idx=(size_t)(ks.size()*std::pow(x,4.0));if(idx>=ks.size())idx=ks.size()-1;}else idx=uni(r);auto t0=Clock::now();uint64_t v=0;db.get(ks[idx],v);auto t1=Clock::now();if(i<50000)lat.push_back(std::chrono::duration<double,std::nano>(t1-t0).count()/1000.0);sink^=v;}auto e=Clock::now();double mops=200000/std::chrono::duration<double>(e-s).count()/1e6;std::cout<<name<<','<<(zipf?"zipf":"uniform")<<','<<mops<<','<<pct(lat,.5)<<','<<pct(lat,.99)<<'\n';}
int main(){constexpr size_t N=1000000;constexpr uint32_t M=10000,P=65536;std::vector<std::string>ks;ks.reserve(N);for(size_t i=0;i<N;++i){char b[48];std::snprintf(b,sizeof(b),"ETBRA_RESOLUTIVE_KEY_%08zu",i);ks.emplace_back(b);}BDR b(M,P);Flat f(M,P,N);b.build(ks);f.build(ks);std::cout<<"engine,dist,mops,p50_us,p99_us\n";bench("BDRCompact",b,ks,false);bench("FlatHash",f,ks,false);bench("BDRCompact",b,ks,true);bench("FlatHash",f,ks,true);std::cerr<<"bdr_bytes_per_record="<<double(b.bytes())/N<<"\n";std::cerr<<"flat_bytes_per_record="<<double(f.bytes())/N<<"\n";}
