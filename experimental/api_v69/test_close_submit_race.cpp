#include "bdr/database.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

int main() {
    constexpr int ROUNDS = 50;
    constexpr int WRITERS = 8;
    constexpr int OPS_PER_WRITER = 1000;

    std::cout << "round,accepted,rejected,last_seq,durable_seq,size,missing,extra,pass\n";

    for (int round = 0; round < ROUNDS; ++round) {
        const fs::path dir = fs::path("v69-db-") += std::to_string(round);
        fs::remove_all(dir);

        bdr::Options opt;
        opt.reserve_bytes = 8ull * 1024ull * 1024ull;
        opt.wal_batch = 128;
        opt.partition_count = 256;
        opt.partition_max_load = 0.78;
        opt.keep_size_preallocation = true;

        auto db = bdr::Database::open(dir, opt);
        std::atomic<bool> go{false};
        std::atomic<int> rejected{0};
        std::mutex accepted_mu;
        std::vector<std::pair<std::uint64_t, std::string>> accepted;
        accepted.reserve(WRITERS * OPS_PER_WRITER);

        std::vector<std::thread> writers;
        for (int w = 0; w < WRITERS; ++w) {
            writers.emplace_back([&, w] {
                while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
                for (int i = 0; i < OPS_PER_WRITER; ++i) {
                    const std::string key = "r" + std::to_string(round) + "-w" + std::to_string(w) + "-k" + std::to_string(i);
                    try {
                        auto t = db->put(key, "value-" + key);
                        if (!t) return;
                        std::lock_guard<std::mutex> lk(accepted_mu);
                        accepted.emplace_back(t.sequence, key);
                    } catch (...) {
                        rejected.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            });
        }

        std::thread closer([&] {
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds(50 + (round % 7) * 25));
            db->close();
        });

        go.store(true, std::memory_order_release);
        for (auto& t : writers) t.join();
        closer.join();

        std::sort(accepted.begin(), accepted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        bool sequence_contiguous = true;
        for (std::size_t i = 0; i < accepted.size(); ++i) {
            if (accepted[i].first != i + 1) {
                sequence_contiguous = false;
                break;
            }
        }

        auto reopened = bdr::Database::open(dir, opt);
        std::size_t missing = 0;
        for (const auto& [seq, key] : accepted) {
            auto value = reopened->get(key);
            if (!value || *value != "value-" + key) ++missing;
        }

        const auto last = reopened->last_sequence();
        const auto durable = reopened->durable_sequence();
        const auto sz = reopened->size();
        const std::size_t extra = sz > accepted.size() ? sz - accepted.size() : 0;

        const bool pass = sequence_contiguous &&
                          missing == 0 &&
                          extra == 0 &&
                          sz == accepted.size() &&
                          last == accepted.size() &&
                          durable == accepted.size();

        std::cout << round << ',' << accepted.size() << ',' << rejected.load() << ','
                  << last << ',' << durable << ',' << sz << ',' << missing << ',' << extra << ','
                  << (pass ? 1 : 0) << '\n';

        reopened->close();
        fs::remove_all(dir);

        if (!pass) return 2;
    }

    return 0;
}
