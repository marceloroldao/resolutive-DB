#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
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
static std::string K(int i){char b[32];std::snprintf(b,sizeof(b),"K%08d",i);return b;}static std::string V(int i){char b[32];std::snprintf(b,sizeof(b),"V%08d",i);return b;}
struct H{uint32_t kl,vl;uint64_t seq;uint32_t crc;};static uint32_t ck(const std::string&k,const std::string&v){uLong c=crc32(0,(const Bytef*)k.data(),k.size());c=crc32(c,(const Bytef*)v.data(),v.size());return (uint32_t)c;}static void add(std::vector<char>&b,const void*p,size_t n){auto q=(const char*)p;b.insert(b.end(),q,q+n);}static bool pw(int fd,const char*p,size_t n,off_t o){while(n){ssize_t w=::pwrite(fd,p,n,o);if(w<=0)return false;p+=w;n-=w;o+=w;}return true;}
struct Req{uint64_t ticket;std::string k,v;};
class MPSCWal{
 int fd=-1;off_t off=0;size_t batch;std::mutex mu;std::condition_variable cv,ack;std::deque<Req>q;uint64_t next=0,durable=0;bool stop=false;std::thread io;
public:MPSCWal(const char*path,size_t reserve_bytes,size_t b):batch(b){fd=::open(path,O_CREAT|O_TRUNC|O_RDWR,0644);if(fd<0)throw std::runtime_error("open");if(::posix_fallocate(fd,0,reserve_bytes)!=0)throw std::runtime_error("fallocate");io=std::thread([this]{loop();});}
~MPSCWal(){{std::lock_guard<std::mutex>g(mu);stop=true;}cv.notify_one();io.join();if(fd>=0)::close(fd);} 
void put(const std::string&k,const std::string&v){uint64_t t;{std::lock_guard<std::mutex>g(mu);t=++next;q.push_back({t,k,v});}cv.notify_one();std::unique_lock<std::mutex>lk(mu);ack.wait(lk,[&]{return durable>=t;});}
uint64_t durable_seq(){std::lock_guard<std::mutex>g(mu);return durable;}
private:void loop(){std::vector<Req> local;std::vector<char>buf;buf.reserve(1<<20);for(;;){local.clear();{std::unique_lock<std::mutex>lk(mu);cv.wait(lk,[&]{return stop||!q.empty();});if(stop&&q.empty())break;while(!q.empty()&&local.size()<batch){local.push_back(std::move(q.front()));q.pop_front();}}
for(auto&r:local){H h{(uint32_t)r.k.size(),(uint32_t)r.v.size(),r.ticket,ck(r.k,r.v)};add(buf,&h,sizeof(h));add(buf,r.k.data(),r.k.size());add(buf,r.v.data(),r.v.size());}if(!pw(fd,buf.data(),buf.size(),off)||::fdatasync(fd)!=0)std::abort();off+=buf.size();buf.clear();{std::lock_guard<std::mutex>g(mu);durable=local.back().ticket;}ack.notify_all();}}
};
struct R{size_t good=0,bad=0,missing=0,dup=0;uint64_t last=0;};static R recover(const char*path,int total){R r;int fd=::open(path,O_RDONLY);off_t e=::lseek(fd,0,SEEK_END),p=0;uint64_t ex=1;std::unordered_map<std::string,std::string>kv;while(p<e){H h{};if(e-p<(off_t)sizeof(h)||::pread(fd,&h,sizeof(h),p)!=(ssize_t)sizeof(h))break;if(!h.kl||h.kl>(1u<<20)||h.vl>(1u<<24)||h.seq!=ex)break;off_t n=sizeof(h)+(off_t)h.kl+h.vl;if(e-p<n)break;std::string k(h.kl,'\0'),v(h.vl,'\0');::pread(fd,k.data(),h.kl,p+sizeof(h));::pread(fd,v.data(),h.vl,p+sizeof(h)+h.kl);if(ck(k,v)!=h.crc){r.bad++;break;}kv[k]=v;r.good++;r.last=h.seq;ex++;p+=n;}::close(fd);for(int i=0;i<total;++i){auto it=kv.find(K(i));if(it==kv.end()||it->second!=V(i))r.missing++;}r.dup=r.good-kv.size();return r;}
static double pct(std::vector<double>v,double q){std::sort(v.begin(),v.end());return v[std::min(v.size()-1,(size_t)(q*(v.size()-1)))];}
int main(int argc,char**argv){int total=argc>1?std::stoi(argv[1]):100000;int batch=argc>2?std::stoi(argv[2]):128;std::cout<<"writers,total,batch,throughput_ops_s,p50_us,p95_us,p99_us,max_us,recovered,last_seq,bad,missing,duplicates\n";for(int writers:{1,2,4,8,16}){std::filesystem::remove("v29.wal");std::atomic<int>next{0};std::vector<std::vector<double>>lat(writers);auto t0=Clock::now();{MPSCWal w("v29.wal",(size_t)total*96+4096,batch);std::vector<std::thread>ts;for(int x=0;x<writers;++x)ts.emplace_back([&,x]{for(;;){int i=next.fetch_add(1);if(i>=total)break;auto a=Clock::now();w.put(K(i),V(i));auto b=Clock::now();lat[x].push_back(std::chrono::duration<double,std::micro>(b-a).count());}});for(auto&t:ts)t.join();}double sec=std::chrono::duration<double>(Clock::now()-t0).count();std::vector<double>a;for(auto&v:lat)a.insert(a.end(),v.begin(),v.end());auto r=recover("v29.wal",total);std::cout<<writers<<','<<total<<','<<batch<<','<<total/sec<<','<<pct(a,.5)<<','<<pct(a,.95)<<','<<pct(a,.99)<<','<<pct(a,1)<<','<<r.good<<','<<r.last<<','<<r.bad<<','<<r.missing<<','<<r.dup<<"\n";if(r.good!=(size_t)total||r.last!=(uint64_t)total||r.bad||r.missing||r.dup)return 2;}}
