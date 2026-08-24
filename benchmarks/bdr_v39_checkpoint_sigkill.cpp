#define main v38_unused_main
#include "bdr_v38_checkpoint_bdr3.cpp"
#undef main
#include <csignal>
#include <sys/wait.h>

static void die_at(int phase,int target){if(phase==target){::kill(::getpid(),SIGKILL);::_exit(137);}}
static void checkpoint_killable(const fs::path&dir,uint64_t seq,const std::map<std::string,std::string>&kv,uint64_t next_seg,int target){
    int phase=1;fs::path active=dir/"wal-000000.log";int afd=::open(active.c_str(),O_RDONLY);if(afd>=0){if(::fsync(afd))throw std::runtime_error("wal sync");::close(afd);}die_at(phase++,target);
    auto bytes=encode_snapshot(seq,kv);fs::path tmp=dir/"snapshot.tmp",dst=dir/"snapshot.bdr3";int fd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("tmp");xwrite(fd,bytes.data(),bytes.size());if(::fsync(fd))throw std::runtime_error("tmp sync");::close(fd);die_at(phase++,target);
    fs::rename(tmp,dst);fsync_dir(dir);die_at(phase++,target);
    fs::path nw=dir/"wal-000001.log";create_wal(nw,next_seg,seq+1);fsync_dir(dir);die_at(phase++,target);
    if(fs::exists(active))fs::remove(active);fsync_dir(dir);die_at(phase++,target);
}
static void prepare(const fs::path&d,std::map<std::string,std::string>&kv,uint64_t&seq){fs::remove_all(d);fs::create_directory(d);create_wal(d/"wal-000000.log",1,1);seq=0;for(int i=0;i<5000;++i){auto k="K"+std::to_string(i),v="V"+std::to_string(i);kv[k]=v;append_wal(d/"wal-000000.log",++seq,1,k,v);}}
int main(){std::cout<<"kill_phase,signal_seen,records,last_seq,pass\n";int fail=0;for(int target=1;target<=5;++target){fs::path d="v39db_"+std::to_string(target);std::map<std::string,std::string>kv;uint64_t seq=0;prepare(d,kv,seq);pid_t p=fork();if(p==0){checkpoint_killable(d,seq,kv,2,target);::_exit(0);}int st=0;waitpid(p,&st,0);bool sig=WIFSIGNALED(st)&&WTERMSIG(st)==SIGKILL;bool ok=false;size_t n=0;uint64_t rs=0;try{auto r=reopen(d);n=r.kv.size();rs=r.seq;ok=sig&&r.seq==seq&&r.kv==kv;}catch(...){ok=false;}std::cout<<target<<','<<sig<<','<<n<<','<<rs<<','<<ok<<"\n";fail+=!ok;}return fail?2:0;}
