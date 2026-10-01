// Population audit outside timed benchmarks. Reuse exactly the common encoder.
#define main benchmark_main
#include "bench.cpp"
#undef main
int main(){std::cout<<"N,workload,unique_fingerprints,duplicate_fingerprints,max_fingerprint_multiplicity,max_depth1_population,max_depth2_population,max_depth3_population\n";
 for(size_t n:{100000ul,1000000ul,10000000ul})for(std::string w:{"uniform","prefix","rho","rho_phi","fp8","poor"}){
 std::vector<U> fp;fp.reserve(n);std::vector<U> count(262144);
 for(U i=0;i<n;++i){auto a=encode(key(i,w),w);fp.push_back(a.fp);++count[a.route&262143];}
 std::sort(fp.begin(),fp.end());U unique=0,maxgroup=0,group=0,prev=0;for(U x:fp){if(!group||x!=prev){++unique;group=1;}else ++group;maxgroup=std::max(maxgroup,group);prev=x;}
 std::vector<U> p1(64),p2(4096);for(size_t i=0;i<count.size();++i){p1[i&63]+=count[i];p2[i&4095]+=count[i];}
 std::cout<<n<<','<<w<<','<<unique<<','<<n-unique<<','<<maxgroup<<','<<*std::max_element(p1.begin(),p1.end())<<','<<*std::max_element(p2.begin(),p2.end())<<','<<*std::max_element(count.begin(),count.end())<<'\n';}
}
