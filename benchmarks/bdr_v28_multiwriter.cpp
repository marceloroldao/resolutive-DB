#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <zlib.h>

using Clock=std::chrono::steady_clock;
static std::string K(int i){char b[32];std::snprintf(b,sizeof(b),"K%08d",i);return b;}
static std::string V(int i){char b[32];std::snprintf(b,sizeof(b),"V%08d",i);return b;}
static void add(std::vector<char>&b,const void*p,size_t n){auto q=(const char*)p;b.insert(b.end(),q,q+n);} 
static bool pwriteall(int fd,const char*p,size_t n,off_t off){while(n){ssize_t w=::pwrite(fd,p,n,off);if(w<=0)return false;p+=w;n-=w;off+=w;}return true;}
struct RecHdr{uint32_t kl,vl;uint64_t seq;uint32_t crc;};
static uint32_t crcKV(const std::string&k,const std::string&v){uLong c=crc32(0,(const Bytef*)k.data(),k.size());c=crc32(c,(const Bytef*)v.data(),v.size());return (uint32_t)c;}

class WAL28{
 int fd=-1; off_t off=0; uint64_t seq=0; std::vector<char>buf; std::mutex mu; size_t sync_every;
public:
 WAL28(const char*path,size_t reserve_bytes,size_t batch):sync_every(batch){fd=::open(path,O_CREAT|O_TRUNC|O_RDWR,0644);if(fd<0)throw std::runtime_error("open");if(::posix_fallocate(fd,0,reserve_bytes)!=0)throw std::runtime_error("fallocate");buf.reserve(1<<20);}~WAL28(){sync();if(fd>=0)::close(fd);} 
 void put(const std::string&k,const std::string&v){std::lock_guard<std::mutex>g(mu);RecHdr h{(uint32_t)k.size(),(uint32_t)v.size(),++seq,crcKV(k,v)};add(buf,&h,sizeof(h));add(buf,k.data(),k.size());add(buf,v.data(),v.size());if(seq%sync_every==0)sync_locked();}
 void sync(){std::lock_guard<std::mutex>g(mu);sync_locked();}
 uint64_t last_seq(){std::lock_guard<std::mutex>g(mu);return seq;}
private:
 void sync_locked(){if(buf.empty())return; if(!pwriteall(fd,buf.data(),buf.size(),off))throw std::runtime_error("write");off+=buf.size();buf.clear();if(::fdatasync(fd)!=0)throw std::runtime_error("fdatasync");}
};

struct Recovery{size_t good=0,bad=0;uint64_t last=0;bool truncated=false;std::unordered_map<std::string,std::string>kv;};
static Recovery recover(const char*path){Recovery r;int fd=::open(path,O_RDONLY);if(fd<0)throw std::runtime_error("open rec");off_t end=::lseek(fd,0,SEEK_END),p=0;uint64_t expected=1;while(p<end){RecHdr h{};if(end-p<(off_t)sizeof(h)){r.truncated=true;break;}if(::pread(fd,&h,sizeof(h),p)!=(ssize_t)sizeof(h)){r.truncated=true;break;}if(h.kl==0||h.kl>(1u<<20)||h.vl>(1u<<24)||h.seq!=expected){break;}off_t need=sizeof(h)+(off_t)h.kl+h.vl;if(end-p<need){r.truncated=true;break;}std::string k(h.kl,'\0'),v(h.vl,'\0');if(::pread(fd,k.data(),h.kl,p+sizeof(h))!=(ssize_t)h.kl||::pread(fd,v.data(),h.vl,p+sizeof(h)+h.kl)!=(ssize_t)h.vl){r.truncated=true;break;}if(crcKV(k,v)!=h.crc){r.bad++;break;}r.kv[k]=v;r.good++;r.last=h.seq;expected++;p+=need;}::close(fd);return r;}

static double pct(std::vector<double> v,double q){if(v.empty())return 0;std::sort(v.begin(),v.end());size_t i=std::min(v.size()-1,(size_t)(q*(v.size()-1)));return v[i];}
int main(int argc,char**argv){int total=argc>1?std::stoi(argv[1]):100000;int batch=argc>2?std::stoi(argv[2]):128;std::cout<<"writers,total,batch,throughput_ops_s,p50_us,p95_us,p99_us,max_us,recovered,last_seq,bad,missing,duplicates\n";
for(int writers: {1,2,4,8}){std::filesystem::remove("v28.wal");WAL28 wal("v28.wal",(size_t)total*96+4096,batch);std::atomic<int> next{0};std::vector<std::vector<double>> lats(writers);auto t0=Clock::now();std::vector<std::thread> th;for(int w=0;w<writers;++w)th.emplace_back([&,w]{for(;;){int i=next.fetch_add(1);if(i>=total)break;auto a=Clock::now();wal.put(K(i),V(i));auto b=Clock::now();lats[w].push_back(std::chrono::duration<double,std::micro>(b-a).count());}});for(auto&x:th)x.join();wal.sync();double sec=std::chrono::duration<double>(Clock::now()-t0).count();std::vector<double> all;for(auto&v:lats)all.insert(all.end(),v.begin(),v.end());auto r=recover("v28.wal");size_t missing=0;for(int i=0;i<total;++i){auto it=r.kv.find(K(i));if(it==r.kv.end()||it->second!=V(i))missing++;}size_t dup=r.good-r.kv.size();std::cout<<writers<<','<<total<<','<<batch<<','<<(total/sec)<<','<<pct(all,.50)<<','<<pct(all,.95)<<','<<pct(all,.99)<<','<<pct(all,1.0)<<','<<r.good<<','<<r.last<<','<<r.bad<<','<<missing<<','<<dup<<"\n";if(r.good!=(size_t)total||r.last!=(uint64_t)total||r.bad||missing||dup)return 2;}
}
