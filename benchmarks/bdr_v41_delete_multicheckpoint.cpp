#define main v38_original_main
#include "bdr_v38_checkpoint_bdr3.cpp"
#undef main

static void checkpoint_gen(const fs::path&dir,const std::string&active_name,const std::string&next_name,uint64_t seq,const std::map<std::string,std::string>&kv,uint64_t next_seg){
    fs::path active=dir/active_name;int afd=::open(active.c_str(),O_RDONLY);if(afd>=0){if(::fsync(afd))throw std::runtime_error("pre checkpoint wal sync");::close(afd);}
    auto bytes=encode_snapshot(seq,kv);fs::path tmp=dir/"snapshot.tmp",dst=dir/"snapshot.bdr3";int fd=::open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0644);if(fd<0)throw std::runtime_error("snapshot tmp");xwrite(fd,bytes.data(),bytes.size());if(::fsync(fd))throw std::runtime_error("snapshot sync");::close(fd);fs::rename(tmp,dst);fsync_dir(dir);
    fs::path nw=dir/next_name;create_wal(nw,next_seg,seq+1);fsync_dir(dir);if(fs::exists(active))fs::remove(active);fsync_dir(dir);
}

int main(){
    fs::path d="v41db";fs::remove_all(d);fs::create_directory(d);
    std::map<std::string,std::string> kv;uint64_t seq=0;std::string active="wal-000000.log";create_wal(d/active,1,1);
    uint64_t total_put=0,total_del=0;bool all=true;
    for(int cycle=0;cycle<4;++cycle){
        for(int i=0;i<5000;++i){std::string k="C"+std::to_string(cycle)+"_K"+std::to_string(i),v="V"+std::to_string(cycle)+"_"+std::to_string(i);kv[k]=v;append_wal(d/active,++seq,1,k,v);++total_put;}
        if(cycle>0){for(int i=0;i<1000;++i){std::string k="C"+std::to_string(cycle-1)+"_K"+std::to_string(i);kv.erase(k);append_wal(d/active,++seq,2,k,"");++total_del;}}
        std::string next="wal-"+std::string(6-std::to_string(cycle+1).size(),'0')+std::to_string(cycle+1)+".log";
        checkpoint_gen(d,active,next,seq,kv,uint64_t(cycle+2));active=next;
        for(int i=0;i<250;++i){std::string k="P"+std::to_string(cycle)+"_"+std::to_string(i),v="Q"+std::to_string(i);kv[k]=v;append_wal(d/active,++seq,1,k,v);++total_put;}
        auto r=reopen(d);bool ok=r.seq==seq&&r.kv==kv&&!fs::exists(d/"snapshot.tmp");all=all&&ok;
        std::cout<<"cycle,"<<cycle+1<<",records,"<<kv.size()<<",seq,"<<seq<<",reopen_records,"<<r.kv.size()<<",reopen_seq,"<<r.seq<<",pass,"<<ok<<"\n";
    }
    auto r=reopen(d);uint64_t resurrected=0;for(int c=0;c<3;++c)for(int i=0;i<1000;++i)if(r.kv.count("C"+std::to_string(c)+"_K"+std::to_string(i)))++resurrected;
    bool pass=all&&r.kv==kv&&r.seq==seq&&resurrected==0;
    std::cout<<"summary,total_put,"<<total_put<<",total_delete,"<<total_del<<",final_records,"<<kv.size()<<",resurrected,"<<resurrected<<",pass,"<<pass<<"\n";
    return pass?0:2;
}
