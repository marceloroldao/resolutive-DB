#define main v20_unused_main
#include "bdr_v20_density_target_engine.cpp"
#undef main
#define main v30_unused_main
#include "bdr_v30_ticketed_pipeline.cpp"
#undef main
#include <fstream>
#include <sys/resource.h>

static long rss_kb(){struct rusage r{};getrusage(RUSAGE_SELF,&r);return r.ru_maxrss;}
static void sample(const char*phase){std::cout<<phase<<','<<rss_kb()<<"\n";}
int main(int argc,char**argv){size_t N=argc>1?std::stoull(argv[1]):1000000;uint32_t M=pow2ceil(std::max<uint64_t>(64,(N+255)/256));auto ks=keys("sequential",N,M);std::vector<A>as;as.reserve(N);for(auto&k:ks)as.push_back(enc(k,M));sample("keys_and_addresses");LocalDB db(M,as,16);for(size_t i=0;i<N;++i)db.put(as[i],i);db.finish();sample("index_built");ks.clear();ks.shrink_to_fit();as.clear();as.shrink_to_fit();sample("index_after_build_inputs_released");std::filesystem::remove("v36b.wal");{TicketWal wal("v36b.wal",N*96+4096,512);uint64_t last=0;for(size_t i=0;i<N;++i){last=wal.submit(K((int)i),V((int)i));if((i+1)%128==0)wal.wait(last);}if(last)wal.wait(last);sample("index_plus_wal_pipeline");}sample("index_after_wal_close");auto rr=recover("v36b.wal",(int)N);if(rr.good!=N||rr.bad||rr.missing||rr.dup)return 2;sample("index_plus_recovery_map");std::cout<<"records,"<<N<<"\n";return 0;}
