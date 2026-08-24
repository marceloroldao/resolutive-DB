#include "bdr/database.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

int main() {
    constexpr int ROUNDS = 20;
    constexpr int WRITERS = 8;
    constexpr int OPS_PER_WRITER = 2500;
    constexpr int CHECKPOINTS = 25;

    std::cout << "round,accepted,last_seq,durable_seq,size,missing,bad_value,pass\n";

    for (int round = 0; round < ROUNDS; ++round) {
        const fs::path dir = "v70-db-" + std::to_string(round);
        fs::remove_all(dir);

        bdr::Options opt;
        opt.reserve_bytes = 16ull * 1024ull * 1024ull;
        opt.wal_batch = 256;
        opt.partition_count = 512;
        opt.partition_max_load = 0.78;
        opt.keep_size_preallocation = true;

        auto db = bdr::Database::open(dir, opt);
        std::atomic<bool> go{false};
        std::atomic<bool> writers_done{false};
        std::mutex expected_mu;
        std::unordered_map<std::string, std::string> expected;
        expected.reserve(WRITERS * OPS_PER_WRITER);
        std::vector<std::uint64_t> tickets;
        tickets.reserve(WRITERS * OPS_PER_WRITER);

        std::vector<std::thread> writers;
        for (int w = 0; w < WRITERS; ++w) {
            writers.emplace_back([&, w] {
                while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
                for (int i = 0; i < OPS_PER_WRITER; ++i) {
                    const std::string key = "r" + std::to_string(round) + "-w" + std::to_string(w) + "-k" + std::to_string(i);
                    const std::string value = "value-" + std::to_string(i) + "-" + std::to_string(w);
                    auto t = db->put(key, value);
                    std::lock_guard<std::mutex> lk(expected_mu);
                    expected[key] = value;
                    tickets.push_back(t.sequence);
                }
            });
        }

        std::thread checkpointer([&] {
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < CHECKPOINTS; ++i) {
                db->checkpoint();
                std::this_thread::sleep_for(100us);
            }
            while (!writers_done.load(std::memory_order_acquire)) {
                db->checkpoint();
                std::this_thread::sleep_for(100us);
            }
        });

        go.store(true, std::memory_order_release);
        for (auto& t : writers) t.join();
        writers_done.store(true, std::memory_order_release);
        checkpointer.join();

        db->sync();
        db->checkpoint();
        db->close();

        std::sort(tickets.begin(), tickets.end());
        bool contiguous = tickets.size() == expected.size();
        for (std::size_t i = 0; contiguous && i < tickets.size(); ++i)
            contiguous = tickets[i] == i + 1;

        auto reopened = bdr::Database::open(dir, opt);
        std::size_t missing = 0, bad_value = 0;
        for (const auto& [key, value] : expected) {
            auto got = reopened->get(key);
            if (!got) ++missing;
            else if (*got != value) ++bad_value;
        }

        const auto last = reopened->last_sequence();
        const auto durable = reopened->durable_sequence();
        const auto sz = reopened->size();
        const bool pass = contiguous && missing == 0 && bad_value == 0 &&
                          sz == expected.size() && last == expected.size() &&
                          durable == expected.size();

        std::cout << round << ',' << expected.size() << ',' << last << ',' << durable << ','
                  << sz << ',' << missing << ',' << bad_value << ',' << (pass ? 1 : 0) << '\n';

        reopened->close();
        fs::remove_all(dir);
        if (!pass) return 2;
    }
    return 0;
}
