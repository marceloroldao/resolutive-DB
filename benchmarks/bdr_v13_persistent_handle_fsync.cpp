#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#pragma pack(push,1)
struct RecHdr { uint32_t magic, klen, vlen; uint64_t csum; };
#pragma pack(pop)

static uint64_t fnv1a(const void* data,size_t n,uint64_t h=1469598103934665603ULL){
  const auto* p=static_cast<const unsigned char*>(data);
  for(size_t i=0;i<n;++i){h^=p[i];h*=1099511628211ULL;} return h;
}
static uint64_t checksum(const std::string&k,const std::string&v){
  uint64_t h=fnv1a(k.data(),k.size()); return fnv1a(v.data(),v.size(),h);
}
static bool write_all(int fd,const void*buf,size_t n){
  const char* p=static_cast<const char*>(buf);
  while(n){ssize_t w=::write(fd,p,n); if(w<=0)return false; p+=w; n-=size_t(w);} return true;
}
static double run(bool durable,int n,const char* path){
  ::unlink(path); int fd=::open(path,O_CREAT|O_WRONLY|O_APPEND,0644); if(fd<0)return -1;
  auto t0=std::chrono::steady_clock::now();
  for(int i=0;i<n;++i){
    std::string k="K"+std::to_string(i),v="V"+std::to_string(i);
    RecHdr h{0x31445242u,uint32_t(k.size()),uint32_t(v.size()),checksum(k,v)};
    if(!write_all(fd,&h,sizeof(h))||!write_all(fd,k.data(),k.size())||!write_all(fd,v.data(),v.size())) return -1;
    if(durable && ::fdatasync(fd)!=0) return -1;
  }
  auto t1=std::chrono::steady_clock::now(); ::close(fd);
  return std::chrono::duration<double,std::micro>(t1-t0).count()/n;
}
int main(){
  constexpr int N=2000, R=7; std::vector<double> buffered,durable;
  for(int r=0;r<R;++r){
    buffered.push_back(run(false,N,"bdr_v13_buffered.wal"));
    durable.push_back(run(true,N,"bdr_v13_durable.wal"));
  }
  std::sort(buffered.begin(),buffered.end()); std::sort(durable.begin(),durable.end());
  std::cout<<"buffered_median_us="<<buffered[R/2]<<"\n";
  std::cout<<"fdatasync_median_us="<<durable[R/2]<<"\n";
  for(int i=0;i<R;++i) std::cout<<"rep"<<i<<",buffered="<<buffered[i]<<",durable="<<durable[i]<<"\n";
}
