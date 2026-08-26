#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>

using Clock=std::chrono::steady_clock;
static inline std::uint64_t mix64(std::uint64_t x) noexcept { x+=0x9e3779b97f4a7c15ULL; x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL; x=(x^(x>>27))*0x94d049bb133111ebULL; return x^(x>>31); }
static inline std::uint64_t fnv1a64(const std::string&s,std::uint64_t seed=1469598103934665603ULL) noexcept { std::uint64_t h=seed; for(unsigned char c:s){h^=c;h*=1099511628211ULL;} return h; }
static std::size_t np2(std::size_t n){std::size_t p=8;while(p<n)p<<=1;return p;}

class ReclaimCompactIndex {
    struct Slot { std::uint64_t fp=0; std::uint32_t key_off=0,val_off=0,key_len=0,val_len=0; std::uint16_t dist=0; std::uint8_t used=0; };
    struct Part {
        mutable std::shared_mutex mu; std::vector<Slot> slots{8}; std::vector<char> arena; std::size_t records=0, live_bytes=0;
        static std::size_t start(std::uint64_t fp,std::size_t mask){return std::size_t(mix64(fp))&mask;}
        std::string_view keyv(const Slot&s)const{return {arena.data()+s.key_off,s.key_len};}
        std::string val(const Slot&s)const{return {arena.data()+s.val_off,s.val_len};}
        Slot append(std::uint64_t fp,const std::string&k,const std::string&v){Slot s;s.fp=fp;s.used=1;s.key_off=arena.size();s.key_len=k.size();arena.insert(arena.end(),k.begin(),k.end());s.val_off=arena.size();s.val_len=v.size();arena.insert(arena.end(),v.begin(),v.end());return s;}
        void compact(){std::vector<char> na;na.reserve(live_bytes);for(auto&s:slots)if(s.used){auto k=keyv(s);auto v=std::string_view(arena.data()+s.val_off,s.val_len);s.key_off=na.size();na.insert(na.end(),k.begin(),k.end());s.val_off=na.size();na.insert(na.end(),v.begin(),v.end());}arena.swap(na);}
        void maybe_compact(){if(arena.size()>1024*1024 && arena.size()>live_bytes*2)compact();}
        void rehash(std::size_t n){auto old=std::move(slots);slots.assign(np2(n),Slot{});records=0;for(auto&s:old)if(s.used){std::size_t mask=slots.size()-1,idx=start(s.fp,mask);Slot cur=s;cur.dist=0;for(;;){auto&x=slots[idx];if(!x.used){x=cur;++records;break;}if(x.dist<cur.dist)std::swap(x,cur);idx=(idx+1)&mask;++cur.dist;}}}
        bool put(std::uint64_t fp,const std::string&k,const std::string&v,double ml){std::unique_lock lk(mu);if(double(records+1)/slots.size()>ml)rehash(slots.size()*2);std::size_t mask=slots.size()-1,idx=start(fp,mask);std::uint16_t d=0;for(;;){auto&x=slots[idx];if(!x.used)break;if(x.fp==fp&&keyv(x)==k){live_bytes-=x.key_len+x.val_len;Slot ns=append(fp,k,v);ns.dist=x.dist;x=ns;live_bytes+=k.size()+v.size();maybe_compact();return false;}if(x.dist<d)break;idx=(idx+1)&mask;++d;}Slot cur=append(fp,k,v);live_bytes+=k.size()+v.size();for(;;){auto&x=slots[idx];if(!x.used){cur.dist=d;x=cur;++records;maybe_compact();return true;}if(x.dist<d){cur.dist=d;std::swap(x,cur);d=cur.dist;}idx=(idx+1)&mask;++d;}}
        std::optional<std::string> get(std::uint64_t fp,const std::string&k)const{std::shared_lock lk(mu);std::size_t mask=slots.size()-1,idx=start(fp,mask);std::uint16_t d=0;for(;;){const auto&s=slots[idx];if(!s.used||s.dist<d)return std::nullopt;if(s.fp==fp&&keyv(s)==k)return val(s);idx=(idx+1)&mask;if(++d>=slots.size())return std::nullopt;}}
        std::size_t ab()const{std::shared_lock lk(mu);return arena.size();} std::size_t lb()const{std::shared_lock lk(mu);return live_bytes;}
    };
    std::vector<std::unique_ptr<Part>> p_;std::size_t n_=0;double ml_=.78;
public:
    ReclaimCompactIndex(std::size_t n=4096){p_.reserve(n);for(std::size_t i=0;i<n;++i)p_.push_back(std::make_unique<Part>());}
    void put(const std::string&k,const std::string&v){auto h1=mix64(fnv1a64(k));auto h2=mix64(fnv1a64(k,1099511628211ULL)^(h1<<1));if(p_[h1%p_.size()]->put(h2,k,v,ml_))++n_;}
    std::optional<std::string> get(const std::string&k)const{auto h1=mix64(fnv1a64(k));auto h2=mix64(fnv1a64(k,1099511628211ULL)^(h1<<1));return p_[h1%p_.size()]->get(h2,k);}
    std::size_t arena_bytes()const{std::size_t x=0;for(auto&p:p_)x+=p->ab();return x;}std::size_t live_bytes()const{std::size_t x=0;for(auto&p:p_)x+=p->lb();return x;}std::size_t size()const{return n_;}
};

int main(){const std::size_t n=std::strtoull(std::getenv("BDR_RECLAIM_RECORDS")?std::getenv("BDR_RECLAIM_RECORDS"):"100000",nullptr,10);const std::size_t updates=std::strtoull(std::getenv("BDR_RECLAIM_UPDATES")?std::getenv("BDR_RECLAIM_UPDATES"):"5000000",nullptr,10);const std::size_t vb=16;ReclaimCompactIndex idx;std::string v(vb,'x');auto t0=Clock::now();for(std::size_t i=0;i<n;++i)idx.put("k"+std::to_string(i),v);for(std::size_t i=0;i<updates;++i){std::string nv(vb,char('a'+i%26));idx.put("k"+std::to_string(i%n),nv);}for(std::size_t i=0;i<10000;++i){auto x=idx.get("k"+std::to_string((i*9973)%n));if(!x)throw std::runtime_error("verify");}auto sec=std::chrono::duration<double>(Clock::now()-t0).count();std::cout<<"RECLAIM_COMPACT PASS records="<<idx.size()<<" updates="<<updates<<" arena_bytes="<<idx.arena_bytes()<<" live_bytes="<<idx.live_bytes()<<" amplification="<<(double(idx.arena_bytes())/std::max<std::size_t>(1,idx.live_bytes()))<<" seconds="<<sec<<"\n";}
