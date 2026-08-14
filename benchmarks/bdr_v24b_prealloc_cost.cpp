#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
using Clock=std::chrono::steady_clock;
int main(){std::cout<<"records,reserved_bytes,setup_ms\n";for(long long n:{10000LL,50000LL,100000LL,1000000LL})for(int rep=0;rep<3;++rep){const char*p="v24b.wal";std::filesystem::remove(p);size_t bytes=size_t(n)*64+4096;auto a=Clock::now();int fd=::open(p,O_CREAT|O_TRUNC|O_RDWR,0644);int rc=::posix_fallocate(fd,0,bytes);::close(fd);auto z=Clock::now();std::cout<<n<<','<<bytes<<','<<std::chrono::duration<double,std::milli>(z-a).count()<<'\n';if(rc) return rc;}std::filesystem::remove("v24b.wal");}
