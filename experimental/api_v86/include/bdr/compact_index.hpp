#pragma once

#include "bdr/database.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bdr {
namespace compact_index_detail {
static inline std::uint64_t mix64(std::uint64_t x) noexcept { x+=0x9e3779b97f4a7c15ULL; x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL; x=(x^(x>>27))*0x94d049bb133111ebULL; return x^(x>>31); }
static inline std::uint64_t fnv1a64(const std::string& s,std::uint64_t seed=1469598103934665603ULL) noexcept { std::uint64_t h=seed; for(unsigned char c:s){h^=c;h*=1099511628211ULL;} return h; }
static inline std::size_t next_pow2(std::size_t n){std::size_t p=8;while(p<n)p<<=1;return p;}
}

class CompactIndex {
    struct Slot { std::uint64_t fingerprint=0; std::uint32_t key_off=0,value_off=0,key_len=0,value_len=0,dist=0; bool used=false; };
    struct Partition {
        mutable std::shared_mutex mu; std::vector<Slot> slots{8}; std::vector<char> arena; std::size_t records=0,garbage_bytes=0,compactions=0; bool reclaim=true;
        std::string_view key_view(const Slot&s)const{return {arena.data()+s.key_off,s.key_len};}
        std::string key_copy(const Slot&s)const{return std::string(arena.data()+s.key_off,s.key_len);}
        std::string value_copy(const Slot&s)const{return std::string(arena.data()+s.value_off,s.value_len);}
        static std::size_t start(std::uint64_t fp,std::size_t mask)noexcept{return std::size_t(compact_index_detail::mix64(fp))&mask;}
        Slot append(std::uint64_t fp,const std::string&k,const std::string&v){if(arena.size()+k.size()+v.size()>0xffffffffULL)throw std::runtime_error("compact arena offset overflow");Slot s;s.fingerprint=fp;s.used=true;s.key_off=static_cast<std::uint32_t>(arena.size());s.key_len=static_cast<std::uint32_t>(k.size());arena.insert(arena.end(),k.begin(),k.end());s.value_off=static_cast<std::uint32_t>(arena.size());s.value_len=static_cast<std::uint32_t>(v.size());arena.insert(arena.end(),v.begin(),v.end());return s;}
        void compact_arena(){std::vector<char> next;next.reserve(arena.size()-std::min(arena.size(),garbage_bytes));for(auto&s:slots)if(s.used){auto k=key_copy(s);auto v=value_copy(s);s.key_off=static_cast<std::uint32_t>(next.size());next.insert(next.end(),k.begin(),k.end());s.value_off=static_cast<std::uint32_t>(next.size());next.insert(next.end(),v.begin(),v.end());}arena.swap(next);garbage_bytes=0;++compactions;}
        void maybe_compact(){if(reclaim&&garbage_bytes>=8192&&garbage_bytes*2>=arena.size())compact_arena();}
        bool insert_nolock(std::uint64_t fp,const std::string&k,const std::string&v){const auto mask=slots.size()-1;auto idx=start(fp,mask);Slot cur=append(fp,k,v);for(;;){auto&slot=slots[idx];if(!slot.used){slot=cur;++records;return true;}if(slot.fingerprint==fp&&key_view(slot)==k){garbage_bytes+=slot.key_len+slot.value_len;slot=cur;maybe_compact();return false;}if(slot.dist<cur.dist)std::swap(slot,cur);idx=(idx+1)&mask;if(++cur.dist>=slots.size())throw std::runtime_error("compact partition overflow");}}
        void rehash(std::size_t cap){auto old=std::move(slots);slots.assign(compact_index_detail::next_pow2(cap),Slot{});records=0;for(auto s:old)if(s.used){const auto mask=slots.size()-1;auto idx=start(s.fingerprint,mask);s.dist=0;for(;;){auto&slot=slots[idx];if(!slot.used){slot=s;++records;break;}if(slot.dist<s.dist)std::swap(slot,s);idx=(idx+1)&mask;++s.dist;}}}
        bool put(std::uint64_t fp,const std::string&k,const std::string&v,double max_load){std::unique_lock lk(mu);if(double(records+1)/double(slots.size())>max_load)rehash(slots.size()*2);return insert_nolock(fp,k,v);}
        std::optional<std::string> get(std::uint64_t fp,const std::string&k)const{std::shared_lock lk(mu);const auto mask=slots.size()-1;auto idx=start(fp,mask);std::uint32_t dist=0;for(;;){const auto&slot=slots[idx];if(!slot.used||slot.dist<dist)return std::nullopt;if(slot.fingerprint==fp&&key_view(slot)==k)return value_copy(slot);idx=(idx+1)&mask;if(++dist>=slots.size())return std::nullopt;}}
        bool erase(std::uint64_t fp,const std::string&k){std::unique_lock lk(mu);const auto mask=slots.size()-1;auto idx=start(fp,mask);std::uint32_t dist=0;for(;;){auto&slot=slots[idx];if(!slot.used||slot.dist<dist)return false;if(slot.fingerprint==fp&&key_view(slot)==k)break;idx=(idx+1)&mask;if(++dist>=slots.size())return false;}garbage_bytes+=slots[idx].key_len+slots[idx].value_len;auto hole=idx;auto next=(hole+1)&mask;while(slots[next].used&&slots[next].dist>0){slots[hole]=slots[next];--slots[hole].dist;hole=next;next=(next+1)&mask;}slots[hole]=Slot{};--records;maybe_compact();return true;}
    };
    struct Address{std::uint32_t rho;std::uint64_t fingerprint;}; std::size_t partition_count_;double max_load_;std::vector<std::unique_ptr<Partition>>parts_;std::atomic<std::size_t>size_{0};
    Address encode(const std::string&k)const noexcept{const auto h1=compact_index_detail::mix64(compact_index_detail::fnv1a64(k));const auto h2=compact_index_detail::mix64(compact_index_detail::fnv1a64(k,1099511628211ULL)^(h1<<1));return {std::uint32_t(h1%partition_count_),h2};}
public:
    CompactIndex(std::size_t partitions,double max_load):partition_count_(partitions),max_load_(max_load){if(!partitions)throw std::invalid_argument("partition_count must be > 0");if(!(max_load>0.40&&max_load<0.95))throw std::invalid_argument("invalid max_load");parts_.reserve(partitions);for(std::size_t i=0;i<partitions;++i)parts_.push_back(std::make_unique<Partition>());}
    void put(const std::string&k,const std::string&v){auto a=encode(k);if(parts_[a.rho]->put(a.fingerprint,k,v,max_load_))size_.fetch_add(1,std::memory_order_acq_rel);}
    bool erase(const std::string&k){auto a=encode(k);if(!parts_[a.rho]->erase(a.fingerprint,k))return false;size_.fetch_sub(1,std::memory_order_acq_rel);return true;}
    std::optional<std::string> get(const std::string&k)const{auto a=encode(k);return parts_[a.rho]->get(a.fingerprint,k);}
    bool contains(const std::string&k)const{return get(k).has_value();}
    std::size_t size()const noexcept{return size_.load(std::memory_order_acquire);}
    std::vector<std::pair<std::string,std::string>> snapshot_items()const{std::vector<std::pair<std::string,std::string>>out;out.reserve(size());for(const auto&p:parts_){std::shared_lock lk(p->mu);for(const auto&s:p->slots)if(s.used)out.emplace_back(p->key_copy(s),p->value_copy(s));}std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.first<b.first;});return out;}
    IndexStats stats()const{IndexStats st;st.partitions=partition_count_;st.records=size();double sum=0;for(const auto&p:parts_){std::shared_lock lk(p->mu);st.slots+=p->slots.size();st.max_partition_records=std::max(st.max_partition_records,p->records);sum+=p->slots.empty()?0.0:double(p->records)/double(p->slots.size());}st.mean_load=partition_count_?sum/double(partition_count_):0.0;return st;}
};

} // namespace bdr
