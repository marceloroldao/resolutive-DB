#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>
#pragma pack(push,1)
struct H37{char magic[4];uint16_t version;uint16_t header_size;uint32_t flags;uint64_t segment_id;uint64_t first_sequence;uint64_t reserved;uint32_t crc;};
#pragma pack(pop)
static uint32_t hcrc(const H37&h){return (uint32_t)crc32(0,(const Bytef*)&h,sizeof(H37)-sizeof(uint32_t));}
static H37 good(){H37 h{};std::memcpy(h.magic,"BDW2",4);h.version=2;h.header_size=sizeof(H37);h.segment_id=7;h.first_sequence=1;h.crc=hcrc(h);return h;}
static bool writeh(const std::string&p,const H37&h,size_t cut=sizeof(H37)){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write((const char*)&h,std::min(cut,sizeof(H37)));return bool(f);}
enum class E{OK,MAGIC,VERSION,SIZE,CRC,TRUNCATED};
static E readh(const std::string&p){std::ifstream f(p,std::ios::binary);H37 h{};f.read((char*)&h,sizeof(h));if(f.gcount()!=(std::streamsize)sizeof(h))return E::TRUNCATED;if(std::memcmp(h.magic,"BDW2",4))return E::MAGIC;if(h.version!=2)return E::VERSION;if(h.header_size!=sizeof(H37))return E::SIZE;if(hcrc(h)!=h.crc)return E::CRC;return E::OK;}
static const char*n(E e){switch(e){case E::OK:return"OK";case E::MAGIC:return"MAGIC";case E::VERSION:return"VERSION";case E::SIZE:return"SIZE";case E::CRC:return"CRC";default:return"TRUNCATED";}}
int main(){std::cout<<"case,result,pass\n";int fail=0;auto run=[&](const char*name,H37 h,size_t cut,E want){std::string p=std::string("v37_")+name+".bin";writeh(p,h,cut);E got=readh(p);bool ok=got==want;std::cout<<name<<','<<n(got)<<','<<ok<<"\n";fail+=!ok;std::filesystem::remove(p);};auto h=good();run("valid",h,sizeof(h),E::OK);h=good();std::memcpy(h.magic,"BDR2",4);h.crc=hcrc(h);run("v01_magic",h,sizeof(h),E::MAGIC);h=good();h.version=99;h.crc=hcrc(h);run("future_version",h,sizeof(h),E::VERSION);h=good();h.header_size=8;h.crc=hcrc(h);run("wrong_size",h,sizeof(h),E::SIZE);h=good();h.segment_id^=0x100;run("header_crc",h,sizeof(h),E::CRC);h=good();run("torn_header",h,sizeof(h)-3,E::TRUNCATED);return fail?2:0;}
