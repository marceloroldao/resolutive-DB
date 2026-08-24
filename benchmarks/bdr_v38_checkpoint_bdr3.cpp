#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <zlib.h>

namespace fs=std::filesystem;
static void xwrite(int fd,const void*buf,size_t n){const char*p=(const char*)buf;while(n){ssize_t w=::write(fd,p,n);if(w<=0)throw std::runtime_error("write");p+=w;n-=size_t(w);}}
static void fsync_dir(const fs::path&p){int fd=::open(p.c_str(),O_RDONLY|O_DIRECTORY);if(fd<0)throw std::runtime_error("open dir");if(::fsync(fd))throw std::runtime_error("fsync dir");::close(fd);} 
static uint32_t crc_bytes(const void*p,size_t n,uint32_t seed=0){return uint32_t(crc32(seed,(const Bytef*)p,n));}
static void put_u32(std::vector<uint8_t>&b,uint32_t v){for(int i=3;i>=0;--i)b.push_back(uint8_t(v>>(i*8)));}
static void put_u64(std::vector<uint8_t>&b,uint64_t v){for(int i=7;i>=0;--i)b.push_back(uint8_t(v>>(i*8)));}
static uint32_t get_u32(const uint8_t*&p){uint32_t v=0;for(int i=0;i<4;++i)v=(v<<8)|*p++;return v;}
static uint64_t get_u64(const uint8_t*&p){uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|*p++;return v;}

struct SnapInfo{uint64_t seq=0;std::map<std::string,std::string>kv;};
static std::vector<uint8_t> encode_snapshot(uint64_t seq,const std::map<std::string,std::string>&kv){
    std::vector<uint8_t>b; b.insert(b.end(),{'B','D','R','3'});put_u32(b,3);put_u64(b,seq);put_u64(b,kv.size());
    for(auto&[k,v]:kv){put_u32(b,uint32_t(k.size()));put_u32(b,uint32_t(v.size()));b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());}
    put_u32(b,crc_bytes(b.data(),b.size())); return b;
}
static SnapInfo decode_snapshot(const fs::path&p){
    std::ifstream f(p,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});if(b.size()<28)throw std::runtime_error("snapshot short");
    uint32_t got=(uint32_t(b[b.size()-4])<<24)|(uint32_t(b[b.size()-3])<<16)|(uint32_t(b[b.size()-2])<<8)|b.back();if(crc_bytes(b.data(),b.size()-4)!=got)throw std::runtime_error("snapshot crc");
    const uint8_t*q=b.data();if(std::memcmp(q,"BDR3",4))throw std::runtime_error("snapshot magic");q+=4;if(get_u32(q)!=3)throw std::runtime_error("snapshot version");SnapInfo s;s.seq=get_u64(q);uint64_t n=get_u64(q);
    const uint8_t*end=b.data()+b.size()-4;for(uint64_t i=0;i<n;++i){if(end-q<8)throw std::runtime_error("snapshot record");uint32_t kl=get_u32(q),vl=get_u32(q);if(uint64_t(end-q)<uint64_t(kl)+vl)throw std::runtime_error("snapshot bounds");std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);q+=vl;s.kv[k]=v;}if(q!=end)throw std::runtime_error("snapshot trailing");return s;
}

#pragma pack(push,1)
struct WalHeader{char magic[4];uint16_t version;uint16_t header_size;uint32_t flags;uint64_t segment_id;uint64_t first_sequence;uint64_t reserved;uint32_t crc;};
struct RecHeader{uint64_t seq;uint8_t op;uint32_t kl;uint32_t vl;uint32_t crc;};
#pragma pack(pop)
static uint32_t whcrc(const WalHeader&h){return crc_bytes(&h,sizeof(h)-4);} 
static void create_wal(const fs::path&p,uint64_t seg,uint64_t first){WalHeader h{};std::memcpy(h.magic,"BDW2",4);h.version=2;h.header_size=sizeof(h);h.segment_id=seg;h.first_sequence=first;h.crc=whcrc(h);int fd=::open(p.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("create wal");xwrite(fd,&h,sizeof(h));if(::fdatasync(fd))throw std::runtime_error("wal sync");::close(fd);} 
static void append_wal(const fs::path&p,uint64_t seq,uint8_t op,const std::string&k,const std::string&v){std::vector<uint8_t>x;put_u64(x,seq);x.push_back(op);put_u32(x,k.size());put_u32(x,v.size());x.insert(x.end(),k.begin(),k.end());x.insert(x.end(),v.begin(),v.end());uint32_t c=crc_bytes(x.data(),x.size());int fd=::open(p.c_str(),O_WRONLY|O_APPEND);if(fd<0)throw std::runtime_error("open wal");xwrite(fd,x.data(),x.size());uint8_t cb[4]={uint8_t(c>>24),uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)};xwrite(fd,cb,4);if(::fdatasync(fd))throw std::runtime_error("append sync");::close(fd);} 
static void replay_wal(const fs::path&p,SnapInfo&s){std::ifstream f(p,std::ios::binary);WalHeader h{};f.read((char*)&h,sizeof(h));if(f.gcount()!=sizeof(h)||std::memcmp(h.magic,"BDW2",4)||h.version!=2||h.header_size!=sizeof(h)||whcrc(h)!=h.crc)throw std::runtime_error("wal header");std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});const uint8_t*q=b.data(),*end=b.data()+b.size();uint64_t expected=std::max<uint64_t>(s.seq+1,h.first_sequence);while(q<end){const uint8_t*start=q;if(end-q<17)break;uint64_t seq=get_u64(q);uint8_t op=*q++;uint32_t kl=get_u32(q),vl=get_u32(q);if(uint64_t(end-q)<uint64_t(kl)+vl+4)break;std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);q+=vl;uint32_t got=get_u32(q);if(crc_bytes(start,size_t(q-start)-4)!=got)throw std::runtime_error("wal record crc");if(seq<=s.seq)continue;if(seq!=expected)throw std::runtime_error("wal sequence");if(op==1)s.kv[k]=v;else if(op==2)s.kv.erase(k);else throw std::runtime_error("wal op");s.seq=seq;++expected;}}

static void atomic_checkpoint(const fs::path&dir,uint64_t seq,const std::map<std::string,std::string>&kv,uint64_t next_seg){
    fs::path active=dir/"wal-000000.log";int afd=::open(active.c_str(),O_RDONLY);if(afd>=0){if(::fsync(afd))throw std::runtime_error("pre checkpoint wal sync");::close(afd);} 
    auto bytes=encode_snapshot(seq,kv);fs::path tmp=dir/"snapshot.tmp",dst=dir/"snapshot.bdr3";int fd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("snapshot tmp");xwrite(fd,bytes.data(),bytes.size());if(::fsync(fd))throw std::runtime_error("snapshot sync");::close(fd);fs::rename(tmp,dst);fsync_dir(dir);
    fs::path nw=dir/"wal-000001.log";create_wal(nw,next_seg,seq+1);fsync_dir(dir);if(fs::exists(active))fs::remove(active);fsync_dir(dir);
}
static SnapInfo reopen(const fs::path&dir){SnapInfo s;if(fs::exists(dir/"snapshot.bdr3"))s=decode_snapshot(dir/"snapshot.bdr3");std::vector<fs::path>w;for(auto&e:fs::directory_iterator(dir))if(e.path().filename().string().rfind("wal-",0)==0)w.push_back(e.path());std::sort(w.begin(),w.end());for(auto&p:w)replay_wal(p,s);return s;}

int main(){fs::path d="v38db";fs::remove_all(d);fs::create_directory(d);create_wal(d/"wal-000000.log",1,1);std::map<std::string,std::string>kv;uint64_t seq=0;for(int i=0;i<10000;++i){auto k="K"+std::to_string(i),v="V"+std::to_string(i);kv[k]=v;append_wal(d/"wal-000000.log",++seq,1,k,v);}atomic_checkpoint(d,seq,kv,2);for(int i=0;i<1000;++i){auto k="P"+std::to_string(i),v="Q"+std::to_string(i);kv[k]=v;append_wal(d/"wal-000001.log",++seq,1,k,v);}for(int i=0;i<250;++i){auto k="K"+std::to_string(i);kv.erase(k);append_wal(d/"wal-000001.log",++seq,2,k,"");}
    auto r=reopen(d);bool ok=r.seq==seq&&r.kv==kv&&fs::exists(d/"snapshot.bdr3")&&!fs::exists(d/"snapshot.tmp")&&!fs::exists(d/"wal-000000.log")&&fs::exists(d/"wal-000001.log");std::cout<<"records_expected,records_recovered,last_seq_expected,last_seq_recovered,old_wal_retired,new_wal_present,pass\n"<<kv.size()<<','<<r.kv.size()<<','<<seq<<','<<r.seq<<','<<(!fs::exists(d/"wal-000000.log"))<<','<<fs::exists(d/"wal-000001.log")<<','<<ok<<"\n";return ok?0:2;}
