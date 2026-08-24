#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>
using Clock=std::chrono::steady_clock;
static inline uint64_t mix64(uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);} 
#pragma pack(push,1)
struct F96{uint64_t lo;uint32_t hi;bool operator<(const F96&o)const{return hi<o.hi||(hi==o.hi&&lo<o.lo);}bool operator==(const F96&o)const{return lo==o.lo&&hi==o.hi;}};
#pragma pack(pop)
struct F128{uint64_t lo,hi;bool operator<(const F128&o)const{return hi<o.hi||(hi==o.hi&&lo<o.lo);}bool operator==(const F128&o)const{return lo==o.lo&&hi==o.hi;}};
template<class T,class Make> void run(const char*name,size_t N,size_t Q,Make make){std::vector<T>v;v.reserve(N);for(size_t i=0;i<N;++i)v.push_back(make(i));auto a=Clock::now();std::sort(v.begin(),v.end());auto b=Clock::now();size_t dup=0;for(size_t i=1;i<N;++i)dup+=v[i]==v[i-1];std::mt19937_64 r(12345);std::uniform_int_distribution<size_t>d(0,N-1);volatile size_t found=0;auto c=Clock::now();for(size_t i=0;i<Q;++i){T x=v[d(r)];found+=std::binary_search(v.begin(),v.end(),x);}auto e=Clock::now();std::cout<<name<<','<<sizeof(T)<<','<<dup<<','<<std::chrono::duration<double>(b-a).count()<<','<<Q/std::chrono::duration<double>(e-c).count()<<'\n';}
int main(int argc,char**argv){size_t N=argc>1?std::stoull(argv[1]):500000,Q=200000;std::cout<<"bits,bytes,observed_duplicates,sort_s,binary_lookup_ops_s\n";run<uint64_t>("64",N,Q,[](size_t i){return mix64(i);});run<F96>("96",N,Q,[](size_t i){uint64_t a=mix64(i),b=mix64(a);return F96{a,uint32_t(b)};});run<F128>("128",N,Q,[](size_t i){uint64_t a=mix64(i);return F128{a,mix64(a)};});}
