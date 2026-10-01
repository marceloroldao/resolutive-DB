// Isolated research harness. Stable source is linked unchanged.
#include "bdr/resolutive_index.hpp"
#include "bdr/compact_index.hpp"
#include <map>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/resource.h>
using U=uint64_t; using C=std::chrono::steady_clock;
U mix(U x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
U fnv(const std::string& k,U h=1469598103934665603ULL){for(unsigned char c:k){h^=c;h*=1099511628211ULL;}return h;}
struct A {U route,fp;};
A encode(const std::string&k,const std::string&w){U h=mix(fnv(k)),fp=mix(fnv(k,1099511628211ULL)^(h<<1));
 if(w=="rho")h&=~U(63); // all keys in one of 64 primary regions
 if(w=="rho_phi")h&=~U(4095); // first two 6-bit levels collapsed
 if(w=="poor") {h=0;fp&=255;} // shared poor address/hash, exact keys retained
 if(w=="fp8")fp&=255; return {h,fp};}
std::string key(U id,const std::string&w){return (w=="prefix"?"TENANT_SHARED_PREFIX_RECORD_":"K_")+std::to_string(id);}
struct Meter {U depth=0,probes=0,comparisons=0,fpfalse=0,pop=0;};
struct Slot {U fp=0,id=0;uint32_t dist=0;bool used=false;};
// V18/V20 Robin Hood algorithm, with growth and exact identity rather than fp-only identity.
struct RH {std::vector<Slot>s;size_t count=0;U put_collisions=0;
 RH(){s.resize(8);}void insert(Slot x){size_t i=mix(x.fp)&(s.size()-1);x.dist=0;for(size_t steps=0;steps<s.size();++steps){auto&v=s[i];if(!v.used){v=x;++count;return;}++put_collisions;if(v.fp==x.fp&&v.id==x.id){v=x;return;}if(v.dist<x.dist)std::swap(v,x);i=(i+1)&(s.size()-1);++x.dist;}throw std::runtime_error("overflow");}
 void put(U fp,U id){if((count+1)>s.size()*.78){auto old=std::move(s);s.resize(old.size()*2);count=0;for(auto x:old)if(x.used)insert(x);}insert({fp,id,0,true});}
 U get(U fp,U id,Meter* m=nullptr)const{size_t i=mix(fp)&(s.size()-1);uint32_t d=0;for(size_t z=0;z<s.size();++z){auto&v=s[i];if(m)++m->probes;if(!v.used||v.dist<d)return ~U(0);if(v.fp==fp){if(m)++m->comparisons;if(v.id==id)return id;if(m)++m->fpfalse;}i=(i+1)&(s.size()-1);++d;}return ~U(0);}
 size_t bytes()const{return sizeof(*this)+s.capacity()*sizeof(Slot);}};
struct Node {std::unique_ptr<RH> leaf=std::make_unique<RH>();std::vector<std::unique_ptr<Node>>child;size_t count=0;};
struct Tree {Node root;int fixed;size_t threshold;std::string work;Tree(int f,size_t t,std::string w):fixed(f),threshold(t),work(w){}
 void split(Node&n,int depth){auto old=std::move(n.leaf);n.child.resize(64);for(auto x:old->s)if(x.used){auto a=encode(key(x.id,work),work);put(n,a,x.id,depth);}}
 void put(Node&n,A a,U id,int depth){++n.count; if(n.child.empty()&&depth<3&&((fixed>depth)||(fixed==0&&n.leaf->count>=threshold))){--n.count;split(n,depth);++n.count;}
 if(n.child.empty())n.leaf->put(a.fp,id);else{size_t ix=(a.route>>(6*depth))&63;if(!n.child[ix])n.child[ix]=std::make_unique<Node>();put(*n.child[ix],a,id,depth+1);}}
 void put(A a,U id){put(root,a,id,0);}const RH* route(A a,Meter*m=nullptr)const{auto*n=&root;int d=0;while(!n->child.empty()){if(m)++m->depth;auto&c=n->child[(a.route>>(6*d++))&63];if(!c)return nullptr;n=c.get();}if(m)m->pop=n->leaf->count;return n->leaf.get();} U get(A a,U id,Meter*m=nullptr)const{auto*r=route(a,m);return r?r->get(a.fp,id,m):~U(0);}
 void stats(const Node&n,U&bytes,U&leaves,U&maximum,U&coll)const{bytes+=sizeof(Node)+n.child.capacity()*sizeof(void*);if(n.child.empty()){++leaves;maximum=std::max<U>(maximum,n.leaf->count);bytes+=n.leaf->bytes();coll+=n.leaf->put_collisions;}else for(auto&c:n.child)if(c)stats(*c,bytes,leaves,maximum,coll);}};
struct HKey{U fp,id;bool operator==(const HKey&o)const{return fp==o.fp&&id==o.id;}};
struct Hasher{size_t operator()(const HKey&k)const{return mix(k.fp);}};
volatile U sink=0;
size_t rss(){std::ifstream f("/proc/self/statm");size_t a,b;f>>a>>b;return b*4096;}
double secs(C::time_point t){return std::chrono::duration<double>(C::now()-t).count();}
int main(int argc,char**argv){if(argc!=6){std::cerr<<"usage: bench N workload engine seed Q\n";return 2;}size_t N=std::stoull(argv[1]),Q=std::stoull(argv[5]);std::string w=argv[2],eng=argv[3];U seed=std::stoull(argv[4]);if(!N||!Q)return 2;
 // Poor fingerprints require quadratic insertion; deliberately bounded failure probe.
 if((w=="poor"||w=="fp8")&&N>100000)return 3;
 std::unique_ptr<Tree> tree;std::unique_ptr<RH> rh;std::unique_ptr<bdr::ResolutiveIndex> stable;std::unique_ptr<bdr::CompactIndex> compact;std::map<std::string,std::string> ordered;
 std::unordered_map<HKey,U,Hasher> flat;
 if(eng=="fixed1")tree=std::make_unique<Tree>(1,256,w);else if(eng=="fixed2")tree=std::make_unique<Tree>(2,256,w);else if(eng=="fixed3")tree=std::make_unique<Tree>(3,256,w);else if(eng=="adaptive64")tree=std::make_unique<Tree>(0,64,w);else if(eng=="adaptive1024")tree=std::make_unique<Tree>(0,1024,w);else if(eng=="adaptive")tree=std::make_unique<Tree>(0,256,w);else if(eng=="robin")rh=std::make_unique<RH>();else if(eng=="stable") {if(w!="uniform"&&w!="prefix"&&w!="zipf")return 3;stable=std::make_unique<bdr::ResolutiveIndex>(4096,.78);}else if(eng=="compact"||eng=="atomic_map"){if(w!="uniform"&&w!="prefix"&&w!="zipf")return 3;if(eng=="compact")compact=std::make_unique<bdr::CompactIndex>(4096,.78);}else if(eng=="unordered")flat.reserve(N);else return 2;
 size_t before=rss();auto start=C::now();
 for(U i=0;i<N;++i){auto k=key(i,w);auto a=(stable||compact||eng=="atomic_map")?A{}:encode(k,w);if(tree)tree->put(a,i);else if(rh)rh->put(a.fp,i);else if(stable)stable->put(k,std::to_string(i));else if(compact)compact->put(k,std::to_string(i));else if(eng=="atomic_map")ordered[k]=std::to_string(i);else flat.emplace(HKey{a.fp,i},i);}
 double build=secs(start);size_t resident=rss();U bytes=0,leaves=1,maxpop=N,coll=0;
 if(tree){maxpop=0;leaves=0;tree->stats(tree->root,bytes,leaves,maxpop,coll);}else if(rh){bytes=rh->bytes();coll=rh->put_collisions;}else if(stable){auto st=stable->stats();leaves=st.partitions;maxpop=st.max_partition_records;} else if(compact){auto st=compact->stats();leaves=st.partitions;maxpop=st.max_partition_records;} // map/stable bytes deliberately unavailable
 std::mt19937_64 rng(seed);std::vector<U> queries(Q);std::uniform_real_distribution<double> u(0,1);for(auto&i:queries)i=w=="zipf"?std::min<U>(N-1,U(std::pow(u(rng),4)*N)):rng()%N;
 auto lookup=[&](U id,Meter*m,bool pre)->U{auto k=key(id,w);auto a=(stable||compact||eng=="atomic_map")?A{}:encode(k,w);(void)pre;if(tree)return tree->get(a,id,m);if(rh){if(m)m->pop=N;return rh->get(a.fp,id,m);}if(stable){auto v=stable->get(k);return v?std::stoull(*v):~U(0);}if(compact){auto v=compact->get(k);return v?std::stoull(*v):~U(0);}if(eng=="atomic_map"){auto it=ordered.find(k);return it==ordered.end()?~U(0):U(std::stoull(it->second));}auto it=flat.find({a.fp,id});return it==flat.end()?~U(0):it->second;};
 // Exact hit and miss oracle, including absent key IDs at N+id.
 for(auto id:queries){if(lookup(id,nullptr,false)!=id||lookup(N+id,nullptr,false)!=~U(0))throw std::runtime_error("fidelity failure");}
 std::vector<A> addresses;for(auto id:queries)addresses.push_back(encode(key(id,w),w));
 start=C::now();for(size_t j=0;j<Q;++j){U v;if(tree)v=tree->get(addresses[j],queries[j]);else if(rh)v=rh->get(addresses[j].fp,queries[j]);else if(stable||compact||eng=="atomic_map")v=lookup(queries[j],nullptr,false);else{auto it=flat.find({addresses[j].fp,queries[j]});v=it->second;}sink=v;}double local_ns=secs(start)*1e9/Q;
 start=C::now();for(auto id:queries)sink=lookup(id,nullptr,false);double throughput=Q/secs(start);
 start=C::now();for(auto id:queries)sink=lookup(N+id,nullptr,false);double miss_throughput=Q/secs(start);
 start=C::now();for(auto id:queries)sink=encode(key(id,w),w).fp;double address_ns=secs(start)*1e9/Q;
 double route_ns=0;if(tree){start=C::now();for(auto a:addresses)sink=reinterpret_cast<uintptr_t>(tree->route(a));route_ns=secs(start)*1e9/Q;}
 std::vector<double>lat;lat.reserve(Q);for(auto id:queries){start=C::now();sink=lookup(id,nullptr,false);lat.push_back(secs(start)*1e9);}std::sort(lat.begin(),lat.end());
 Meter total;for(auto id:queries){Meter m;lookup(id,&m,false);total.depth+=m.depth;total.probes+=m.probes;total.comparisons+=m.comparisons;total.fpfalse+=m.fpfalse;total.pop+=m.pop;}
 struct rusage usage;getrusage(RUSAGE_SELF,&usage);double cpu=usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;
 auto metric=[&](U x){return (tree||rh)?std::to_string(double(x)/Q):"NA";};
 std::cout<<N<<','<<w<<','<<eng<<','<<seed<<','<<Q<<','<<N/build<<','<<throughput<<','<<lat[Q/2]<<','<<lat[size_t(.95*(Q-1))]<<','<<lat[size_t(.99*(Q-1))]<<','<<((stable||compact||eng=="atomic_map")?"NA":std::to_string(local_ns))<<','<<metric(total.depth)<<','<<metric(total.probes)<<','<<metric(total.comparisons)<<','<<metric(total.fpfalse)<<','<<metric(total.pop)<<','<<leaves<<','<<maxpop<<','<<coll<<','<<(bytes?std::to_string(double(bytes)/N):"NA")<<','<<resident<<','<<(resident>before?resident-before:0)<<','<<usage.ru_maxrss*1024ULL<<','<<cpu<<",1,"<<miss_throughput<<','<<address_ns<<','<<(tree?std::to_string(route_ns):"NA")<<"\n";
 return 0;
}
