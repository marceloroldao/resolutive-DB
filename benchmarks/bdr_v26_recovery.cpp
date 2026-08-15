#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <zlib.h>

static std::string K(int i){char b[32];std::snprintf(b,sizeof(b),"K%08d",i);return b;}
static std::string V(int i){char b[32];std::snprintf(b,sizeof(b),"V%08d",i);return b;}
static void add(std::vector<char>&b,const void*p,size_t n){auto q=(const char*)p;b.insert(b.end(),q,q+n);} 
static bool pwriteall(int fd,const char*p,size_t n,off_t off){while(n){ssize_t w=::pwrite(fd,p,n,off);if(w<=0)return false;p+=w;n-=w;off+=w;}return true;}

struct RecHdr{uint32_t kl,vl;uint64_t seq;uint32_t crc;};
struct RecoveryResult{size_t good=0;size_t bad=0;uint64_t last_seq=0;bool truncated_tail=false;std::unordered_map<std::string,std::string> kv;};

static uint32_t crcKV(const std::string&k,const std::string&v){uLong c=crc32(0,(const Bytef*)k.data(),k.size());c=crc32(c,(const Bytef*)v.data(),v.size());return (uint32_t)c;}

class WAL26{
 int fd=-1; off_t off=0; uint64_t seq=0; std::vector<char>buf;
public:
 WAL26(const char*path,size_t reserve_bytes){fd=::open(path,O_CREAT|O_TRUNC|O_RDWR,0644); if(fd<0)throw std::runtime_error("open"); if(::posix_fallocate(fd,0,reserve_bytes)!=0)throw std::runtime_error("fallocate"); buf.reserve(1<<20);} 
 ~WAL26(){if(fd>=0)::close(fd);} 
 void put(const std::string&k,const std::string&v){RecHdr h{(uint32_t)k.size(),(uint32_t)v.size(),++seq,crcKV(k,v)};add(buf,&h,sizeof(h));add(buf,k.data(),k.size());add(buf,v.data(),v.size());}
 bool sync(){if(buf.empty())return true;bool ok=pwriteall(fd,buf.data(),buf.size(),off);off+=buf.size();buf.clear();return ok&&::fdatasync(fd)==0;}
 off_t used()const{return off+(off_t)buf.size();}
};

static RecoveryResult recover(const std::string&path,off_t logical_end=-1){RecoveryResult r;int fd=::open(path.c_str(),O_RDONLY);if(fd<0)throw std::runtime_error("open recovery");off_t end=logical_end;if(end<0){end=::lseek(fd,0,SEEK_END);}off_t p=0;uint64_t expected=1;while(p<end){RecHdr h{};if(end-p<(off_t)sizeof(h)){r.truncated_tail=true;break;}ssize_t n=::pread(fd,&h,sizeof(h),p);if(n!=(ssize_t)sizeof(h)){r.truncated_tail=true;break;}if(h.kl==0||h.kl>(1u<<20)||h.vl>(1u<<24)||h.seq!=expected){r.bad++;break;}off_t need=sizeof(h)+(off_t)h.kl+h.vl;if(end-p<need){r.truncated_tail=true;break;}std::string k(h.kl,'\0'),v(h.vl,'\0');if(::pread(fd,k.data(),h.kl,p+sizeof(h))!=(ssize_t)h.kl||::pread(fd,v.data(),h.vl,p+sizeof(h)+h.kl)!=(ssize_t)h.vl){r.truncated_tail=true;break;}if(crcKV(k,v)!=h.crc){r.bad++;break;}r.kv[k]=v;r.good++;r.last_seq=h.seq;expected++;p+=need;}::close(fd);return r;}

static bool corrupt_byte(const std::string&path,off_t pos){int fd=::open(path.c_str(),O_RDWR);if(fd<0)return false;unsigned char b=0;if(::pread(fd,&b,1,pos)!=1){::close(fd);return false;}b^=0x5A;bool ok=::pwrite(fd,&b,1,pos)==1&&::fdatasync(fd)==0;::close(fd);return ok;}

int main(int argc,char**argv){int N=argc>1?std::stoi(argv[1]):100000;int batch=argc>2?std::stoi(argv[2]):128;std::string path="v26.wal";std::filesystem::remove(path);off_t used=0;{
 WAL26 w(path,(size_t)N*96+4096);for(int i=0;i<N;++i){w.put(K(i),V(i));if((i+1)%batch==0)w.sync();}w.sync();used=w.used();}
 auto clean=recover(path,used);size_t clean_err=0;for(int i=0;i<N;++i){auto it=clean.kv.find(K(i));if(it==clean.kv.end()||it->second!=V(i))clean_err++;}
 off_t trunc_end=std::max<off_t>(0,used-7);auto trunc=recover(path,trunc_end);
 std::filesystem::copy_file(path,"v26_corrupt.wal",std::filesystem::copy_options::overwrite_existing);off_t corrupt_pos=used/2;corrupt_byte("v26_corrupt.wal",corrupt_pos);auto corrupt=recover("v26_corrupt.wal",used);
 std::cout<<"case,good,bad,last_seq,truncated_tail,errors\n";
 std::cout<<"clean,"<<clean.good<<','<<clean.bad<<','<<clean.last_seq<<','<<clean.truncated_tail<<','<<clean_err<<"\n";
 std::cout<<"truncated_tail,"<<trunc.good<<','<<trunc.bad<<','<<trunc.last_seq<<','<<trunc.truncated_tail<<",0\n";
 std::cout<<"mid_corrupt,"<<corrupt.good<<','<<corrupt.bad<<','<<corrupt.last_seq<<','<<corrupt.truncated_tail<<",0\n";
 return clean_err==0&&clean.good==(size_t)N&&clean.bad==0?0:2;
}
