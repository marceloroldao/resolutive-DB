#define main compact_contract_parity_embedded_main
#include "compact_contract_parity.cpp"
#undef main

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <zlib.h>

namespace {

static uint32_t crc32b(const void* p, std::size_t n, uint32_t seed = 0) {
    return uint32_t(::crc32(seed, static_cast<const Bytef*>(p), uInt(n)));
}
static void put32(std::vector<uint8_t>& b, uint32_t v) { for (int i=3;i>=0;--i) b.push_back(uint8_t(v>>(8*i))); }
static void put64(std::vector<uint8_t>& b, uint64_t v) { for (int i=7;i>=0;--i) b.push_back(uint8_t(v>>(8*i))); }
static uint32_t be32(const uint8_t* p) { return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]); }
static uint64_t be64(const uint8_t* p) { uint64_t v=0; for(int i=0;i<8;++i) v=(v<<8)|p[i]; return v; }

#pragma pack(push,1)
struct Wal3Header { char magic[4]; uint16_t version; uint16_t header_size; uint32_t flags; uint64_t segment_id; uint64_t first_sequence; uint32_t crc; };
#pragma pack(pop)
static uint32_t wal_hcrc(const Wal3Header& h) { return crc32b(&h, sizeof(h)-4); }

static std::vector<uint8_t> encode_snapshot(uint64_t seq, const std::vector<std::pair<std::string,std::string>>& items) {
    std::vector<uint8_t> b; b.insert(b.end(), {'B','D','R','3'}); put32(b,3); put64(b,seq); put64(b,uint64_t(items.size()));
    for (const auto& [k,v] : items) { put32(b,uint32_t(k.size())); put32(b,uint32_t(v.size())); b.insert(b.end(),k.begin(),k.end()); b.insert(b.end(),v.begin(),v.end()); }
    put32(b,crc32b(b.data(),b.size())); return b;
}

static std::vector<uint8_t> make_record(uint64_t seq, bdr::OperationType type, const std::string& k, const std::string& v) {
    std::vector<uint8_t> h; put64(h,seq); h.push_back(uint8_t(type)); put32(h,uint32_t(k.size())); put32(h,uint32_t(v.size()));
    uint32_t hc=crc32b(h.data(),h.size()); uint32_t total=4+uint32_t(h.size())+4+uint32_t(k.size()+v.size())+4;
    std::vector<uint8_t>b; b.reserve(total); put32(b,total); b.insert(b.end(),h.begin(),h.end()); put32(b,hc); b.insert(b.end(),k.begin(),k.end()); b.insert(b.end(),v.begin(),v.end()); put32(b,crc32b(b.data(),b.size())); return b;
}

static std::vector<uint8_t> wal_header(uint64_t segment, uint64_t first) {
    Wal3Header h{}; std::memcpy(h.magic,"BDW3",4); h.version=3; h.header_size=sizeof(h); h.segment_id=segment; h.first_sequence=first; h.crc=wal_hcrc(h);
    const auto* p=reinterpret_cast<const uint8_t*>(&h); return std::vector<uint8_t>(p,p+sizeof(h));
}

template<class Index>
static uint64_t replay_snapshot_bytes(const std::vector<uint8_t>& b, Index& index) {
    if (b.size()<28 || std::memcmp(b.data(),"BDR3",4) || be32(b.data()+4)!=3) throw std::runtime_error("snapshot header");
    const uint32_t want=be32(b.data()+b.size()-4); if (crc32b(b.data(),b.size()-4)!=want) throw std::runtime_error("snapshot crc");
    const uint64_t seq=be64(b.data()+8), n=be64(b.data()+16); std::size_t pos=24;
    for(uint64_t i=0;i<n;++i){ if(pos+8>b.size()-4)throw std::runtime_error("snapshot truncated"); uint32_t kl=be32(b.data()+pos),vl=be32(b.data()+pos+4);pos+=8; if(!kl||pos+uint64_t(kl)+vl>b.size()-4)throw std::runtime_error("snapshot bounds"); std::string k((const char*)b.data()+pos,kl);pos+=kl;std::string v((const char*)b.data()+pos,vl);pos+=vl;index.put(k,v); }
    if(pos!=b.size()-4)throw std::runtime_error("snapshot trailing"); return seq;
}

template<class Index>
static std::pair<uint64_t,std::size_t> replay_wal_bytes(const std::vector<uint8_t>& b, Index& index, uint64_t seq) {
    if(b.size()<sizeof(Wal3Header))throw std::runtime_error("wal header short"); Wal3Header h{}; std::memcpy(&h,b.data(),sizeof(h));
    if(std::memcmp(h.magic,"BDW3",4)||h.version!=3||h.header_size!=sizeof(h)||wal_hcrc(h)!=h.crc)throw std::runtime_error("wal header invalid");
    std::size_t pos=sizeof(h),last_good=pos; uint64_t expected=std::max<uint64_t>(seq+1,h.first_sequence);
    while(pos<b.size()){
        if(pos+4>b.size())break; const uint32_t total=be32(b.data()+pos); constexpr uint32_t MIN=4+8+1+4+4+4+4;
        if(total<MIN||total>(1u<<24))throw std::runtime_error("wal total invalid"); if(pos+total>b.size())break;
        const uint8_t* q=b.data()+pos+4; const uint64_t rseq=be64(q);q+=8; const uint8_t op=*q++; const uint32_t kl=be32(q);q+=4; const uint32_t vl=be32(q);q+=4; const uint32_t hc=be32(q);q+=4;
        std::vector<uint8_t> hh;put64(hh,rseq);hh.push_back(op);put32(hh,kl);put32(hh,vl); if(crc32b(hh.data(),hh.size())!=hc)throw std::runtime_error("wal header crc");
        if(!kl||uint64_t(q-(b.data()+pos))+kl+vl+4!=total)throw std::runtime_error("wal bounds"); const uint32_t rc=be32(b.data()+pos+total-4); if(crc32b(b.data()+pos,total-4)!=rc)throw std::runtime_error("wal record crc");
        std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);
        if(rseq>seq){ if(rseq!=expected)throw std::runtime_error("wal sequence gap"); if(op==uint8_t(bdr::OperationType::Put))index.put(k,v); else if(op==uint8_t(bdr::OperationType::Delete))index.erase(k); else throw std::runtime_error("wal op"); seq=rseq;++expected; }
        pos+=total;last_good=pos;
    }
    return {seq,last_good};
}

static std::vector<std::pair<std::string,std::string>> oracle_items(const std::unordered_map<std::string,std::string>& o){ std::vector<std::pair<std::string,std::string>> v(o.begin(),o.end()); std::sort(v.begin(),v.end(),[](const auto&a,const auto&b){return a.first<b.first;});return v; }

} // namespace

int main(){
    constexpr std::size_t N=100000, OPS=250000; constexpr std::size_t PARTS=4096; constexpr double LOAD=.78;
    bdr::ResolutiveIndex seed(PARTS,LOAD); std::unordered_map<std::string,std::string> oracle; oracle.reserve(N*2); uint64_t seq=0;
    for(std::size_t i=0;i<N;++i){auto k=key_for(i);auto v=value_for(i,0);seed.put(k,v);oracle[k]=v;++seq;}
    const uint64_t snap_seq=seq; const auto snapshot=encode_snapshot(snap_seq,seed.snapshot_items()); auto wal=wal_header(7,snap_seq+1);
    for(std::size_t i=0;i<OPS;++i){const std::size_t id=(i*11400714819323198485ULL)%N;auto k=key_for(id);++seq;if(i%13==0){auto r=make_record(seq,bdr::OperationType::Delete,k,{});wal.insert(wal.end(),r.begin(),r.end());oracle.erase(k);}else{auto v=value_for(id,i+17);auto r=make_record(seq,bdr::OperationType::Put,k,v);wal.insert(wal.end(),r.begin(),r.end());oracle[k]=v;}}
    const uint64_t durable_seq=seq; const auto durable_oracle=oracle;
    auto torn=make_record(seq+1,bdr::OperationType::Put,key_for(17),value_for(17,999999)); wal.insert(wal.end(),torn.begin(),torn.begin()+torn.size()/2);

    bdr::ResolutiveIndex baseline(PARTS,LOAD); CompactIndex compact(PARTS,LOAD);
    auto sb=replay_snapshot_bytes(snapshot,baseline);auto sc=replay_snapshot_bytes(snapshot,compact); if(sb!=snap_seq||sc!=snap_seq)throw std::runtime_error("snapshot seq parity");
    auto rb=replay_wal_bytes(wal,baseline,sb);auto rc=replay_wal_bytes(wal,compact,sc); if(rb.first!=durable_seq||rc.first!=durable_seq)throw std::runtime_error("durable seq parity"); if(rb.second!=rc.second||rb.second>=wal.size())throw std::runtime_error("torn tail detection");
    const auto expected=oracle_items(durable_oracle); if(baseline.snapshot_items()!=expected)throw std::runtime_error("baseline persistence mismatch"); if(compact.snapshot_items()!=expected)throw std::runtime_error("compact persistence mismatch"); if(baseline.snapshot_items()!=compact.snapshot_items())throw std::runtime_error("baseline compact persistence parity");

    auto bad_snapshot=snapshot;bad_snapshot[bad_snapshot.size()/2]^=0x5a;bool snapshot_crc_rejected=false;try{CompactIndex x(PARTS,LOAD);replay_snapshot_bytes(bad_snapshot,x);}catch(...){snapshot_crc_rejected=true;}if(!snapshot_crc_rejected)throw std::runtime_error("corrupt snapshot accepted");
    auto bad_wal=wal; if(bad_wal.size()>sizeof(Wal3Header)+64){bad_wal[sizeof(Wal3Header)+40]^=0x33;bool wal_crc_rejected=false;try{CompactIndex x(PARTS,LOAD);auto s=replay_snapshot_bytes(snapshot,x);replay_wal_bytes(bad_wal,x,s);}catch(...){wal_crc_rejected=true;}if(!wal_crc_rejected)throw std::runtime_error("corrupt wal accepted");}

    std::cout<<"COMPACT_PERSISTENCE_PARITY PASS snapshot_records="<<N<<" wal_ops="<<OPS<<" final_records="<<expected.size()<<" durable_seq="<<durable_seq<<" torn_bytes="<<(wal.size()-rb.second)<<"\n";
}
