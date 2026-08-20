from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: stream_snapshot.py <input.cpp> <output.cpp>")

src = Path(sys.argv[1]).read_text()

old = '''struct SnapshotState{uint64_t seq=0;std::vector<std::pair<std::string,std::string>>items;};
static SnapshotState decode_snapshot(const fs::path&p){
    std::ifstream f(p,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});if(b.size()<28)throw std::runtime_error("BDR3 snapshot too short");uint32_t got=be32(b.data()+b.size()-4);if(crc32b(b.data(),b.size()-4)!=got)throw std::runtime_error("BDR3 snapshot CRC mismatch");const uint8_t*q=b.data(),*end=b.data()+b.size()-4;if(std::memcmp(q,"BDR3",4))throw std::runtime_error("BDR3 magic mismatch");q+=4;if(be32(q)!=3)throw std::runtime_error("BDR3 version unsupported");q+=4;SnapshotState s;s.seq=be64(q);q+=8;uint64_t n=be64(q);q+=8;if(n>50'000'000ULL)throw std::runtime_error("BDR3 count unreasonable");s.items.reserve(std::size_t(n));for(uint64_t i=0;i<n;++i){if(end-q<8)throw std::runtime_error("BDR3 record header truncated");uint32_t kl=be32(q);q+=4;uint32_t vl=be32(q);q+=4;if(!kl||kl>(1u<<20)||vl>(1u<<24)||uint64_t(end-q)<uint64_t(kl)+vl)throw std::runtime_error("BDR3 record bounds invalid");std::string k((const char*)q,kl);q+=kl;std::string v((const char*)q,vl);q+=vl;s.items.emplace_back(std::move(k),std::move(v));}if(q!=end)throw std::runtime_error("BDR3 trailing bytes");return s;
}
'''

new = r'''static uint64_t replay_snapshot(const fs::path&p,ResolutiveIndex&index){
    const auto total=fs::file_size(p);
    if(total<28)throw std::runtime_error("BDR3 snapshot too short");

    // Pass 1: validate the complete snapshot CRC before mutating the live index.
    {
        std::ifstream f(p,std::ios::binary);
        if(!f)throw std::runtime_error("BDR3 snapshot open failed");
        uint64_t remaining=total-4;
        uint32_t crc=0;
        std::vector<uint8_t>chunk(64*1024);
        while(remaining){
            const std::size_t want=std::size_t(std::min<uint64_t>(remaining,chunk.size()));
            f.read(reinterpret_cast<char*>(chunk.data()),std::streamsize(want));
            if(std::size_t(f.gcount())!=want)throw std::runtime_error("BDR3 snapshot truncated during CRC pass");
            crc=crc32b(chunk.data(),want,crc);
            remaining-=want;
        }
        uint8_t tail[4];
        f.read(reinterpret_cast<char*>(tail),4);
        if(f.gcount()!=4)throw std::runtime_error("BDR3 snapshot CRC truncated");
        if(be32(tail)!=crc)throw std::runtime_error("BDR3 snapshot CRC mismatch");
    }

    // Pass 2: parse one record at a time directly into the partitioned index.
    std::ifstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("BDR3 snapshot reopen failed");
    uint8_t header[24];
    f.read(reinterpret_cast<char*>(header),24);
    if(f.gcount()!=24)throw std::runtime_error("BDR3 header truncated");
    if(std::memcmp(header,"BDR3",4))throw std::runtime_error("BDR3 magic mismatch");
    if(be32(header+4)!=3)throw std::runtime_error("BDR3 version unsupported");
    const uint64_t seq=be64(header+8);
    const uint64_t n=be64(header+16);
    if(n>50'000'000ULL)throw std::runtime_error("BDR3 count unreasonable");

    uint64_t consumed=24;
    for(uint64_t i=0;i<n;++i){
        uint8_t lens[8];
        f.read(reinterpret_cast<char*>(lens),8);
        if(f.gcount()!=8)throw std::runtime_error("BDR3 record header truncated");
        consumed+=8;
        const uint32_t kl=be32(lens);
        const uint32_t vl=be32(lens+4);
        if(!kl||kl>(1u<<20)||vl>(1u<<24))throw std::runtime_error("BDR3 record bounds invalid");
        if(consumed+uint64_t(kl)+uint64_t(vl)>total-4)throw std::runtime_error("BDR3 record exceeds snapshot bounds");
        std::string k(kl,'\0'),v(vl,'\0');
        f.read(k.data(),std::streamsize(kl));
        if(std::size_t(f.gcount())!=kl)throw std::runtime_error("BDR3 key truncated");
        if(vl){f.read(v.data(),std::streamsize(vl));if(std::size_t(f.gcount())!=vl)throw std::runtime_error("BDR3 value truncated");}
        consumed+=uint64_t(kl)+uint64_t(vl);
        index.put(k,v);
    }
    if(consumed!=total-4)throw std::runtime_error("BDR3 trailing bytes");
    return seq;
}
'''

if src.count(old) != 1:
    raise SystemExit(f"decode_snapshot block match count={src.count(old)}")
src = src.replace(old, new)

old_recover = 'if(fs::exists(snap)){auto s=decode_snapshot(snap);seq=s.seq;for(auto&[k,v]:s.items)index.put(k,v);}'
new_recover = 'if(fs::exists(snap)){seq=replay_snapshot(snap,index);}'
if src.count(old_recover) != 1:
    raise SystemExit(f"recover call match count={src.count(old_recover)}")
src = src.replace(old_recover, new_recover)

Path(sys.argv[2]).parent.mkdir(parents=True,exist_ok=True)
Path(sys.argv[2]).write_text(src)
print(f"streaming snapshot patch: {sys.argv[1]} -> {sys.argv[2]}")
