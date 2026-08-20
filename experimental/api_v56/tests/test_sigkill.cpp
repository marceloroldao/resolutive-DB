#include "bdr/database.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs=std::filesystem;

static bdr::Options opts(){bdr::Options o;o.partition_count=4096;o.wal_batch=512;o.reserve_bytes=64ull*1024ull*1024ull;return o;}

static int child_run(const fs::path&d){
    auto db=bdr::Database::open(d,opts());
    std::atomic<uint64_t>id{0};std::vector<std::thread>ts;
    for(int w=0;w<8;++w)ts.emplace_back([&]{for(;;){uint64_t i=id.fetch_add(1);db->put("K"+std::to_string(i),"V"+std::to_string(i));if((i&127)==127)std::this_thread::yield();}});
    for(auto&t:ts)t.join();return 0;
}

int main(){
    std::cout<<"kill_ms,recovered,last_sequence,size_matches,post_repair,pass\n";
    int fails=0;int case_id=0;
    for(int ms:{5,10,25,50}){
        fs::path d="v58-crash-"+std::to_string(case_id++);fs::remove_all(d);
        pid_t pid=::fork();
        if(pid==0){try{return child_run(d);}catch(...){return 3;}}
        if(pid<0)return 4;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        ::kill(pid,SIGKILL);int status=0;::waitpid(pid,&status,0);
        bool killed=WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL;
        bool size_match=false,post=false;std::size_t n=0;uint64_t seq=0;
        try{
            auto db=bdr::Database::open(d,opts());n=db->size();seq=db->last_sequence();size_match=(n==std::size_t(seq));db->put_sync("POST","REPAIR");db->close();
            auto r=bdr::Database::open(d,opts());post=r->get("POST").value_or("")=="REPAIR"&&r->size()==n+1&&r->last_sequence()==seq+1;r->close();
        }catch(...){size_match=false;post=false;}
        bool pass=killed&&size_match&&post;
        std::cout<<ms<<','<<n<<','<<seq<<','<<size_match<<','<<post<<','<<pass<<"\n";
        if(!pass)++fails;
    }
    return fails?2:0;
}
