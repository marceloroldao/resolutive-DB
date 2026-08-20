#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <zlib.h>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static uint32_t crc32b(const void*p,size_t n,uint32_t s=0){return uint32_t(crc32(s,(const Bytef*)p,uInt(n)));}
static void u32(std::vector<uint8_t>&b,uint32_t v){for(int i=3;i>=0;--i)b.push_back(uint8_t(v>>(8*i)));}static void u64(std::vector<uint8_t>&b,uint64_t v){for(int i=7;i>=0;--i)b.push_back(uint8_t(v>>(8*i)));}
static void writeall(int fd,const uint8_t*p,size_t n){while(n){ssize_t w=::write(fd,p,n);if(w<=0)throw std::runtime_error("write");p+=w;n-=size_t(w);}}
static std::vector<uint8_t> rec2(uint64_t seq,const std::string&k,const std::string&v){std::vector<uint8_t>b;u64(b,seq);b.push_back(1);u32(b,k.size());u32(b,v.size());b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());u32(b,crc32b(b.data(),b.size()));return b;}
static std::vector<uint8_t> rec3(uint64_t seq,const std::string&k,const std::string&v){std::vector<uint8_t>h;u64(h,seq);h.push_back(1);u32(h,k.size());u32(h,v.size());uint32_t hc=crc32b(h.data(),h.size());uint32_t total=4+uint32_t(h.size())+4+uint32_t(k.size()+v.size())+4;std::vector<uint8_t>b;u32(b,total);b.insert(b.end(),h.begin(),h.end());u32(b,hc);b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());u32(b,crc32b(b.data(),b.size()));return b;}
static double run(bool v3,int batch,int N){std::string p=v3?"v47_3.wal":"v47_2.wal";int fd=::open(p.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("open");std::vector<uint8_t>buf;buf.reserve(size_t(batch)*96);auto t0=Clock::now();for(int i=0;i<N;++i){std::string k="K"+std::to_string(i),v="VALUE_"+std::to_string(i);auto r=v3?rec3(i+1,k,v):rec2(i+1,k,v);buf.insert(buf.end(),r.begin(),r.end());if(int((i+1)%batch)==0){writeall(fd,buf.data(),buf.size());buf.clear();if(::fdatasync(fd))throw std::runtime_error("sync");}}if(!buf.empty()){writeall(fd,buf.data(),buf.size());if(::fdatasync(fd))throw std::runtime_error("sync");}auto t1=Clock::now();::close(fd);return N/std::chrono::duration<double>(t1-t0).count();}
int main(){int N=100000;std::cout<<"batch,bdw2_ops_s,bdw3_ops_s,bdw3_over_bdw2\n";for(int b:{1,32,128,512}){double a=run(false,b,N),c=run(true,b,N);std::cout<<b<<','<<a<<','<<c<<','<<(c/a)<<"\n";}return 0;}
