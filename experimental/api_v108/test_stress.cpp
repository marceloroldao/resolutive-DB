#include "../api_v107/integrated_database.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using bdr::v101::OpType;
using bdr::v101::Operation;

static std::string key_for(int cycle, int batch, int op) {
    return "c" + std::to_string(cycle) + "/b" + std::to_string(batch) + "/o" + std::to_string(op);
}

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_v108_stress";
    const fs::path legacy = root / "legacy";
    const fs::path wal = root / "v11.bdw4";
    fs::remove_all(root);
    fs::create_directories(legacy);

    std::map<std::string, std::string> expected;
    std::uint64_t expected_sequence = 0;

    constexpr int cycles = 10;
    constexpr int batches_per_cycle = 120;

    for (int cycle = 0; cycle < cycles; ++cycle) {
        {
            bdr::v107::IntegratedDatabase db(legacy, wal);
            assert(db.last_sequence() == expected_sequence);
            assert(db.durable_sequence() == expected_sequence);

            for (int batch = 0; batch < batches_per_cycle; ++batch) {
                const int op_count = 1 + ((cycle * 17 + batch * 7) % 8);
                std::vector<Operation> ops;
                ops.reserve(op_count + 1);

                for (int op = 0; op < op_count; ++op) {
                    const auto key = key_for(cycle, batch, op);
                    const auto value = "value/" + std::to_string(cycle) + "/" + std::to_string(batch) + "/" + std::to_string(op);
                    ops.push_back({OpType::Put, key, value});
                    expected[key] = value;
                }

                if (batch % 9 == 0 && batch > 0) {
                    const auto old_key = key_for(cycle, batch - 1, 0);
                    ops.push_back({OpType::Delete, old_key, {}});
                    expected.erase(old_key);
                }

                const auto result = db.write_batch(std::move(ops));
                ++expected_sequence;
                assert(result.sequence == expected_sequence);
                assert(result.durable);
                assert(db.last_sequence() == expected_sequence);
                assert(db.durable_sequence() == expected_sequence);
            }

            for (const auto& [key, value] : expected) {
                const auto got = db.get(key);
                assert(got && *got == value);
            }
        }

        {
            bdr::v107::IntegratedDatabase reopened(legacy, wal);
            assert(reopened.last_sequence() == expected_sequence);
            assert(reopened.durable_sequence() == expected_sequence);
            assert(reopened.size() == expected.size());
            for (const auto& [key, value] : expected) {
                const auto got = reopened.get(key);
                assert(got && *got == value);
            }
        }
    }

    // Concurrent extension: unique keys avoid ambiguity while still stressing
    // the global ordering/serialization point of the integrated candidate.
    constexpr int threads = 8;
    constexpr int batches_per_thread = 100;
    std::mutex expected_mu;
    std::vector<std::thread> workers;

    {
        bdr::v107::IntegratedDatabase db(legacy, wal);
        const auto start_sequence = db.last_sequence();
        assert(start_sequence == expected_sequence);

        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&, t] {
                for (int i = 0; i < batches_per_thread; ++i) {
                    std::vector<Operation> ops;
                    const int count = 1 + ((t + i) % 6);
                    for (int j = 0; j < count; ++j) {
                        const auto key = "thread/" + std::to_string(t) + "/batch/" + std::to_string(i) + "/op/" + std::to_string(j);
                        const auto value = "tv/" + std::to_string(t) + "/" + std::to_string(i) + "/" + std::to_string(j);
                        ops.push_back({OpType::Put, key, value});
                    }
                    db.write_batch(ops);
                    std::lock_guard<std::mutex> lock(expected_mu);
                    for (const auto& op : ops) expected[op.key] = op.value;
                }
            });
        }
        for (auto& worker : workers) worker.join();
        expected_sequence += std::uint64_t(threads) * batches_per_thread;
        assert(db.last_sequence() == expected_sequence);
        assert(db.durable_sequence() == expected_sequence);
    }

    // Reopen repeatedly without new writes to detect replay drift.
    constexpr int reopen_loops = 25;
    for (int i = 0; i < reopen_loops; ++i) {
        bdr::v107::IntegratedDatabase db(legacy, wal);
        assert(db.last_sequence() == expected_sequence);
        assert(db.durable_sequence() == expected_sequence);
        assert(db.size() == expected.size());
        for (const auto& [key, value] : expected) {
            const auto got = db.get(key);
            assert(got && *got == value);
        }
    }

    std::cout << "V108 PASS cycles=" << cycles
              << " sequential_batches=" << (cycles * batches_per_cycle)
              << " concurrent_batches=" << (threads * batches_per_thread)
              << " reopen_loops=" << reopen_loops
              << " final_sequence=" << expected_sequence
              << " final_records=" << expected.size() << "\n";

    fs::remove_all(root);
    return 0;
}
