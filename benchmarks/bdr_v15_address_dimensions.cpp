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
static inline uint64_t fnv(const std::string&s,uint64_t h=1469598103934665603ULL){for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
struct EncVals{uint32_t rho,phi,theta;uint64_t fp;};
static inline EncVals enc(const std::string&k,uint32_t M,uint32_t P,uint32_t T){
  uint64_t h1=mix64(fnv(k));
  uint64_t h2=mix64(h1^0xD6E8FEB86659FD93ULL);
  uint64_t h3=mix64(h2^0xA5A35625D3E7E5B9ULL);
  return {uint32_t(h1%M),uint32_t(h2%P),uint32_t(h3%T),mix64(h1^(h2<<1)^(h3>>1))};
}

template<int MODE> class PartDB{
  uint32_t M,P,T; std::vector<std::unordered_map<uint64_t,uint64_t>> parts;
  static uint64_t local(const EncVals&e){
    if constexpr(MODE==0) return e.fp;
    if constexpr(MODE==1) return mix64(((uint64_t)e.phi<<32)^e.fp);
    if constexpr(MODE==2) return mix64(((uint64_t)e.theta<<32)^e.fp);
    return mix64(((uint64_t)e.phi<<32)^((uint64_t)e.theta<<16)^e.fp);
  }
public:
  PartDB(uint32_t m,uint32_t p,uint32_t t):M(m),P(p),T(t),parts(m){}
  void build(const std::vector<std::string>&ks){
    std::vector<size_t> cnt(M); for(auto&k:ks) cnt[enc(k,M,P,T).rho]++;
    for(size_t i=0;i<M;++i) parts[i].reserve(cnt[i]*2+1);
    for(size_t i=0;i<ks.size();++i){auto e=enc(ks[i],M,P,T); parts[e.rho][local(e)]=i;}
  }
  bool get(const std::string&k,uint64_t&v)const{auto e=enc(k,M,P,T); auto it=parts[e.rho].find(local(e)); if(it==parts[e.rho].end())return false;v=it->second;return true;}
};

template<class DB> std::pair<double,double> bench(DB&db,const std::vector<std::string>&ks,size_t q){
 std::mt19937_64 r(12345); std::uniform_int_distribution<size_t>d(0,ks.size()-1); std::vector<double> lat;lat.reserve(q);volatile uint64_t sink=0;auto t0=Clock::now();
 for(size_t i=0;i<q;++i){auto a=Clock::now();uint64_t v=0;db.get(ks[d(r)],v);auto b=Clock::now();sink^=v;lat.push_back(std::chrono::duration<double,std::micro>(b-a).count());}
 auto t1=Clock::now();std::sort(lat.begin(),lat.end());double us=std::chrono::duration<double,std::micro>(t1-t0).count()/q;double p99=lat[(size_t)(0.99*(lat.size()-1))];return{us,p99};
}
int main(int argc,char**argv){size_t N=argc>1?std::stoull(argv[1]):1000000;size_t Q=200000;uint32_t M=10000,P=65536,T=65536;std::vector<std::string>ks;ks.reserve(N);for(size_t i=0;i<N;++i){char b[48];std::snprintf(b,sizeof(b),"K_%010zu",i);ks.emplace_back(b);}std::cout<<"mode,build_s,lookup_us,p99_us,mops\n";
 auto run=[&]<int MODE>(const char*name){PartDB<MODE> db(M,P,T);auto a=Clock::now();db.build(ks);auto b=Clock::now();auto [us,p99]=bench(db,ks,Q);std::cout<<name<<','<<std::chrono::duration<double>(b-a).count()<<','<<us<<','<<p99<<','<<(1.0/us)<<"\n";};
 run.template operator()<0>("rho+fp");run.template operator()<1>("rho+phi+fp");run.template operator()<2>("rho+theta+fp");run.template operator()<3>("rho+phi+theta+fp");
}
