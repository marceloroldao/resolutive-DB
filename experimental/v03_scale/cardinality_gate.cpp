#include "bdr/database.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static std::uint64_t dir_bytes(const fs::path& p) {
    std::uint64_t total = 0;
    if (!fs::exists(p)) return 0;
    for (const auto& e : fs::recursive_directory_iterator(p)) {
        if (e.is_regular_file()) total += static_cast<std::uint64_t>(e.file_size());
    }
    return total;
}

int main() {
    const std::size_t n = [] {
        if (const char* s = std::getenv("BDR_CARDINALITY")) return static_cast<std::size_t>(std::stoull(s));
        return static_cast<std::size_t>(100000);
    }();
    const fs::path path = "v03_cardinality_db";
    fs::remove_all(path);

    bdr::Options o;
    o.wal_batch = 512;
    auto db = bdr::Database::open(path, o);

    const auto insert_start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        const std::string key = "k" + std::to_string(i);
        const std::string value = "v" + std::to_string(i) + std::string(96, char('A' + (i % 26)));
        auto t = db->put(key, value);
        if ((i + 1) % 4096 == 0) db->wait(t);
    }
    db->sync();
    const auto insert_end = Clock::now();

    if (db->size() != n) throw std::runtime_error("size mismatch before checkpoint");
    if (db->last_sequence() != db->durable_sequence()) throw std::runtime_error("sequence mismatch before checkpoint");

    const auto checkpoint_start = Clock::now();
    db->checkpoint();
    const auto checkpoint_end = Clock::now();
    const auto bytes_after_checkpoint = dir_bytes(path);
    db->close();

    const auto reopen_start = Clock::now();
    db = bdr::Database::open(path, o);
    const auto reopen_end = Clock::now();
    if (db->size() != n) throw std::runtime_error("size mismatch after reopen");

    const auto lookup_start = Clock::now();
    std::size_t verified = 0;
    const std::size_t stride = n < 10000 ? 1 : (n / 10000);
    for (std::size_t i = 0; i < n; i += stride) {
        const std::string key = "k" + std::to_string(i);
        const std::string expected = "v" + std::to_string(i) + std::string(96, char('A' + (i % 26)));
        auto v = db->get(key);
        if (!v || *v != expected) throw std::runtime_error("lookup verification failed");
        ++verified;
    }
    const auto lookup_end = Clock::now();

    const auto stats = db->index_stats();
    db->close();

    const double insert_s = std::chrono::duration<double>(insert_end - insert_start).count();
    const double checkpoint_s = std::chrono::duration<double>(checkpoint_end - checkpoint_start).count();
    const double reopen_s = std::chrono::duration<double>(reopen_end - reopen_start).count();
    const double lookup_s = std::chrono::duration<double>(lookup_end - lookup_start).count();
    const double bytes_per_record = n ? static_cast<double>(bytes_after_checkpoint) / static_cast<double>(n) : 0.0;
    const double lookup_ops_s = lookup_s > 0 ? static_cast<double>(verified) / lookup_s : 0.0;

    std::cout << "V03_CARDINALITY PASS"
              << " records=" << n
              << " insert_seconds=" << insert_s
              << " insert_ops_per_sec=" << (insert_s > 0 ? static_cast<double>(n) / insert_s : 0.0)
              << " checkpoint_seconds=" << checkpoint_s
              << " reopen_seconds=" << reopen_s
              << " lookup_verified=" << verified
              << " lookup_ops_per_sec=" << lookup_ops_s
              << " disk_bytes=" << bytes_after_checkpoint
              << " bytes_per_record=" << bytes_per_record
              << " partitions=" << stats.partitions
              << " slots=" << stats.slots
              << " max_partition_records=" << stats.max_partition_records
              << " mean_load=" << stats.mean_load
              << "\n";
    return 0;
}
