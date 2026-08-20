#define main v38_original_main
#include "bdr_v38_checkpoint_bdr3.cpp"
#undef main

#pragma pack(push,1)
struct Wal3Header{char magic[4];uint16_t version;uint16_t header_size;uint32_t flags;uint64_t segment_id;uint64_t first_sequence;uint32_t crc;};
#pragma pack(pop)

static uint32_t wal3_hcrc(const Wal3Header&h){return crc_bytes(&h,sizeof(h)-4);}
static uint32_t be32v(const uint8_t*p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
static uint64_t be64v(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|p[i];return v;}

static void create_wal3(const fs::path&p,uint64_t seg,uint64_t first){
    Wal3Header h{};std::memcpy(h.magic,"BDW3",4);h.version=3;h.header_size=sizeof(h);h.segment_id=seg;h.first_sequence=first;h.crc=wal3_hcrc(h);
    int fd=::open(p.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("create wal3");xwrite(fd,&h,sizeof(h));if(::fdatasync(fd))throw std::runtime_error("wal3 sync");::close(fd);
}

static std::vector<uint8_t> make_record3(uint64_t seq,uint8_t op,const std::string&k,const std::string&v){
    std::vector<uint8_t>h;put_u64(h,seq);h.push_back(op);put_u32(h,uint32_t(k.size()));put_u32(h,uint32_t(v.size()));uint32_t hc=crc_bytes(h.data(),h.size());
    uint32_t total=4+uint32_t(h.size())+4+uint32_t(k.size()+v.size())+4;std::vector<uint8_t>b;put_u32(b,total);b.insert(b.end(),h.begin(),h.end());put_u32(b,hc);b.insert(b.end(),k.begin(),k.end());b.insert(b.end(),v.begin(),v.end());put_u32(b,crc_bytes(b.data(),b.size()));return b;
}
static void append_wal3(const fs::path&p,uint64_t seq,uint8_t op,const std::string&k,const std::string&v){auto b=make_record3(seq,op,k,v);int fd=::open(p.c_str(),O_WRONLY|O_APPEND);if(fd<0)throw std::runtime_error("open wal3");xwrite(fd,b.data(),b.size());if(::fdatasync(fd))throw std::runtime_error("wal3 append sync");::close(fd);}

static void replay_wal3(const fs::path&p,SnapInfo&s){
    std::ifstream f(p,std::ios::binary);Wal3Header wh{};f.read((char*)&wh,sizeof(wh));if(f.gcount()!=sizeof(wh)||std::memcmp(wh.magic,"BDW3",4)||wh.version!=3||wh.header_size!=sizeof(wh)||wal3_hcrc(wh)!=wh.crc)throw std::runtime_error("wal3 header");
    uint64_t expected=std::max<uint64_t>(s.seq+1,wh.first_sequence);
    for(;;){uint8_t lb[4];f.read((char*)lb,4);if(f.gcount()==0)break;if(f.gcount()!=4)break;uint32_t total=be32v(lb);constexpr uint32_t MIN=4+8+1+4+4+4+4;if(total<MIN||total>(1u<<24))throw std::runtime_error("wal3 total");std::vector<uint8_t>b(total-4);f.read((char*)b.data(),std::streamsize(b.size()));if(f.gcount()!=std::streamsize(b.size()))break;
        const uint8_t*q=b.data();uint64_t seq=be64v(q);q+=8;uint8_t op=*q++;uint32_t kl=be32v(q);q+=4;uint32_t vl=be32v(q);q+=4;uint32_t hc=be32v(q);q+=4;
        std::vector<uint8_t>hh;put_u64(hh,seq);hh.push_back(op);put_u32(hh,kl);put_u32(hh,vl);if(crc_bytes(hh.data(),hh.size())!=hc)throw std::runtime_error("wal3 header crc");if(op!=1&&op!=2)throw std::runtime_error("wal3 op");if(uint64_t(kl)+vl+4!=uint64_t(b.data()+b.size()-q))throw std::runtime_error("wal3 bounds");
        const uint8_t*payload=q;std::string k((const char*)payload,kl);payload+=kl;std::string v((const char*)payload,vl);uint32_t got=be32v(b.data()+b.size()-4);std::vector<uint8_t>whole;whole.insert(whole.end(),lb,lb+4);whole.insert(whole.end(),b.begin(),b.end()-4);if(crc_bytes(whole.data(),whole.size())!=got)throw std::runtime_error("wal3 record crc");
        if(seq<=s.seq)continue;if(seq!=expected)throw std::runtime_error("wal3 sequence");if(op==1)s.kv[k]=v;else s.kv.erase(k);s.seq=seq;++expected;
    }
}

static void atomic_checkpoint3(const fs::path&dir,const std::string&active,const std::string&next,uint64_t seq,const std::map<std::string,std::string>&kv,uint64_t next_seg){
    fs::path activep=dir/active;int afd=::open(activep.c_str(),O_RDONLY);if(afd>=0){if(::fsync(afd))throw std::runtime_error("pre checkpoint sync");::close(afd);}auto bytes=encode_snapshot(seq,kv);
    fs::path tmp=dir/"snapshot.tmp",dst=dir/"snapshot.bdr3";int fd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("snapshot tmp");xwrite(fd,bytes.data(),bytes.size());if(::fsync(fd))throw std::runtime_error("snapshot sync");::close(fd);fs::rename(tmp,dst);fsync_dir(dir);
    create_wal3(dir/next,next_seg,seq+1);fsync_dir(dir);if(fs::exists(activep))fs::remove(activep);fsync_dir(dir);
}
static SnapInfo reopen3(const fs::path&dir){SnapInfo s;if(fs::exists(dir/"snapshot.bdr3"))s=decode_snapshot(dir/"snapshot.bdr3");std::vector<fs::path>w;for(auto&e:fs::directory_iterator(dir))if(e.path().extension()==".bdw3")w.push_back(e.path());std::sort(w.begin(),w.end());for(auto&p:w)replay_wal3(p,s);return s;}

int main(){
    fs::path d="v48db";fs::remove_all(d);fs::create_directory(d);std::string active="wal-000000.bdw3";create_wal3(d/active,1,1);std::map<std::string,std::string>kv;uint64_t seq=0;
    for(int i=0;i<10000;++i){auto k="K"+std::to_string(i),v="V"+std::to_string(i);kv[k]=v;append_wal3(d/active,++seq,1,k,v);}for(int i=0;i<750;++i){auto k="K"+std::to_string(i);kv.erase(k);append_wal3(d/active,++seq,2,k,"");}
    std::string next="wal-000001.bdw3";atomic_checkpoint3(d,active,next,seq,kv,2);active=next;
    for(int i=0;i<1500;++i){auto k="P"+std::to_string(i),v="Q"+std::to_string(i);kv[k]=v;append_wal3(d/active,++seq,1,k,v);}for(int i=1000;i<1250;++i){auto k="K"+std::to_string(i);kv.erase(k);append_wal3(d/active,++seq,2,k,"");}
    auto r=reopen3(d);bool ok=r.seq==seq&&r.kv==kv&&fs::exists(d/"snapshot.bdr3")&&!fs::exists(d/"snapshot.tmp")&&!fs::exists(d/"wal-000000.bdw3")&&fs::exists(d/"wal-000001.bdw3");
    std::cout<<"records_expected,records_recovered,last_seq_expected,last_seq_recovered,old_wal_retired,new_wal_present,pass\n"<<kv.size()<<','<<r.kv.size()<<','<<seq<<','<<r.seq<<','<<(!fs::exists(d/"wal-000000.bdw3"))<<','<<fs::exists(d/"wal-000001.bdw3")<<','<<ok<<"\n";return ok?0:2;
}
