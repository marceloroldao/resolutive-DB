#define main v50_unused_main
#include "bdr_v50_keep_size_prealloc.cpp"
#undef main

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>

static std::string K51(int i){char b[32];std::snprintf(b,sizeof(b),"K%08d",i);return b;}
static std::string V51(int i){char b[32];std::snprintf(b,sizeof(b),"V%08d",i);return b;}

struct Req51{uint64_t ticket;std::string key,value;};

class TicketWal3KeepSize {
    int fd_=-1;
    off_t off_=0;
    size_t max_batch_;
    std::mutex mu_;
    std::condition_variable cv_,ack_;
    std::deque<Req51> q_;
    uint64_t next_=0,durable_=0;
    bool stop_=false;
    std::thread io_;
public:
    TicketWal3KeepSize(const char* path,size_t reserve_bytes,size_t max_batch):max_batch_(max_batch){
        fd_=::open(path,O_CREAT|O_TRUNC|O_RDWR,0644);
        if(fd_<0) throw std::runtime_error("open");
        if(::fallocate(fd_,FALLOC_FL_KEEP_SIZE,0,reserve_bytes)!=0) throw std::runtime_error("fallocate KEEP_SIZE");
        auto h=header3();
        pwrite_all(fd_,h.data(),h.size(),0);
        off_=off_t(h.size());
        if(::fdatasync(fd_)!=0) throw std::runtime_error("header sync");
        io_=std::thread([this]{loop();});
    }
    ~TicketWal3KeepSize(){
        {std::lock_guard<std::mutex> g(mu_);stop_=true;}
        cv_.notify_one();
        io_.join();
        if(fd_>=0)::close(fd_);
    }
    uint64_t submit(std::string k,std::string v){
        std::lock_guard<std::mutex> g(mu_);
        uint64_t t=++next_;
        q_.push_back({t,std::move(k),std::move(v)});
        cv_.notify_one();
        return t;
    }
    void wait(uint64_t t){
        std::unique_lock<std::mutex> lk(mu_);
        ack_.wait(lk,[&]{return durable_>=t;});
    }
private:
    void loop(){
        std::vector<Req51> local;
        std::vector<uint8_t> buf;
        buf.reserve(1<<20);
        for(;;){
            local.clear();
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk,[&]{return stop_||!q_.empty();});
                if(stop_&&q_.empty()) break;
                while(!q_.empty()&&local.size()<max_batch_){local.push_back(std::move(q_.front()));q_.pop_front();}
            }
            buf.clear();
            for(auto& r:local){
                auto rec=rec3(r.ticket,r.key,r.value);
                buf.insert(buf.end(),rec.begin(),rec.end());
            }
            pwrite_all(fd_,buf.data(),buf.size(),off_);
            off_+=off_t(buf.size());
            if(::fdatasync(fd_)!=0) std::abort();
            {
                std::lock_guard<std::mutex> g(mu_);
                durable_=local.back().ticket;
            }
            ack_.notify_all();
        }
    }
};

struct Recovery51{size_t good=0,bad=0,missing=0,duplicates=0;uint64_t last=0;bool exact_eof=false;};

static Recovery51 recover51(const char* path,int total){
    Recovery51 r;
    int fd=::open(path,O_RDONLY);
    if(fd<0) throw std::runtime_error("recover open");
    struct stat st{};if(::fstat(fd,&st)) throw std::runtime_error("recover stat");
    W3H wh{};
    if(::pread(fd,&wh,sizeof(wh),0)!=(ssize_t)sizeof(wh)||std::memcmp(wh.magic,"BDW3",4)||wh.version!=3||wh.hsize!=sizeof(wh)||hcrc(wh)!=wh.crc){r.bad++;::close(fd);return r;}
    off_t pos=sizeof(W3H);uint64_t expected=1;
    std::unordered_map<std::string,std::string> kv;kv.reserve(size_t(total)*2);
    while(pos<st.st_size){
        uint8_t lb[4];if(::pread(fd,lb,4,pos)!=4){r.bad++;break;}
        uint32_t frame=be32(lb);constexpr uint32_t MIN=4+8+1+4+4+4+4;
        if(frame<MIN||frame>(1u<<24)||pos+frame>st.st_size){r.bad++;break;}
        std::vector<uint8_t>b(frame-4);if(::pread(fd,b.data(),b.size(),pos+4)!=(ssize_t)b.size()){r.bad++;break;}
        const uint8_t*q=b.data();uint64_t seq=be64(q);q+=8;uint8_t op=*q++;uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;uint32_t hc=be32(q);q+=4;
        std::vector<uint8_t>hh;put64(hh,seq);hh.push_back(op);put32(hh,kl);put32(hh,vl);
        if(seq!=expected||op!=1||crc32b(hh.data(),hh.size())!=hc||uint64_t(kl)+vl+4!=uint64_t(b.data()+b.size()-q)){r.bad++;break;}
        uint32_t got=be32(b.data()+b.size()-4);std::vector<uint8_t>whole;whole.insert(whole.end(),lb,lb+4);whole.insert(whole.end(),b.begin(),b.end()-4);
        if(crc32b(whole.data(),whole.size())!=got){r.bad++;break;}
        std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);
        if(!kv.emplace(k,v).second) r.duplicates++;
        r.good++;r.last=seq;expected++;pos+=frame;
    }
    r.exact_eof=(pos==st.st_size);::close(fd);
    for(int i=0;i<total;++i){auto it=kv.find(K51(i));if(it==kv.end()||it->second!=V51(i))r.missing++;}
    return r;
}

static double pct51(std::vector<double> v,double q){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[std::min(v.size()-1,size_t(q*(v.size()-1)))];}

int main(int argc,char**argv){
    int total=argc>1?std::stoi(argv[1]):100000;
    int max_batch=argc>2?std::stoi(argv[2]):512;
    std::cout<<"writers,window,total,max_batch,throughput_ops_s,submit_p50_us,submit_p99_us,wait_p50_us,wait_p99_us,recovered,last_seq,bad,missing,duplicates,exact_eof,pass\n";
    int fail=0;
    for(int writers:{1,2,4,8,16}) for(int window:{1,32,128}){
        std::filesystem::remove("v51.bdw3");
        std::atomic<int> next{0};std::vector<std::vector<double>> sub(writers),wt(writers);
        auto t0=Clock::now();
        {
            TicketWal3KeepSize wal("v51.bdw3",size_t(total)*96+4096,max_batch);
            std::vector<std::thread> ts;
            for(int x=0;x<writers;++x) ts.emplace_back([&,x]{uint64_t last=0;int pending=0;for(;;){int i=next.fetch_add(1);if(i>=total)break;auto a=Clock::now();last=wal.submit(K51(i),V51(i));auto b=Clock::now();sub[x].push_back(std::chrono::duration<double,std::micro>(b-a).count());if(++pending>=window){auto c=Clock::now();wal.wait(last);auto d=Clock::now();wt[x].push_back(std::chrono::duration<double,std::micro>(d-c).count());pending=0;}}if(pending){auto c=Clock::now();wal.wait(last);auto d=Clock::now();wt[x].push_back(std::chrono::duration<double,std::micro>(d-c).count());}});
            for(auto&t:ts)t.join();
        }
        double sec=std::chrono::duration<double>(Clock::now()-t0).count();
        std::vector<double>s,w;for(auto&v:sub)s.insert(s.end(),v.begin(),v.end());for(auto&v:wt)w.insert(w.end(),v.begin(),v.end());
        auto r=recover51("v51.bdw3",total);bool pass=r.good==size_t(total)&&r.last==uint64_t(total)&&!r.bad&&!r.missing&&!r.duplicates&&r.exact_eof;
        std::cout<<writers<<','<<window<<','<<total<<','<<max_batch<<','<<total/sec<<','<<pct51(s,.5)<<','<<pct51(s,.99)<<','<<pct51(w,.5)<<','<<pct51(w,.99)<<','<<r.good<<','<<r.last<<','<<r.bad<<','<<r.missing<<','<<r.duplicates<<','<<r.exact_eof<<','<<pass<<"\n";
        if(!pass)fail++;
    }
    return fail?2:0;
}
