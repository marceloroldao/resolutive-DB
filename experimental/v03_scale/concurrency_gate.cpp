#include "bdr/database.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static int run_case(std::size_t writers, std::size_t total_ops) {
    const fs::path dir = "v03_concurrency_db_" + std::to_string(writers);
    fs::remove_all(dir);

    bdr::Options opt;
    opt.wal_batch = 512;
    opt.partition_count = 4096;
    opt.partition_max_load = 0.80;

    auto db = bdr::Database::open(dir, opt);
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(writers);

    const auto t0 = Clock::now();
    for (std::size_t w = 0; w < writers; ++w) {
        threads.emplace_back([&, w] {
            try {
                bdr::Ticket last{};
                bool have_last = false;
                for (std::size_t i = w; i < total_ops; i += writers) {
                    const std::string key = "w" + std::to_string(w) + "_k" + std::to_string(i);
                    const std::string value = "value_" + std::to_string(w) + "_" + std::to_string(i);
                    last = db->put(key, value);
                    have_last = true;
                    if ((i % 1024) == 0) db->wait(last);
                }
                if (have_last) db->wait(last);
            } catch (const std::exception& e) {
                std::cerr << "writer " << w << " exception: " << e.what() << "\n";
                failed.store(true);
            }
        });
    }
    for (auto& t : threads) t.join();
    if (failed.load()) return 2;

    db->sync();
    const auto t1 = Clock::now();
    db->close();

    auto reopened = bdr::Database::open(dir, opt);
    if (reopened->size() != total_ops) {
        std::cerr << "size mismatch writers=" << writers << " expected=" << total_ops
                  << " got=" << reopened->size() << "\n";
        return 3;
    }
    for (std::size_t w = 0; w < writers; ++w) {
        for (std::size_t i = w; i < total_ops; i += writers) {
            const std::string key = "w" + std::to_string(w) + "_k" + std::to_string(i);
            const std::string expect = "value_" + std::to_string(w) + "_" + std::to_string(i);
            auto got = reopened->get(key);
            if (!got || *got != expect) {
                std::cerr << "value mismatch writers=" << writers << " key=" << key << "\n";
                return 4;
            }
        }
    }
    if (reopened->last_sequence() != reopened->durable_sequence()) {
        std::cerr << "sequence mismatch writers=" << writers << "\n";
        return 5;
    }
    reopened->close();

    const double seconds = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "V03_CONCURRENCY PASS writers=" << writers
              << " ops=" << total_ops
              << " seconds=" << seconds
              << " ops_per_sec=" << (total_ops / seconds) << "\n";
    return 0;
}

int main() {
    constexpr std::size_t total_ops = 200000;
    for (std::size_t writers : {1u, 4u, 8u, 16u}) {
        const int rc = run_case(writers, total_ops);
        if (rc != 0) return rc;
    }
    return 0;
}
