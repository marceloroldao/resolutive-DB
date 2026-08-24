#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <zlib.h>
using Clock=std::chrono::steady_clock;
static std::string K(int i){char b[32];std::snprintf(b,sizeof(b),"K%08d",i);return b;}static std::string V(int i){char b[32];std::snprintf(b,sizeof(b),"V%08d",i);return b;}
static void add(std::vector<char>&b,const void*p,size_t n){const char*q=(const char*)p;b.insert(b.end(),q,q+n);}static bool writeall(int fd,const char*p,size_t n){while(n){ssize_t w=::write(fd,p,n);if(w<=0)return false;p+=w;n-=w;}return true;}
class Wal{int fd=-1;bool dsync=false;std::vector<char>b;uint64_t seq=0;public:Wal(const char*path,bool d):dsync(d){int flags=O_CREAT|O_TRUNC|O_WRONLY|O_APPEND|(d?O_DSYNC:0);fd=::open(path,flags,0644);b.reserve(1<<20);}~Wal(){if(fd>=0)::close(fd);}void put(const std::string&k,const std::string&v){uint32_t kl=k.size(),vl=v.size(),crc;uint64_t s=++seq;uLong c=crc32(0,(const Bytef*)k.data(),k.size());c=crc32(c,(const Bytef*)v.data(),v.size());crc=(uint32_t)c;add(b,&kl,4);add(b,&vl,4);add(b,&s,8);add(b,&crc,4);add(b,k.data(),kl);add(b,v.data(),vl);}bool commit(){if(b.empty())return true;bool ok=writeall(fd,b.data(),b.size());b.clear();if(!ok)return false;return dsync||::fdatasync(fd)==0;}};
template<class F>double rate(int n,F&&f){auto a=Clock::now();f();return n/std::chrono::duration<double>(Clock::now()-a).count();}
static double run(int N,int batch,bool dsync){std::filesystem::remove(dsync?"v23_dsync.wal":"v23_fdatasync.wal");Wal w(dsync?"v23_dsync.wal":"v23_fdatasync.wal",dsync);return rate(N,[&]{for(int i=0;i<N;++i){w.put(K(i),V(i));if((i+1)%batch==0)w.commit();}w.commit();});}
int main(int argc,char**argv){int N=argc>1?std::stoi(argv[1]):20000;std::cout<<"mode,batch,put_ops_s\n";for(int b:{1,8,32,128}){std::cout<<"write+fdatasync,"<<b<<','<<run(N,b,false)<<'\n';std::cout<<"O_DSYNC_write,"<<b<<','<<run(N,b,true)<<'\n';}}
