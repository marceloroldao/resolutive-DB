#include "bdr/resolutive_index.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <mutex>

using Clock=std::chrono::steady_clock;

static inline std::uint64_t mix64(std::uint64_t x) noexcept { x+=0x9e3779b97f4a7c15ULL; x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL; x=(x^(x>>27))*0x94d049bb133111ebULL; return x^(x>>31); }
static inline std::uint64_t fnv1a64(const std::string&s,std::uint64_t seed=1469598103934665603ULL) noexcept { std::uint64_t h=seed; for(unsigned char c:s){h^=c;h*=1099511628211ULL;} return h; }
static std::size_t np2(std::size_t n){std::size_t p=8;while(p<n)p<<=1;return p;}

class CompactIndex {
    struct Slot { std::uint64_t fp=0; std::uint32_t key_off=0,val_off=0; std::uint32_t key_len=0,val_len=0; std::uint16_t dist=0; std::uint8_t used=0; };
    struct Part {
        mutable std::shared_mutex mu;
        std::vector<Slot> slots;
        std::vector<char> arena;
        std::size_t records=0;
        std::size_t garbage_bytes=0;
        std::size_t compactions=0;
        bool reclaim=false;
        explicit Part(bool r=false):slots(8),reclaim(r){}
        static std::size_t start(std::uint64_t fp,std::size_t mask){return std::size_t(mix64(fp))&mask;}
        std::string_view key_view(const Slot&s) const {return {arena.data()+s.key_off,s.key_len};}
        std::string value_copy(const Slot&s) const {return std::string(arena.data()+s.val_off,s.val_len);}
        Slot make(std::uint64_t fp,const std::string&k,const std::string&v){Slot s; s.fp=fp;s.used=1;s.key_off=arena.size();s.key_len=k.size();arena.insert(arena.end(),k.begin(),k.end());s.val_off=arena.size();s.val_len=v.size();arena.insert(arena.end(),v.begin(),v.end());return s;}
        void compact_arena(){
            std::vector<char> next;
            next.reserve(arena.size()-std::min(garbage_bytes,arena.size()));
            for(auto &s:slots) if(s.used){
                const auto ko=s.key_off, vo=s.val_off, kl=s.key_len, vl=s.val_len;
                s.key_off=static_cast<std::uint32_t>(next.size());
                next.insert(next.end(),arena.begin()+ko,arena.begin()+ko+kl);
                s.val_off=static_cast<std::uint32_t>(next.size());
                next.insert(next.end(),arena.begin()+vo,arena.begin()+vo+vl);
            }
            arena.swap(next);
            garbage_bytes=0;
            ++compactions;
        }
        void maybe_compact(){
            if(reclaim && garbage_bytes>=8192 && garbage_bytes*2>=arena.size()) compact_arena();
        }
        bool ins(std::uint64_t fp,const std::string&k,const std::string&v){
            std::size_t mask=slots.size()-1,idx=start(fp,mask);Slot cur=make(fp,k,v);
            for(;;){
                auto&x=slots[idx];
                if(!x.used){x=cur;++records;return true;}
                if(x.fp==fp&&key_view(x)==k){garbage_bytes+=x.key_len+x.val_len;x=cur;maybe_compact();return false;}
                if(x.dist<cur.dist)std::swap(x,cur);
                idx=(idx+1)&mask;
                if(++cur.dist>=slots.size())throw std::runtime_error("overflow");
            }
        }
        void rehash(std::size_t n){auto old=std::move(slots);slots.assign(np2(n),Slot{});records=0;for(auto&s:old)if(s.used){std::size_t mask=slots.size()-1,idx=start(s.fp,mask);Slot cur=s;cur.dist=0;for(;;){auto&x=slots[idx];if(!x.used){x=cur;++records;break;}if(x.dist<cur.dist)std::swap(x,cur);idx=(idx+1)&mask;++cur.dist;}}}
        bool put(std::uint64_t fp,const std::string&k,const std::string&v,double ml){std::unique_lock lk(mu);if(double(records+1)/slots.size()>ml)rehash(slots.size()*2);return ins(fp,k,v);}
        std::optional<std::string> get(std::uint64_t fp,const std::string&k)const{std::shared_lock lk(mu);std::size_t mask=slots.size()-1,idx=start(fp,mask);std::uint16_t d=0;for(;;){const auto&s=slots[idx];if(!s.used||s.dist<d)return std::nullopt;if(s.fp==fp&&key_view(s)==k)return value_copy(s);idx=(idx+1)&mask;if(++d>=slots.size())return std::nullopt;}}
        std::size_t arena_bytes() const { std::shared_lock lk(mu); return arena.size(); }
        std::size_t garbage() const { std::shared_lock lk(mu); return garbage_bytes; }
        std::size_t gc_count() const { std::shared_lock lk(mu); return compactions; }
    };
    std::vector<std::unique_ptr<Part>> p_; double ml_=.78; std::size_t n_=0;
public:
    explicit CompactIndex(std::size_t n=4096,bool reclaim=false){p_.reserve(n);for(std::size_t i=0;i<n;++i)p_.push_back(std::make_unique<Part>(reclaim));}
    void put(const std::string&k,const std::string&v){auto h1=mix64(fnv1a64(k));auto h2=mix64(fnv1a64(k,1099511628211ULL)^(h1<<1));if(p_[h1%p_.size()]->put(h2,k,v,ml_))++n_;}
    std::optional<std::string> get(const std::string&k)const{auto h1=mix64(fnv1a64(k));auto h2=mix64(fnv1a64(k,1099511628211ULL)^(h1<<1));return p_[h1%p_.size()]->get(h2,k);}
    std::size_t size()const{return n_;}
    std::size_t arena_bytes() const { std::size_t t=0; for(const auto& p:p_) t+=p->arena_bytes(); return t; }
    std::size_t garbage_bytes() const { std::size_t t=0; for(const auto& p:p_) t+=p->garbage(); return t; }
    std::size_t compactions() const { std::size_t t=0; for(const auto& p:p_) t+=p->gc_count(); return t; }
};

int main(){
    const char* m=std::getenv("BDR_COMPACT_MODE");std::string mode=m?m:"baseline";
    const std::size_t n=std::strtoull(std::getenv("BDR_COMPACT_RECORDS")?std::getenv("BDR_COMPACT_RECORDS"):"1000000",nullptr,10);
    const std::size_t vb=std::strtoull(std::getenv("BDR_COMPACT_VALUE_BYTES")?std::getenv("BDR_COMPACT_VALUE_BYTES"):"16",nullptr,10);
    const std::size_t updates=std::strtoull(std::getenv("BDR_COMPACT_UPDATES")?std::getenv("BDR_COMPACT_UPDATES"):"0",nullptr,10);
    std::string value(vb,'x');auto t0=Clock::now();
    if(mode=="baseline"){
        bdr::ResolutiveIndex idx(4096,.78);for(std::size_t i=0;i<n;++i)idx.put("k"+std::to_string(i),value);
        for(std::size_t i=0;i<updates;++i){std::string v(vb,char('a'+(i%26)));idx.put("k"+std::to_string(i%n),v);}
        for(std::size_t i=0;i<10000;++i){auto x=idx.get("k"+std::to_string((i*9973)%n));if(!x)throw std::runtime_error("baseline verify");}
        std::cout<<"COMPACT_ABLATION PASS mode=baseline records="<<idx.size()<<" value_bytes="<<vb<<" updates="<<updates;
    } else {
        const bool reclaim=(mode=="compact_gc");
        CompactIndex idx(4096,reclaim);for(std::size_t i=0;i<n;++i)idx.put("k"+std::to_string(i),value);
        for(std::size_t i=0;i<updates;++i){std::string v(vb,char('a'+(i%26)));idx.put("k"+std::to_string(i%n),v);}
        for(std::size_t i=0;i<10000;++i){auto x=idx.get("k"+std::to_string((i*9973)%n));if(!x)throw std::runtime_error("compact verify");}
        std::cout<<"COMPACT_ABLATION PASS mode="<<mode<<" records="<<idx.size()<<" value_bytes="<<vb<<" updates="<<updates<<" arena_bytes="<<idx.arena_bytes()<<" garbage_bytes="<<idx.garbage_bytes()<<" compactions="<<idx.compactions();
    }
    auto t1=Clock::now();std::cout<<" seconds="<<std::chrono::duration<double>(t1-t0).count()<<"\n";
}
