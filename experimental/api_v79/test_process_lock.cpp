#include "bdr/database.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

int main(){
    const fs::path root="v79-process-lock-db";
    fs::remove_all(root);

    bdr::Options opt;
    opt.reserve_bytes=0;
    opt.wal_batch=32;
    opt.partition_count=32;

    auto parent_db=bdr::Database::open(root,opt);
    parent_db->put_sync("parent","open");

    pid_t pid=fork();
    if(pid<0){std::cerr<<"fork failed\n";return 2;}
    if(pid==0){
        try{
            auto child=bdr::Database::open(root,opt);
            child->close();
            _exit(10); // lock failure: child opened concurrently
        }catch(const std::exception&){
            _exit(0); // expected
        }
    }

    int status=0;
    if(waitpid(pid,&status,0)<0){std::cerr<<"waitpid failed\n";return 3;}
    if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){
        std::cerr<<"concurrent child was not rejected, status="<<status<<"\n";
        return 4;
    }

    // Threads inside the owning process remain valid.
    auto t1=parent_db->put("thread-compatible-1","a");
    auto t2=parent_db->put("thread-compatible-2","b");
    parent_db->wait(t2);
    if(parent_db->get("thread-compatible-1")!=std::optional<std::string>("a")||
       parent_db->get("thread-compatible-2")!=std::optional<std::string>("b")){
        std::cerr<<"same-process operations failed\n";return 5;
    }

    parent_db->checkpoint();
    parent_db->close();

    // Once the owner closes, another process/open must be allowed.
    pid=fork();
    if(pid<0){std::cerr<<"second fork failed\n";return 6;}
    if(pid==0){
        try{
            auto child=bdr::Database::open(root,opt);
            if(child->get("parent")!=std::optional<std::string>("open"))_exit(11);
            child->put_sync("child-after-release","ok");
            child->close();
            _exit(0);
        }catch(...){_exit(12);}
    }
    if(waitpid(pid,&status,0)<0||!WIFEXITED(status)||WEXITSTATUS(status)!=0){
        std::cerr<<"child could not open after lock release\n";return 7;
    }

    auto final_db=bdr::Database::open(root,opt);
    if(final_db->get("child-after-release")!=std::optional<std::string>("ok")){
        std::cerr<<"post-release persistence mismatch\n";return 8;
    }
    final_db->close();
    fs::remove_all(root);

    std::cout<<"concurrent_process_rejected,same_process_ok,lock_release_ok,reopen_ok,pass\n";
    std::cout<<"1,1,1,1,1\n";
    return 0;
}
