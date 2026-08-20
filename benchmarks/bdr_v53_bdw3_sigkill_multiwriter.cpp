#define main v51_unused_main
#include "bdr_v51_bdw3_multiwriter_keep_size.cpp"
#undef main

#include <csignal>
#include <sys/wait.h>

struct Prefix53{size_t good=0;size_t bad=0;uint64_t last=0;bool exact_eof=false;};

static Prefix53 scan_prefix53(const char* path){
    Prefix53 r;int fd=::open(path,O_RDONLY);if(fd<0)throw std::runtime_error("open");struct stat st{};if(::fstat(fd,&st))throw std::runtime_error("stat");W3H wh{};
    if(::pread(fd,&wh,sizeof(wh),0)!=(ssize_t)sizeof(wh)||std::memcmp(wh.magic,"BDW3",4)||wh.version!=3||wh.hsize!=sizeof(wh)||hcrc(wh)!=wh.crc){r.bad++;::close(fd);return r;}
    off_t pos=sizeof(W3H);uint64_t expected=1;
    while(pos<st.st_size){
        uint8_t lb[4];ssize_t nr=::pread(fd,lb,4,pos);if(nr==0)break;if(nr!=4)break;uint32_t frame=be32(lb);constexpr uint32_t MIN=4+8+1+4+4+4+4;
        if(frame<MIN||frame>(1u<<24)){r.bad++;break;}if(pos+frame>st.st_size)break;
        std::vector<uint8_t>b(frame-4);if(::pread(fd,b.data(),b.size(),pos+4)!=(ssize_t)b.size())break;const uint8_t*q=b.data();uint64_t seq=be64(q);q+=8;uint8_t op=*q++;uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;uint32_t hc=be32(q);q+=4;
        std::vector<uint8_t>hh;put64(hh,seq);hh.push_back(op);put32(hh,kl);put32(hh,vl);if(seq!=expected||op!=1||crc32b(hh.data(),hh.size())!=hc||uint64_t(kl)+vl+4!=uint64_t(b.data()+b.size()-q)){r.bad++;break;}
        uint32_t got=be32(b.data()+b.size()-4);std::vector<uint8_t>whole;whole.insert(whole.end(),lb,lb+4);whole.insert(whole.end(),b.begin(),b.end()-4);if(crc32b(whole.data(),whole.size())!=got){r.bad++;break;}
        std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);if(k.size()!=9||k[0]!='K'||v.size()!=9||v[0]!='V'||k.substr(1)!=v.substr(1)){r.bad++;break;}
        r.good++;r.last=seq;expected++;pos+=frame;
    }
    r.exact_eof=(pos==st.st_size);::close(fd);return r;
}

static void child_run53(int total){
    std::atomic<int> next{0};TicketWal3KeepSize wal("v53.bdw3",size_t(total)*96+4096,512);std::vector<std::thread>ts;
    for(int x=0;x<8;++x)ts.emplace_back([&]{uint64_t last=0;int pending=0;for(;;){int i=next.fetch_add(1);if(i>=total)break;last=wal.submit(K51(i),V51(i));if(++pending>=128){wal.wait(last);pending=0;}}if(pending)wal.wait(last);});
    for(auto&t:ts)t.join();
}

int main(){
    std::cout<<"kill_after_ms,signal_seen,recovered,last_seq,bad,exact_eof,pass\n";int fail=0;int total=1000000;
    for(int ms:{5,10,20,50,100}){
        std::filesystem::remove("v53.bdw3");pid_t p=fork();if(p==0){child_run53(total);::_exit(0);}usleep(ms*1000);::kill(p,SIGKILL);int st=0;waitpid(p,&st,0);bool sig=WIFSIGNALED(st)&&WTERMSIG(st)==SIGKILL;Prefix53 r{};bool ok=false;try{r=scan_prefix53("v53.bdw3");ok=sig&&r.good==r.last&&r.bad==0;}catch(...){ok=false;}
        std::cout<<ms<<','<<sig<<','<<r.good<<','<<r.last<<','<<r.bad<<','<<r.exact_eof<<','<<ok<<"\n";if(!ok)fail++;
    }
    return fail?2:0;
}
