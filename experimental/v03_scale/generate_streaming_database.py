from pathlib import Path

src_path = Path("experimental/api_v86/src/database.cpp")
out_path = Path("experimental/v03_scale/generated_database_streaming.cpp")
src = src_path.read_text()

# Test-only failpoints are inert unless BDR_CHECKPOINT_FAILPOINT is set.
# They let the recovery gate kill a process at exact checkpoint boundaries
# without changing the persistent BDR3/BDW3 format.
src = src.replace(
    "#include <condition_variable>\n",
    "#include <condition_variable>\n#include <cstdlib>\n#include <csignal>\n",
    1,
)

anchor = """static uint64_t replay_snapshot(const fs::path&p,ResolutiveIndex&index){"""
helper = r'''static bool checkpoint_failpoint_is(const char* phase){
    const char* p=std::getenv("BDR_CHECKPOINT_FAILPOINT");
    return p&&std::strcmp(p,phase)==0;
}
static void checkpoint_failpoint(const char* phase){
    if(checkpoint_failpoint_is(phase))::kill(::getpid(),SIGKILL);
}

static void write_snapshot_streaming_fd(int fd,uint64_t seq,const std::vector<std::pair<std::string,std::string>>&items){
    uint32_t crc=0;
    auto emit=[&](const void* p,std::size_t n){
        if(!n)return;
        write_all(fd,p,n);
        crc=crc32b(p,n,crc);
    };
    std::vector<uint8_t> h;
    h.insert(h.end(),{'B','D','R','3'});put32(h,3);put64(h,seq);put64(h,uint64_t(items.size()));
    emit(h.data(),h.size());
    const bool crash_mid=checkpoint_failpoint_is("mid_snapshot");
    std::size_t emitted_items=0;
    for(const auto&[k,v]:items){
        std::vector<uint8_t> lens;lens.reserve(8);put32(lens,uint32_t(k.size()));put32(lens,uint32_t(v.size()));
        emit(lens.data(),lens.size());emit(k.data(),k.size());emit(v.data(),v.size());
        ++emitted_items;
        if(crash_mid&&emitted_items==std::max<std::size_t>(1,items.size()/2))::kill(::getpid(),SIGKILL);
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
        write_snapshot_streaming_fd(sfd,seq,items);checkpoint_failpoint(\"after_snapshot_write\");if(::fsync(sfd)){::close(sfd);throw std::runtime_error(\"snapshot fsync failed\");}checkpoint_failpoint(\"after_snapshot_fsync\");"""
if old not in src:
    raise SystemExit("checkpoint replacement anchor not found")
src = src.replace(old,new,1)

replacements = [
    (
        "::close(sfd);fs::rename(tmp,dst);fsync_dir(dir);",
        "::close(sfd);checkpoint_failpoint(\"after_snapshot_close\");fs::rename(tmp,dst);checkpoint_failpoint(\"after_rename\");fsync_dir(dir);checkpoint_failpoint(\"after_snapshot_dir_fsync\");",
    ),
    (
        "::close(wal_fd);wal_fd=-1;}",
        "::close(wal_fd);wal_fd=-1;checkpoint_failpoint(\"after_old_wal_close\");}",
    ),
    (
        "create_wal(segment_id+1,seq+1);fsync_dir(dir);",
        "create_wal(segment_id+1,seq+1);checkpoint_failpoint(\"after_new_wal_create\");fsync_dir(dir);checkpoint_failpoint(\"after_new_wal_dir_fsync\");",
    ),
    (
        "for(const auto&e:fs::directory_iterator(dir))if(e.is_regular_file()&&e.path().extension()==\".bdw3\"&&e.path()!=active_wal)fs::remove(e.path());\n        fsync_dir(dir);",
        "checkpoint_failpoint(\"before_old_wal_remove\");for(const auto&e:fs::directory_iterator(dir))if(e.is_regular_file()&&e.path().extension()==\".bdw3\"&&e.path()!=active_wal)fs::remove(e.path());checkpoint_failpoint(\"after_old_wal_remove\");\n        fsync_dir(dir);checkpoint_failpoint(\"after_old_wal_remove_dir_fsync\");",
    ),
]
for old_text,new_text in replacements:
    if old_text not in src:
        raise SystemExit(f"checkpoint failpoint anchor not found: {old_text[:40]}")
    src = src.replace(old_text,new_text,1)

out_path.write_text(src)
print(f"generated {out_path} from {src_path}")
