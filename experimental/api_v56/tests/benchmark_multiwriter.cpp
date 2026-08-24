#include "bdr/database.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using Clock=std::chrono::steady_clock;
namespace fs=std::filesystem;
static double pct(std::vector<double>v,double q){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[std::min(v.size()-1,std::size_t(q*(v.size()-1)))];}
static std::string K(int i){return "K"+std::to_string(i);}static std::string V(int i){return "V"+std::to_string(i);} 

int main(int argc,char**argv){
    int total=argc>1?std::stoi(argv[1]):100000;
    std::cout<<"writers,window,total,ops_s,submit_p50_us,submit_p99_us,wait_p50_us,wait_p99_us,reopen_errors,records,pass\n";
    int failures=0;
    for(int writers:{1,2,4,8,16})for(int window:{1,32,128}){
        fs::path d="v57-"+std::to_string(writers)+"-"+std::to_string(window);fs::remove_all(d);
        bdr::Options o;o.partition_count=4096;o.wal_batch=512;o.reserve_bytes=64ull*1024ull*1024ull;
        std::atomic<int>next{0};std::vector<std::vector<double>>sub(writers),wt(writers);
        auto db=bdr::Database::open(d,o);auto t0=Clock::now();std::vector<std::thread>ts;
        for(int x=0;x<writers;++x)ts.emplace_back([&,x]{bdr::Ticket last{};int pending=0;for(;;){int i=next.fetch_add(1);if(i>=total)break;auto a=Clock::now();last=db->put(K(i),V(i));auto b=Clock::now();sub[x].push_back(std::chrono::duration<double,std::micro>(b-a).count());if(++pending>=window){auto c=Clock::now();db->wait(last);auto e=Clock::now();wt[x].push_back(std::chrono::duration<double,std::micro>(e-c).count());pending=0;}}if(pending){auto c=Clock::now();db->wait(last);auto e=Clock::now();wt[x].push_back(std::chrono::duration<double,std::micro>(e-c).count());}});
        for(auto&t:ts)t.join();db->sync();auto t1=Clock::now();double ops=double(total)/std::chrono::duration<double>(t1-t0).count();db->close();
        std::vector<double>s,w;for(auto&v:sub)s.insert(s.end(),v.begin(),v.end());for(auto&v:wt)w.insert(w.end(),v.begin(),v.end());
        auto reopen=bdr::Database::open(d,o);std::size_t errors=0;for(int i=0;i<total;++i)if(reopen->get(K(i)).value_or("")!=V(i))++errors;bool pass=!errors&&reopen->size()==std::size_t(total);auto records=reopen->size();reopen->close();
        std::cout<<writers<<','<<window<<','<<total<<','<<ops<<','<<pct(s,.5)<<','<<pct(s,.99)<<','<<pct(w,.5)<<','<<pct(w,.99)<<','<<errors<<','<<records<<','<<pass<<"\n";
        if(!pass)++failures;
    }
    return failures?2:0;
}
