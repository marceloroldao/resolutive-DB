#define main v22_unused_main
#define BDR BDR_V22_OLD
#define bdr bdr_v22_old
#include "bdr_v22_batched_wal_market.cpp"
#undef bdr
#undef BDR
#undef main

static bool pwriteall25(int fd,const char*p,size_t n,off_t off){while(n){ssize_t w=::pwrite(fd,p,n,off);if(w<=0)return false;p+=w;n-=w;off+=w;}return true;}
class BDR25{uint32_t M;std::vector<RH>p;std::vector<std::string>vals;std::vector<char>wal;int fd=-1;off_t off=0;uint64_t seq=0;public:BDR25(int N,const char*path):M(std::max(1,N/256)),p(M),vals(N){std::vector<size_t>c(M);for(int i=0;i<N;++i)c[enc(K(i),M).r]++;for(uint32_t i=0;i<M;++i)p[i]=RH(c[i]);wal.reserve(1<<20);fd=::open(path,O_CREAT|O_TRUNC|O_RDWR,0644);::posix_fallocate(fd,0,size_t(N)*64+4096);}~BDR25(){if(fd>=0)::close(fd);}void put(const std::string&k,const std::string&v,int id){auto a=enc(k,M);p[a.r].put(a.fp,id);vals[id]=v;uint32_t kl=k.size(),vl=v.size(),crc;uint64_t q=++seq;uLong c=crc32(0,(const Bytef*)k.data(),k.size());c=crc32(c,(const Bytef*)v.data(),v.size());crc=(uint32_t)c;add(wal,&kl,4);add(wal,&vl,4);add(wal,&q,8);add(wal,&crc,4);add(wal,k.data(),kl);add(wal,v.data(),vl);}bool sync(){if(wal.empty())return true;bool ok=pwriteall25(fd,wal.data(),wal.size(),off);off+=wal.size();wal.clear();return ok&&::fdatasync(fd)==0;}bool get(const std::string&k,std::string&v)const{auto a=enc(k,M);uint64_t id=0;if(!p[a.r].get(a.fp,id)||id>=vals.size())return false;v=vals[id];return true;}};
R bdr25(int N,int b){std::filesystem::remove("v25.bdr");BDR25 d(N,"v25.bdr");double wp=rate(N,[&]{for(int i=0;i<N;++i){d.put(K(i),V(i),i);if((i+1)%b==0)d.sync();}d.sync();});uint64_t er=0;double rd=rate(N,[&]{for(int i=0;i<N;++i){std::string o;if(!d.get(K(i),o)||o!=V(i))++er;}});return{"BDR-v25",b,wp,rd,er};}
int main(int argc,char**argv){int N=argc>1?std::stoi(argv[1]):10000;std::cout<<"engine,batch,put_ops_s,get_ops_s,errors\n";for(int b:{1,32,128})for(auto&r:{bdr25(N,b),sqlite(N,b),lmdb(N,b),level(N,b),rocks(N,b)})std::cout<<r.e<<','<<r.b<<','<<r.put<<','<<r.get<<','<<r.err<<'\n';}
