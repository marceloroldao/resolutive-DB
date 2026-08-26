#include "bdr/database.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static std::size_t env_u64(const char* n, std::size_t d){
    if(const char* v=std::getenv(n)) return std::stoull(v);
    return d;
}

int main(){
    const std::size_t n = env_u64("BDR_ABLATION_RECORDS", 1000000);
    const std::size_t value_bytes = env_u64("BDR_ABLATION_VALUE_BYTES", 128);
    const fs::path dir = "v03_memory_ablation_db";
    fs::remove_all(dir);
    bdr::Options o;
    auto db = bdr::Database::open(dir,o);
    const auto t0=Clock::now();
    for(std::size_t i=0;i<n;++i){
        std::string key = "k" + std::to_string(i);
        std::string value(value_bytes, char('A'+(i%26)));
        db->put(std::move(key), std::move(value));
    }
    db->sync();
    const auto t1=Clock::now();
    auto st=db->index_stats();
    std::cout << "V03_MEMORY_ABLATION PASS records=" << n
              << " value_bytes=" << value_bytes
              << " seconds=" << std::chrono::duration<double>(t1-t0).count()
              << " partitions=" << st.partitions
              << " slots=" << st.slots
              << " max_partition_records=" << st.max_partition_records
              << " mean_load=" << st.mean_load << "\n";
    db->close();
    return 0;
}
