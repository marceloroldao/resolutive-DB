from pathlib import Path

src_path = Path("experimental/api_v86/src/database.cpp")
out_path = Path("experimental/v03_scale/generated_database_streaming.cpp")
src = src_path.read_text()

anchor = """static uint64_t replay_snapshot(const fs::path&p,ResolutiveIndex&index){"""
helper = r'''static void write_snapshot_streaming_fd(int fd,uint64_t seq,const std::vector<std::pair<std::string,std::string>>&items){
    uint32_t crc=0;
    auto emit=[&](const void* p,std::size_t n){
        if(!n)return;
        write_all(fd,p,n);
        crc=crc32b(p,n,crc);
    };
    std::vector<uint8_t> h;
    h.insert(h.end(),{'B','D','R','3'});put32(h,3);put64(h,seq);put64(h,uint64_t(items.size()));
    emit(h.data(),h.size());
    for(const auto&[k,v]:items){
        std::vector<uint8_t> lens;lens.reserve(8);put32(lens,uint32_t(k.size()));put32(lens,uint32_t(v.size()));
        emit(lens.data(),lens.size());emit(k.data(),k.size());emit(v.data(),v.size());
    }
    std::vector<uint8_t> tail;tail.reserve(4);put32(tail,crc);write_all(fd,tail.data(),tail.size());
}

'''
if anchor not in src:
    raise SystemExit("streaming helper anchor not found")
src = src.replace(anchor, helper + anchor, 1)

old = """uint64_t seq=next_seq.load();wait_impl(Ticket{seq});auto items=index.snapshot_items();auto bytes=encode_snapshot(seq,items);
        fs::path tmp=dir/\"snapshot.tmp\",dst=dir/\"snapshot.bdr3\";int sfd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(sfd<0)throw std::runtime_error(\"snapshot temp open failed\");
        write_all(sfd,bytes.data(),bytes.size());if(::fsync(sfd)){::close(sfd);throw std::runtime_error(\"snapshot fsync failed\");}"""
new = """uint64_t seq=next_seq.load();wait_impl(Ticket{seq});auto items=index.snapshot_items();
        fs::path tmp=dir/\"snapshot.tmp\",dst=dir/\"snapshot.bdr3\";int sfd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(sfd<0)throw std::runtime_error(\"snapshot temp open failed\");
        write_snapshot_streaming_fd(sfd,seq,items);if(::fsync(sfd)){::close(sfd);throw std::runtime_error(\"snapshot fsync failed\");}"""
if old not in src:
    raise SystemExit("checkpoint replacement anchor not found")
src = src.replace(old,new,1)

out_path.write_text(src)
print(f"generated {out_path} from {src_path}")
