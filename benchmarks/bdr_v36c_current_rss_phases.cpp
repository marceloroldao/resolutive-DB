#define main v20_unused_main
#include "bdr_v20_density_target_engine.cpp"
#undef main
#define main v30_unused_main
#include "bdr_v30_ticketed_pipeline.cpp"
#undef main
#include <fstream>
#include <sstream>

static long current_rss_kb(){std::ifstream f("/proc/self/status");std::string line;while(std::getline(f,line)){if(line.rfind("VmRSS:",0)==0){std::istringstream s(line.substr(6));long kb=0;s>>kb;return kb;}}return -1;}
static void sample37(const char*phase){std::cout<<phase<<','<<current_rss_kb()<<"\n";}
int main(int argc,char**argv){size_t N=argc>1?std::stoull(argv[1]):1000000;uint32_t M=pow2ceil(std::max<uint64_t>(64,(N+255)/256));sample37("start");auto ks=keys("sequential",N,M);std::vector<A>as;as.reserve(N);for(auto&k:ks)as.push_back(enc(k,M));sample37("keys_and_addresses");LocalDB db(M,as,16);for(size_t i=0;i<N;++i)db.put(as[i],i);db.finish();sample37("index_built_with_inputs");ks.clear();ks.shrink_to_fit();as.clear();as.shrink_to_fit();sample37("index_steady_state");std::filesystem::remove("v36c.wal");{TicketWal wal("v36c.wal",N*96+4096,512);uint64_t last=0;for(size_t i=0;i<N;++i){last=wal.submit(K((int)i),V((int)i));if((i+1)%128==0)wal.wait(last);}if(last)wal.wait(last);sample37("index_plus_wal_pipeline");}sample37("index_after_wal_close");auto rr=recover("v36c.wal",(int)N);if(rr.good!=N||rr.bad||rr.missing||rr.dup)return 2;sample37("index_plus_recovery_map");std::cout<<"records,"<<N<<"\n";return 0;}
