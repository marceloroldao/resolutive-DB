#include "bdr/database.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static std::string value_for(std::size_t i, int generation) {
    return "g" + std::to_string(generation) + ":" + std::to_string(i) + ":" + std::string(24, char('a' + (i % 26)));
}

static std::unordered_map<std::string,std::string> prepare_database(const fs::path& dir, std::uint64_t& expected_seq) {
    fs::remove_all(dir);
    bdr::Options options;
    options.reserve_bytes = 0;
    options.keep_size_preallocation = false;
    options.wal_batch = 256;
    options.partition_count = 1024;
    options.partition_max_load = 0.78;

    std::unordered_map<std::string,std::string> oracle;
    auto db = bdr::Database::open(dir, options);

    constexpr std::size_t base = 30000;
    for(std::size_t i=0;i<base;++i){
        auto k="k"+std::to_string(i);
        auto v=value_for(i,0);
        db->put(k,v);
        oracle[k]=v;
    }
    db->sync();
    db->checkpoint();

    for(std::size_t i=0;i<20000;++i){
        auto id=base+i;
        auto k="k"+std::to_string(id);
        auto v=value_for(id,1);
        db->put(k,v);
        oracle[k]=v;
    }
    for(std::size_t i=0;i<10000;++i){
        auto k="k"+std::to_string(i);
        auto v=value_for(i,2);
        db->put(k,v);
        oracle[k]=v;
    }
    for(std::size_t i=10000;i<15000;++i){
        auto k="k"+std::to_string(i);
        db->erase(k);
        oracle.erase(k);
    }
    db->sync();
    expected_seq=db->durable_sequence();
    if(expected_seq!=65000)throw std::runtime_error("unexpected prepared sequence");
    if(db->size()!=oracle.size())throw std::runtime_error("prepared size mismatch");
    db->close();
    return oracle;
}

static void verify_database(const fs::path& dir,
                            const std::unordered_map<std::string,std::string>& oracle,
                            std::uint64_t expected_seq) {
    bdr::Options options;
    options.reserve_bytes = 0;
    options.keep_size_preallocation = false;
    options.wal_batch = 256;
    options.partition_count = 1024;
    options.partition_max_load = 0.78;

    auto db=bdr::Database::open(dir,options);
    if(db->durable_sequence()!=expected_seq)throw std::runtime_error("recovered durable sequence mismatch");
    if(db->size()!=oracle.size())throw std::runtime_error("recovered size mismatch");
    for(const auto& [k,v]:oracle){
        auto got=db->get(k);
        if(!got||*got!=v)throw std::runtime_error("recovered value mismatch for "+k);
    }
    for(std::size_t i=10000;i<15000;++i){
        if(db->contains("k"+std::to_string(i)))throw std::runtime_error("deleted key resurrected");
    }
    db->close();
}

int main(){
    const char* backend_env=std::getenv("BDR_BACKEND_LABEL");
    const std::string backend=backend_env?backend_env:"unknown";
    const std::vector<std::string> phases={
        "mid_snapshot",
        "after_snapshot_write",
        "after_snapshot_fsync",
        "after_snapshot_close",
        "after_rename",
        "after_snapshot_dir_fsync",
        "after_old_wal_close",
        "after_new_wal_create",
        "after_new_wal_dir_fsync",
        "before_old_wal_remove",
        "after_old_wal_remove",
        "after_old_wal_remove_dir_fsync"
    };

    for(const auto& phase:phases){
        fs::path dir="v03-checkpoint-crash-"+backend+"-"+phase;
        std::uint64_t expected_seq=0;
        auto oracle=prepare_database(dir,expected_seq);

        pid_t pid=::fork();
        if(pid<0)throw std::runtime_error("fork failed");
        if(pid==0){
            ::setenv("BDR_CHECKPOINT_FAILPOINT",phase.c_str(),1);
            try{
                bdr::Options options;
                options.reserve_bytes=0;
                options.keep_size_preallocation=false;
                options.wal_batch=256;
                options.partition_count=1024;
                options.partition_max_load=0.78;
                auto db=bdr::Database::open(dir,options);
                db->checkpoint();
                db->close();
            }catch(...){
                ::_exit(90);
            }
            ::_exit(91);
        }

        int status=0;
        if(::waitpid(pid,&status,0)!=pid)throw std::runtime_error("waitpid failed");
        if(!WIFSIGNALED(status)||WTERMSIG(status)!=SIGKILL){
            throw std::runtime_error("failpoint did not terminate with SIGKILL: "+phase);
        }

        verify_database(dir,oracle,expected_seq);
        std::cout<<"CHECKPOINT_CRASH_BOUNDARY PASS backend="<<backend
                 <<" phase="<<phase
                 <<" sequence="<<expected_seq
                 <<" records="<<oracle.size()<<"\n";
        fs::remove_all(dir);
    }

    std::cout<<"V03_CHECKPOINT_CRASH_BOUNDARY PASS backend="<<backend
             <<" phases="<<phases.size()<<"\n";
    return 0;
}
