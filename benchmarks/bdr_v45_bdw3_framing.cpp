#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>
namespace fs=std::filesystem;
static uint32_t crc(const void*p,size_t n,uint32_t seed=0){return uint32_t(crc32(seed,(const Bytef*)p,uInt(n)));}
static void put32(std::vector<uint8_t>&b,uint32_t v){for(int i=3;i>=0;--i)b.push_back(uint8_t(v>>(8*i)));}static void put64(std::vector<uint8_t>&b,uint64_t v){for(int i=7;i>=0;--i)b.push_back(uint8_t(v>>(8*i)));}static uint32_t be32(const uint8_t*p){return(uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}static uint64_t be64(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|p[i];return v;}
#pragma pack(push,1)
struct W3H{char magic[4];uint16_t version;uint16_t hsize;uint32_t flags;uint64_t seg;uint64_t first_seq;uint32_t crc;};
#pragma pack(pop)
static uint32_t hcrc(const W3H&h){return crc(&h,sizeof(h)-4);} 
static void create_wal(const fs::path&p){W3H h{};std::memcpy(h.magic,"BDW3",4);h.version=3;h.hsize=sizeof(h);h.seg=1;h.first_seq=1;h.crc=hcrc(h);std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write((char*)&h,sizeof(h));}
static std::vector<uint8_t> record(uint64_t seq,uint8_t op,const std::string&k,const std::string&v){
  // frame: total_len | seq | op | key_len | value_len | header_crc | key | value | record_crc
  std::vector<uint8_t> h;put64(h,seq);h.push_back(op);put32(h,uint32_t(k.size()));put32(h,uint32_t(v.size()));uint32_t hc=crc(h.data(),h.size());
  uint32_t total=4+uint32_t(h.size())+4+uint32_t(k.size()+v.size())+4;std::vector<uint8_t>b;put32(b,total);b.insert(b.end(),h.begin(),h.end());put32(b,hc);b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());put32(b,crc(b.data(),b.size()));return b;
}
static void append(const fs::path&p,uint64_t seq,uint8_t op,const std::string&k,const std::string&v){auto b=record(seq,op,k,v);std::ofstream f(p,std::ios::binary|std::ios::app);f.write((char*)b.data(),std::streamsize(b.size()));}
enum class R{OK,TORN,CORRUPT};
static R scan(const fs::path&p,size_t&good){good=0;std::ifstream f(p,std::ios::binary);W3H wh{};f.read((char*)&wh,sizeof(wh));if(f.gcount()!=sizeof(wh)||std::memcmp(wh.magic,"BDW3",4)||wh.version!=3||wh.hsize!=sizeof(wh)||hcrc(wh)!=wh.crc)return R::CORRUPT;for(;;){uint8_t lb[4];f.read((char*)lb,4);if(f.gcount()==0)return R::OK;if(f.gcount()!=4)return R::TORN;uint32_t total=be32(lb);constexpr uint32_t MIN=4+8+1+4+4+4+4;if(total<MIN||total>(1u<<24))return R::CORRUPT;std::vector<uint8_t>b(total-4);f.read((char*)b.data(),std::streamsize(b.size()));if(f.gcount()!=std::streamsize(b.size()))return R::TORN;const uint8_t*q=b.data();uint64_t seq=be64(q);q+=8;uint8_t op=*q++;uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;uint32_t hc=be32(q);q+=4;std::vector<uint8_t>hh;put64(hh,seq);hh.push_back(op);put32(hh,kl);put32(hh,vl);if(crc(hh.data(),hh.size())!=hc)return R::CORRUPT;if(op!=1&&op!=2)return R::CORRUPT;if(uint64_t(kl)+vl+4!=uint64_t(b.data()+b.size()-q))return R::CORRUPT;uint32_t got=be32(b.data()+b.size()-4);std::vector<uint8_t>whole;whole.insert(whole.end(),lb,lb+4);whole.insert(whole.end(),b.begin(),b.end()-4);if(crc(whole.data(),whole.size())!=got)return R::CORRUPT;++good;}}
static void flip(const fs::path&p,size_t off){std::fstream f(p,std::ios::binary|std::ios::in|std::ios::out);f.seekg(off);char c;f.read(&c,1);c^=0x40;f.seekp(off);f.write(&c,1);} 
int main(){fs::path p="v45.bdw3";create_wal(p);for(int i=0;i<100;++i)append(p,i+1,1,"K"+std::to_string(i),"V"+std::to_string(i));size_t good=0;bool clean=scan(p,good)==R::OK&&good==100;auto sz=fs::file_size(p);fs::copy_file(p,"v45_len.bdw3",fs::copy_options::overwrite_existing);flip("v45_len.bdw3",sizeof(W3H)+1);size_t g1=0;bool bad_len=scan("v45_len.bdw3",g1)==R::CORRUPT;fs::copy_file(p,"v45_h.bdw3",fs::copy_options::overwrite_existing);flip("v45_h.bdw3",sizeof(W3H)+4+8+1+1);size_t g2=0;bool bad_head=scan("v45_h.bdw3",g2)==R::CORRUPT;fs::copy_file(p,"v45_tail.bdw3",fs::copy_options::overwrite_existing);fs::resize_file("v45_tail.bdw3",sz-3);size_t g3=0;bool torn=scan("v45_tail.bdw3",g3)==R::TORN&&g3==99;bool pass=clean&&bad_len&&bad_head&&torn;std::cout<<"clean,corrupt_total_len,corrupt_header,torn_tail,good_before_torn,pass\n"<<clean<<','<<bad_len<<','<<bad_head<<','<<torn<<','<<g3<<','<<pass<<"\n";return pass?0:2;}
