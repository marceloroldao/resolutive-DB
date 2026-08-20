#include "bdr/database.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct Result {
    double put_ops_s = 0.0;
    double wait_p50_us = 0.0;
    double wait_p95_us = 0.0;
    double wait_p99_us = 0.0;
    double get_ops_s = 0.0;
    std::size_t errors = 0;
};

static double pct(std::vector<double> xs, double p) {
    if (xs.empty()) return 0.0;
    std::sort(xs.begin(), xs.end());
    const auto i = std::min(xs.size()-1,
        static_cast<std::size_t>(std::llround((xs.size()-1)*p)));
    return xs[i];
}

static std::string key_for(std::size_t i) {
    return "v81-key-" + std::to_string(i);
}
static std::string val_for(std::size_t i) {
    return "payload-" + std::to_string(i) + "-abcdefghijklmnopqrstuvwxyz012345";
}

static Result run_one(const fs::path& root, int writers, int window,
                      std::size_t total_ops, std::size_t query_count) {
    fs::remove_all(root);
    bdr::Options opt;
    opt.reserve_bytes = 64ull * 1024ull * 1024ull;
    opt.wal_batch = 512;
    opt.partition_count = 4096;
    opt.partition_max_load = 0.78;

    auto db = bdr::Database::open(root, opt);
    std::atomic<std::size_t> next{0};
    std::mutex lat_mu;
    std::vector<double> waits_us;
    waits_us.reserve(total_ops / std::max(1,window) + writers + 16);

    const auto t0 = Clock::now();
    std::vector<std::thread> ts;
    for (int w=0; w<writers; ++w) {
        ts.emplace_back([&] {
            bdr::Ticket last{};
            int pending = 0;
            for (;;) {
                const auto i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= total_ops) break;
                last = db->put(key_for(i), val_for(i));
                ++pending;
                if (pending == window) {
                    const auto a=Clock::now();
                    db->wait(last);
                    const auto b=Clock::now();
                    const double us=std::chrono::duration<double,std::micro>(b-a).count();
                    { std::lock_guard<std::mutex> g(lat_mu); waits_us.push_back(us); }
                    pending=0;
                }
            }
            if (pending && last) {
                const auto a=Clock::now();
                db->wait(last);
                const auto b=Clock::now();
                const double us=std::chrono::duration<double,std::micro>(b-a).count();
                std::lock_guard<std::mutex> g(lat_mu); waits_us.push_back(us);
            }
        });
    }
    for (auto& t:ts) t.join();
    db->sync();
    const auto t1 = Clock::now();

    Result r;
    const double put_s = std::chrono::duration<double>(t1-t0).count();
    r.put_ops_s = total_ops / put_s;
    r.wait_p50_us = pct(waits_us,0.50);
    r.wait_p95_us = pct(waits_us,0.95);
    r.wait_p99_us = pct(waits_us,0.99);

    const auto g0=Clock::now();
    for (std::size_t q=0;q<query_count;++q) {
        const std::size_t i=(q*104729ull)%total_ops;
        auto v=db->get(key_for(i));
        if (!v || *v!=val_for(i)) ++r.errors;
    }
    const auto g1=Clock::now();
    const double get_s=std::chrono::duration<double>(g1-g0).count();
    r.get_ops_s=query_count/get_s;

    if (db->size()!=total_ops || db->last_sequence()!=total_ops ||
        db->durable_sequence()!=total_ops) ++r.errors;
    db->checkpoint();
    db->close();

    auto reopened=bdr::Database::open(root,opt);
    if (reopened->size()!=total_ops || reopened->last_sequence()!=total_ops ||
        reopened->durable_sequence()!=total_ops) ++r.errors;
    for(std::size_t i=0;i<total_ops;i+=std::max<std::size_t>(1,total_ops/1000)) {
        auto v=reopened->get(key_for(i));
        if(!v||*v!=val_for(i)) ++r.errors;
    }
    reopened->close();
    fs::remove_all(root);
    return r;
}

int main(int argc,char**argv){
    if(argc!=7){
        std::cerr<<"usage: bench <label> <root-prefix> <writers> <window> <ops> <queries>\n";
        return 2;
    }
    const std::string label=argv[1];
    const std::string prefix=argv[2];
    const int writers=std::stoi(argv[3]);
    const int window=std::stoi(argv[4]);
    const std::size_t ops=std::stoull(argv[5]);
    const std::size_t queries=std::stoull(argv[6]);

    std::cout<<"engine,run,writers,window,ops,put_ops_s,wait_p50_us,wait_p95_us,wait_p99_us,get_ops_s,errors\n";
    std::size_t total_errors=0;
    for(int run=1;run<=3;++run){
        auto r=run_one(prefix+"-"+std::to_string(run),writers,window,ops,queries);
        total_errors+=r.errors;
        std::cout<<label<<','<<run<<','<<writers<<','<<window<<','<<ops<<','
                 <<r.put_ops_s<<','<<r.wait_p50_us<<','<<r.wait_p95_us<<','
                 <<r.wait_p99_us<<','<<r.get_ops_s<<','<<r.errors<<'\n';
    }
    return total_errors?3:0;
}
