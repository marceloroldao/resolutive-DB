#define main v30_unused_main
#include "bdr_v30_ticketed_pipeline.cpp"
#undef main
#include <cstdlib>

struct CrashRec{size_t good=0;size_t bad=0;uint64_t last=0;bool tail_stop=false;};
static CrashRec recover_prefix(const char*path){CrashRec r;int fd=::open(path,O_RDONLY);if(fd<0)return r;off_t e=::lseek(fd,0,SEEK_END),p=0;uint64_t ex=1;while(p<e){H h{};if(e-p<(off_t)sizeof(h)||::pread(fd,&h,sizeof(h),p)!=(ssize_t)sizeof(h)){r.tail_stop=true;break;}if(h.kl==0&&h.vl==0&&h.seq==0){r.tail_stop=true;break;}if(!h.kl||h.kl>(1u<<20)||h.vl>(1u<<24)||h.seq!=ex){r.bad++;r.tail_stop=true;break;}off_t n=sizeof(h)+(off_t)h.kl+h.vl;if(e-p<n){r.tail_stop=true;break;}std::string k(h.kl,'\0'),v(h.vl,'\0');if(::pread(fd,k.data(),h.kl,p+sizeof(h))!=(ssize_t)h.kl||::pread(fd,v.data(),h.vl,p+sizeof(h)+h.kl)!=(ssize_t)h.vl){r.tail_stop=true;break;}if(ck(k,v)!=h.crc){r.bad++;r.tail_stop=true;break;}r.good++;r.last=h.seq;ex++;p+=n;}::close(fd);return r;}

static int writer(const char*path,int total,int writers,int window,int physical){std::filesystem::remove(path);TicketWal wal(path,(size_t)total*96+4096,physical);std::atomic<int>next{0};std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&]{uint64_t last=0;int pending=0;for(;;){int i=next.fetch_add(1);if(i>=total)break;last=wal.submit(K(i),V(i));if(++pending>=window){wal.wait(last);pending=0;}}if(pending)wal.wait(last);});for(auto&t:ts)t.join();return 0;}

int main(int argc,char**argv){if(argc<3)return 2;std::string mode=argv[1];const char*path=argv[2];if(mode=="writer"){int total=argc>3?std::stoi(argv[3]):2000000;int writers=argc>4?std::stoi(argv[4]):8;int window=argc>5?std::stoi(argv[5]):128;int physical=argc>6?std::stoi(argv[6]):512;return writer(path,total,writers,window,physical);}if(mode=="recover"){auto r=recover_prefix(path);std::cout<<r.good<<','<<r.last<<','<<r.bad<<','<<r.tail_stop<<'\n';return r.last==r.good?0:3;}return 4;}
