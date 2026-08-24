#include "bdr/database.hpp"
#include "bdr/resolutive_index.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <linux/falloc.h>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <zlib.h>

namespace bdr {
namespace fs = std::filesystem;
namespace {

static uint32_t crc32b(const void* p, std::size_t n, uint32_t seed = 0) {
    return uint32_t(::crc32(seed, static_cast<const Bytef*>(p), uInt(n)));
}
static void put32(std::vector<uint8_t>& b, uint32_t v) { for (int i=3;i>=0;--i) b.push_back(uint8_t(v>>(8*i))); }
static void put64(std::vector<uint8_t>& b, uint64_t v) { for (int i=7;i>=0;--i) b.push_back(uint8_t(v>>(8*i))); }
static uint32_t be32(const uint8_t* p) { return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]); }
static uint64_t be64(const uint8_t* p) { uint64_t v=0; for(int i=0;i<8;++i) v=(v<<8)|p[i]; return v; }

static void write_all(int fd,const void* p,std::size_t n){const char*q=static_cast<const char*>(p);while(n){ssize_t w=::write(fd,q,n);if(w<=0)throw std::runtime_error("BDR write failed");q+=w;n-=std::size_t(w);}}
static void pwrite_all(int fd,const uint8_t* p,std::size_t n,off_t off){while(n){ssize_t w=::pwrite(fd,p,n,off);if(w<=0)throw std::runtime_error("BDR pwrite failed");p+=w;n-=std::size_t(w);off+=w;}}
static void fsync_dir(const fs::path&p){int fd=::open(p.c_str(),O_RDONLY|O_DIRECTORY);if(fd<0)throw std::runtime_error("BDR open dir failed");int rc=::fsync(fd);::close(fd);if(rc)throw std::runtime_error("BDR dir fsync failed");}

#pragma pack(push,1)
struct Wal3Header{char magic[4];uint16_t version;uint16_t header_size;uint32_t flags;uint64_t segment_id;uint64_t first_sequence;uint32_t crc;};
#pragma pack(pop)
static uint32_t wal_hcrc(const Wal3Header&h){return crc32b(&h,sizeof(h)-4);}

static std::vector<uint8_t> make_record(uint64_t seq,OperationType type,const std::string&k,const std::string&v){
    std::vector<uint8_t> h;put64(h,seq);h.push_back(uint8_t(type));put32(h,uint32_t(k.size()));put32(h,uint32_t(v.size()));uint32_t hc=crc32b(h.data(),h.size());
    uint32_t total=4+uint32_t(h.size())+4+uint32_t(k.size()+v.size())+4;std::vector<uint8_t>b;b.reserve(total);put32(b,total);b.insert(b.end(),h.begin(),h.end());put32(b,hc);b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());put32(b,crc32b(b.data(),b.size()));return b;
}

static std::vector<uint8_t> encode_snapshot(uint64_t seq,const std::vector<std::pair<std::string,std::string>>&items){
    std::vector<uint8_t>b;b.insert(b.end(),{'B','D','R','3'});put32(b,3);put64(b,seq);put64(b,uint64_t(items.size()));for(const auto&[k,v]:items){put32(b,uint32_t(k.size()));put32(b,uint32_t(v.size()));b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());}put32(b,crc32b(b.data(),b.size()));return b;
}

struct SnapshotState{uint64_t seq=0;std::vector<std::pair<std::string,std::string>>items;};
static SnapshotState decode_snapshot(const fs::path&p){
    std::ifstream f(p,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});if(b.size()<28)throw std::runtime_error("BDR3 snapshot too short");uint32_t got=be32(b.data()+b.size()-4);if(crc32b(b.data(),b.size()-4)!=got)throw std::runtime_error("BDR3 snapshot CRC mismatch");const uint8_t*q=b.data(),*end=b.data()+b.size()-4;if(std::memcmp(q,"BDR3",4))throw std::runtime_error("BDR3 magic mismatch");q+=4;if(be32(q)!=3)throw std::runtime_error("BDR3 version unsupported");q+=4;SnapshotState s;s.seq=be64(q);q+=8;uint64_t n=be64(q);q+=8;if(n>50'000'000ULL)throw std::runtime_error("BDR3 count unreasonable");s.items.reserve(std::size_t(n));for(uint64_t i=0;i<n;++i){if(end-q<8)throw std::runtime_error("BDR3 record header truncated");uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;if(!kl||kl>(1u<<20)||vl>(1u<<24)||uint64_t(end-q)<uint64_t(kl)+vl)throw std::runtime_error("BDR3 record bounds invalid");std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);q+=vl;s.items.emplace_back(std::move(k),std::move(v));}if(q!=end)throw std::runtime_error("BDR3 trailing bytes");return s;
}

struct ReplayResult{off_t last_good=0;uint64_t segment=0;bool torn=false;};
static ReplayResult replay_wal(const fs::path&p,ResolutiveIndex&index,uint64_t&seq){
    int fd=::open(p.c_str(),O_RDONLY);if(fd<0)throw std::runtime_error("BDW3 open failed");Wal3Header wh{};if(::pread(fd,&wh,sizeof(wh),0)!=(ssize_t)sizeof(wh)){::close(fd);throw std::runtime_error("BDW3 header truncated");}if(std::memcmp(wh.magic,"BDW3",4)||wh.version!=3||wh.header_size!=sizeof(wh)||wal_hcrc(wh)!=wh.crc){::close(fd);throw std::runtime_error("BDW3 header invalid");}struct stat st{};if(::fstat(fd,&st)){::close(fd);throw std::runtime_error("BDW3 stat failed");}
    off_t pos=sizeof(Wal3Header);uint64_t expected=std::max<uint64_t>(seq+1,wh.first_sequence);ReplayResult rr{pos,wh.segment_id,false};while(pos<st.st_size){uint8_t lb[4];if(::pread(fd,lb,4,pos)!=4){rr.torn=true;break;}uint32_t total=be32(lb);constexpr uint32_t MIN=4+8+1+4+4+4+4;if(total<MIN||total>(1u<<24)){::close(fd);throw std::runtime_error("BDW3 total_len invalid");}if(pos+total>st.st_size){rr.torn=true;break;}std::vector<uint8_t>b(total-4);if(::pread(fd,b.data(),b.size(),pos+4)!=(ssize_t)b.size()){rr.torn=true;break;}const uint8_t*q=b.data();uint64_t rseq=be64(q);q+=8;uint8_t op=*q++;uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;uint32_t hc=be32(q);q+=4;std::vector<uint8_t>hh;put64(hh,rseq);hh.push_back(op);put32(hh,kl);put32(hh,vl);if(crc32b(hh.data(),hh.size())!=hc||!kl||kl>(1u<<20)||vl>(1u<<24)||uint64_t(kl)+vl+4!=uint64_t(b.data()+b.size()-q)){::close(fd);throw std::runtime_error("BDW3 frame header invalid");}if(op!=uint8_t(OperationType::Put)&&op!=uint8_t(OperationType::Delete)){::close(fd);throw std::runtime_error("BDW3 op invalid");}uint32_t rc=be32(b.data()+b.size()-4);std::vector<uint8_t>whole;whole.reserve(total-4);whole.insert(whole.end(),lb,lb+4);whole.insert(whole.end(),b.begin(),b.end()-4);if(crc32b(whole.data(),whole.size())!=rc){::close(fd);throw std::runtime_error("BDW3 record CRC mismatch");}std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);if(rseq>seq){if(rseq!=expected){::close(fd);throw std::runtime_error("BDW3 sequence gap");}if(op==uint8_t(OperationType::Put))index.put(k,v);else index.erase(k);seq=rseq;++expected;}pos+=total;rr.last_good=pos;}
    ::close(fd);return rr;
}

static std::string wal_name(uint64_t seg){char b[48];std::snprintf(b,sizeof(b),"wal-%012llu.bdw3",static_cast<unsigned long long>(seg));return b;}

} // namespace

class Database::Impl {
public:
    struct Pending{uint64_t seq;OperationType type;std::string key,value;};
    fs::path dir;Options options;ResolutiveIndex index;
    std::mutex submit_mu,queue_mu,wal_mu;std::condition_variable queue_cv,durable_cv;std::deque<Pending>queue;
    int wal_fd=-1;off_t wal_off=0;fs::path active_wal;uint64_t segment_id=0;
    std::atomic<uint64_t>next_seq{0},durable_seq{0};bool stop=false,closed=false;std::thread writer;

    Impl(fs::path d,Options o):dir(std::move(d)),options(o),index(o.partition_count,o.partition_max_load){if(!options.wal_batch)throw std::invalid_argument("wal_batch must be > 0");fs::create_directories(dir);recover();writer=std::thread([this]{writer_loop();});}
    ~Impl(){try{close_impl();}catch(...){}}

    void create_wal(uint64_t seg,uint64_t first){active_wal=dir/wal_name(seg);int fd=::open(active_wal.c_str(),O_CREAT|O_TRUNC|O_RDWR,0644);if(fd<0)throw std::runtime_error("BDW3 create failed");Wal3Header h{};std::memcpy(h.magic,"BDW3",4);h.version=3;h.header_size=sizeof(h);h.segment_id=seg;h.first_sequence=first;h.crc=wal_hcrc(h);write_all(fd,&h,sizeof(h));if(::fdatasync(fd)){::close(fd);throw std::runtime_error("BDW3 header sync failed");}if(options.keep_size_preallocation&&options.reserve_bytes){if(::fallocate(fd,FALLOC_FL_KEEP_SIZE,0,off_t(options.reserve_bytes))){::close(fd);throw std::runtime_error("BDW3 KEEP_SIZE failed");}}wal_fd=fd;wal_off=sizeof(Wal3Header);segment_id=seg;}

    void recover(){uint64_t seq=0;fs::path snap=dir/"snapshot.bdr3";if(fs::exists(snap)){auto s=decode_snapshot(snap);seq=s.seq;for(auto&[k,v]:s.items)index.put(k,v);}std::vector<fs::path>wals;for(const auto&e:fs::directory_iterator(dir))if(e.is_regular_file()&&e.path().extension()==".bdw3")wals.push_back(e.path());std::sort(wals.begin(),wals.end());uint64_t high=0;ReplayResult last{};for(std::size_t i=0;i<wals.size();++i){auto rr=replay_wal(wals[i],index,seq);high=std::max(high,rr.segment);if(rr.torn&&i+1!=wals.size())throw std::runtime_error("torn non-final WAL");if(i+1==wals.size())last=rr;}next_seq.store(seq);durable_seq.store(seq);if(wals.empty()){create_wal(1,seq+1);fsync_dir(dir);}else{active_wal=wals.back();if(last.torn){int fd=::open(active_wal.c_str(),O_RDWR);if(fd<0||::ftruncate(fd,last.last_good)||::fdatasync(fd)){if(fd>=0)::close(fd);throw std::runtime_error("tail repair failed");}::close(fd);}wal_fd=::open(active_wal.c_str(),O_RDWR);if(wal_fd<0)throw std::runtime_error("active WAL open failed");wal_off=::lseek(wal_fd,0,SEEK_END);segment_id=high;if(options.keep_size_preallocation&&options.reserve_bytes>std::size_t(wal_off))if(::fallocate(wal_fd,FALLOC_FL_KEEP_SIZE,0,off_t(options.reserve_bytes)))throw std::runtime_error("re-preallocation failed");}}

    Ticket submit_impl(Operation op){
        if(op.key.empty()||op.key.size()>(1u<<20)||op.value.size()>(1u<<24))throw std::invalid_argument("invalid BDR key/value size");
        std::lock_guard<std::mutex> order(submit_mu);
        if(closed)throw std::runtime_error("database closed");
        const uint64_t seq=next_seq.load(std::memory_order_acquire)+1;
        Pending pending{seq,op.type,std::move(op.key),std::move(op.value)};
        {
            std::unique_lock<std::mutex> qg(queue_mu);
            queue.push_back(std::move(pending));
            try{
                const Pending& accepted=queue.back();
                if(accepted.type==OperationType::Put)index.put(accepted.key,accepted.value);else index.erase(accepted.key);
                next_seq.store(seq,std::memory_order_release);
            }catch(...){
                queue.pop_back();
                throw;
            }
        }
        queue_cv.notify_one();
        return Ticket{seq};
    }

    void writer_loop(){std::vector<Pending>local;std::vector<uint8_t>buf;buf.reserve(1<<20);for(;;){local.clear();{std::unique_lock<std::mutex>lk(queue_mu);queue_cv.wait(lk,[&]{return stop||!queue.empty();});if(stop&&queue.empty())break;while(!queue.empty()&&local.size()<options.wal_batch){local.push_back(std::move(queue.front()));queue.pop_front();}}buf.clear();for(const auto&p:local){auto r=make_record(p.seq,p.type,p.key,p.value);buf.insert(buf.end(),r.begin(),r.end());}{std::lock_guard<std::mutex>wg(wal_mu);pwrite_all(wal_fd,buf.data(),buf.size(),wal_off);wal_off+=off_t(buf.size());if(::fdatasync(wal_fd))std::abort();}durable_seq.store(local.back().seq);durable_cv.notify_all();}}

    void wait_impl(Ticket t){if(!t)return;if(t.sequence>next_seq.load())throw std::invalid_argument("future ticket");std::unique_lock<std::mutex>lk(queue_mu);durable_cv.wait(lk,[&]{return durable_seq.load()>=t.sequence;});}
    void sync_impl(){wait_impl(Ticket{next_seq.load()});}

    void checkpoint_impl(){std::lock_guard<std::mutex>sg(submit_mu);if(closed)throw std::runtime_error("database closed");uint64_t seq=next_seq.load();wait_impl(Ticket{seq});auto items=index.snapshot_items();auto bytes=encode_snapshot(seq,items);fs::path tmp=dir/"snapshot.tmp",dst=dir/"snapshot.bdr3";int sfd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(sfd<0)throw std::runtime_error("snapshot temp open failed");write_all(sfd,bytes.data(),bytes.size());if(::fsync(sfd)){::close(sfd);throw std::runtime_error("snapshot fsync failed");}::close(sfd);fs::rename(tmp,dst);fsync_dir(dir);std::lock_guard<std::mutex>wg(wal_mu);if(wal_fd>=0){if(::fsync(wal_fd))throw std::runtime_error("pre-rotation fsync failed");::close(wal_fd);wal_fd=-1;}create_wal(segment_id+1,seq+1);fsync_dir(dir);for(const auto&e:fs::directory_iterator(dir))if(e.is_regular_file()&&e.path().extension()==".bdw3"&&e.path()!=active_wal)fs::remove(e.path());fsync_dir(dir);}

    void close_impl(){
        {
            std::lock_guard<std::mutex> sg(submit_mu);
            if(closed)return;
            closed=true;
        }
        sync_impl();
        {std::lock_guard<std::mutex>qg(queue_mu);stop=true;}
        queue_cv.notify_one();
        if(writer.joinable())writer.join();
        {std::lock_guard<std::mutex>wg(wal_mu);if(wal_fd>=0){::fdatasync(wal_fd);::close(wal_fd);wal_fd=-1;}}
    }
};

Database::Database(std::unique_ptr<Impl>p):impl_(std::move(p)){}
Database::~Database()=default;
std::unique_ptr<Database> Database::open(const fs::path&d,Options o){return std::unique_ptr<Database>(new Database(std::make_unique<Impl>(d,o)));}
Ticket Database::submit(Operation o){return impl_->submit_impl(std::move(o));}
Ticket Database::put(std::string k,std::string v){return submit({OperationType::Put,std::move(k),std::move(v)});}
Ticket Database::erase(std::string k){return submit({OperationType::Delete,std::move(k),{}});}
void Database::put_sync(std::string k,std::string v){wait(put(std::move(k),std::move(v)));}
void Database::erase_sync(std::string k){wait(erase(std::move(k)));}
std::optional<std::string> Database::get(const std::string&k)const{return impl_->index.get(k);}
bool Database::contains(const std::string&k)const{return impl_->index.contains(k);}
void Database::wait(Ticket t){impl_->wait_impl(t);}
void Database::sync(){impl_->sync_impl();}
void Database::checkpoint(){impl_->checkpoint_impl();}
void Database::close(){impl_->close_impl();}
std::uint64_t Database::last_sequence()const noexcept{return impl_->next_seq.load();}
std::uint64_t Database::durable_sequence()const noexcept{return impl_->durable_seq.load();}
std::size_t Database::size()const{return impl_->index.size();}
IndexStats Database::index_stats()const{return impl_->index.stats();}

} // namespace bdr
