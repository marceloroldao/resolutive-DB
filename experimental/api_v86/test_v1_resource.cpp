#include "bdr/database.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;

static std::string value_for(std::size_t i) {
    std::string v(32, char('a' + (i % 26)));
    v += ":" + std::to_string(i);
    return v;
}

static double seconds_since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

int main() {
    const std::size_t n = std::strtoull(std::getenv("BDR_RESOURCE_RECORDS") ? std::getenv("BDR_RESOURCE_RECORDS") : "100000", nullptr, 10);
    const char* label_env = std::getenv("BDR_BACKEND_LABEL");
    const std::string label = label_env ? label_env : "unknown";
    const fs::path dir = "v03-resource-" + label + "-" + std::to_string(n);
    fs::remove_all(dir);

    bdr::Options options;
    options.reserve_bytes = 0;
    options.keep_size_preallocation = false;
    options.wal_batch = 512;
    options.partition_count = 4096;
    options.partition_max_load = 0.78;

    double insert_s = 0.0, checkpoint_s = 0.0, reopen_s = 0.0, lookup_s = 0.0;
    std::uint64_t seq = 0;
    {
        auto db = bdr::Database::open(dir, options);
        auto t0 = Clock::now();
        for (std::size_t i = 0; i < n; ++i) db->put("k" + std::to_string(i), value_for(i));
        db->sync();
        insert_s = seconds_since(t0);
        seq = db->durable_sequence();
        if (seq != n || db->size() != n) throw std::runtime_error("insert state mismatch");

        t0 = Clock::now();
        db->checkpoint();
        checkpoint_s = seconds_since(t0);
        if (db->durable_sequence() != seq) throw std::runtime_error("checkpoint sequence mismatch");
        db->close();
    }

    auto t0 = Clock::now();
    auto db = bdr::Database::open(dir, options);
    reopen_s = seconds_since(t0);
    if (db->size() != n || db->durable_sequence() != seq) throw std::runtime_error("reopen state mismatch");

    const std::size_t samples = std::min<std::size_t>(10000, n);
    t0 = Clock::now();
    for (std::size_t i = 0; i < samples; ++i) {
        const std::size_t id = (i * 9973ULL) % n;
        auto got = db->get("k" + std::to_string(id));
        if (!got || *got != value_for(id)) throw std::runtime_error("lookup mismatch");
    }
    lookup_s = seconds_since(t0);
    const auto st = db->index_stats();
    db->close();

    std::uintmax_t disk_bytes = 0;
    for (const auto& e : fs::directory_iterator(dir)) if (e.is_regular_file()) disk_bytes += e.file_size();

    std::cout << "DATABASE_BACKEND_RESOURCE PASS"
              << " backend=" << label
              << " records=" << n
              << " insert_seconds=" << insert_s
              << " insert_ops_per_sec=" << (n / insert_s)
              << " checkpoint_seconds=" << checkpoint_s
              << " reopen_seconds=" << reopen_s
              << " lookup_ops_per_sec=" << (samples / lookup_s)
              << " disk_bytes=" << disk_bytes
              << " slots=" << st.slots
              << " mean_load=" << st.mean_load
              << " max_partition_records=" << st.max_partition_records
              << "\n";

    fs::remove_all(dir);
    return 0;
}
