#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <zlib.h>

struct H{uint32_t kl,vl;uint64_t seq;uint32_t crc;};
static std::string K(uint64_t i){char b[32];std::snprintf(b,sizeof(b),"K%010llu",(unsigned long long)i);return b;}
static std::string V(uint64_t i){char b[32];std::snprintf(b,sizeof(b),"V%010llu",(unsigned long long)i);return b;}
static uint32_t crcKV(const std::string&k,const std::string&v){uLong c=crc32(0,(const Bytef*)k.data(),k.size());c=crc32(c,(const Bytef*)v.data(),v.size());return(uint32_t)c;}
static void add(std::vector<char>&b,const void*p,size_t n){auto*q=(const char*)p;b.insert(b.end(),q,q+n);}static bool pwa(int fd,const char*p,size_t n,off_t o){while(n){ssize_t w=pwrite(fd,p,n,o);if(w<=0)return false;p+=w;n-=w;o+=w;}return true;}
static bool allzero(const H&h){const unsigned char*p=(const unsigned char*)&h;for(size_t i=0;i<sizeof(h);++i)if(p[i])return false;return true;}
struct R{uint64_t good=0,last=0,bad=0;bool partial=false;};
static R recover(const char*path){R r;int fd=open(path,O_RDONLY);off_t end=lseek(fd,0,SEEK_END),p=0;uint64_t exp=1;while(p+(off_t)sizeof(H)<=end){H h{};if(pread(fd,&h,sizeof(h),p)!=(ssize_t)sizeof(h))break;if(allzero(h))break;if(h.kl==0||h.kl>1024||h.vl>4096||h.seq!=exp){r.bad++;break;}off_t need=sizeof(h)+h.kl+h.vl;if(p+need>end){r.partial=true;break;}std::string k(h.kl,'\0'),v(h.vl,'\0');if(pread(fd,k.data(),h.kl,p+sizeof(h))!=(ssize_t)h.kl||pread(fd,v.data(),h.vl,p+sizeof(h)+h.kl)!=(ssize_t)h.vl){r.partial=true;break;}if(crcKV(k,v)!=h.crc||k!=K(h.seq-1)||v!=V(h.seq-1)){r.bad++;break;}r.good++;r.last=h.seq;exp++;p+=need;}close(fd);return r;}
static void writer(const char*path,uint64_t N,int batch){int fd=open(path,O_CREAT|O_TRUNC|O_RDWR,0644);posix_fallocate(fd,0,N*64+4096);std::vector<char>b;b.reserve(1<<20);off_t off=0;for(uint64_t i=0;i<N;++i){auto k=K(i),v=V(i);H h{(uint32_t)k.size(),(uint32_t)v.size(),i+1,crcKV(k,v)};add(b,&h,sizeof(h));add(b,k.data(),k.size());add(b,v.data(),v.size());if((i+1)%batch==0){pwa(fd,b.data(),b.size(),off);off+=b.size();b.clear();fdatasync(fd);}}if(!b.empty()){pwa(fd,b.data(),b.size(),off);fdatasync(fd);}close(fd);_exit(0);}
int main(){std::cout<<"delay_ms,exit_signal,good,last_seq,bad,partial,valid\n";bool ok=true;for(int delay:{2,5,10,20}){std::string path="v27_"+std::to_string(delay)+".wal";std::filesystem::remove(path);pid_t pid=fork();if(pid==0)writer(path.c_str(),1000000,128);usleep(delay*1000);kill(pid,SIGKILL);int st=0;waitpid(pid,&st,0);R r=recover(path.c_str());bool valid=(r.good==r.last&&r.bad<=1);ok&=valid;std::cout<<delay<<','<<(WIFSIGNALED(st)?WTERMSIG(st):0)<<','<<r.good<<','<<r.last<<','<<r.bad<<','<<r.partial<<','<<valid<<"\n";}return ok?0:2;}
