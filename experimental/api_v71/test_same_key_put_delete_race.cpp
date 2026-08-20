#include "bdr/database.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

int main() {
    const fs::path root = "v71-same-key-race-db";
    fs::remove_all(root);

    bdr::Options opt;
    opt.reserve_bytes = 8ull * 1024ull * 1024ull;
    opt.wal_batch = 128;
    opt.partition_count = 64;
    opt.partition_max_load = 0.78;

    constexpr int threads_n = 8;
    constexpr int ops_per_thread = 4000;
    const std::string key = "shared-key";

    auto db = bdr::Database::open(root, opt);
    std::mutex accepted_mu;
    struct Accepted { std::uint64_t seq; bool is_put; std::string value; };
    std::vector<Accepted> accepted;
    accepted.reserve(threads_n * ops_per_thread);

    std::vector<std::thread> threads;
    for (int t = 0; t < threads_n; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < ops_per_thread; ++i) {
                try {
                    if (((i + t) & 1) == 0) {
                        std::string value = "v-" + std::to_string(t) + "-" + std::to_string(i);
                        auto ticket = db->put(key, value);
                        std::lock_guard<std::mutex> g(accepted_mu);
                        accepted.push_back({ticket.sequence, true, std::move(value)});
                    } else {
                        auto ticket = db->erase(key);
                        std::lock_guard<std::mutex> g(accepted_mu);
                        accepted.push_back({ticket.sequence, false, {}});
                    }
                } catch (...) {
                    std::cerr << "unexpected submit failure\n";
                    std::abort();
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    db->sync();
    db->checkpoint();
    const auto last_before = db->last_sequence();
    const auto durable_before = db->durable_sequence();
    db->close();

    std::sort(accepted.begin(), accepted.end(), [](const auto& a, const auto& b){ return a.seq < b.seq; });
    if (accepted.empty()) return 2;
    for (std::size_t i = 0; i < accepted.size(); ++i) {
        if (accepted[i].seq != i + 1) {
            std::cerr << "sequence gap at " << i << " got=" << accepted[i].seq << "\n";
            return 3;
        }
    }

    const auto& final_op = accepted.back();
    auto reopened = bdr::Database::open(root, opt);
    if (reopened->last_sequence() != accepted.size() ||
        reopened->durable_sequence() != accepted.size() ||
        last_before != accepted.size() ||
        durable_before != accepted.size()) {
        std::cerr << "sequence frontier mismatch\n";
        return 4;
    }

    const auto got = reopened->get(key);
    if (final_op.is_put) {
        if (!got || *got != final_op.value || reopened->size() != 1) {
            std::cerr << "final PUT state mismatch\n";
            return 5;
        }
    } else {
        if (got || reopened->size() != 0) {
            std::cerr << "final DELETE state mismatch\n";
            return 6;
        }
    }

    reopened->close();
    fs::remove_all(root);

    std::cout << "accepted,sequence_contiguous,reopen_frontier,final_state,pass\n";
    std::cout << accepted.size() << ",1,1,1,1\n";
    return 0;
}
