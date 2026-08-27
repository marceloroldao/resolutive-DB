#include "concurrent_database.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "bdr_v105_concurrency.wal";
    std::error_code ec;
    fs::remove(path, ec);

    constexpr int kThreads = 8;
    constexpr int kOpsPerThread = 100;
    constexpr int kTotal = kThreads * kOpsPerThread;

    std::vector<std::uint64_t> sequences;
    sequences.reserve(kTotal);
    std::map<std::uint64_t, std::string> shared_value_by_sequence;
    std::mutex capture_mutex;

    {
        bdr::v105::ConcurrentDatabase db(path);
        std::vector<std::thread> workers;
        workers.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([&, t] {
                for (int i = 0; i < kOpsPerThread; ++i) {
                    const std::string unique_key = "t" + std::to_string(t) + ":" + std::to_string(i);
                    const std::string unique_value = "v" + std::to_string(t) + ":" + std::to_string(i);
                    const std::string shared_value = "winner:" + std::to_string(t) + ":" + std::to_string(i);

                    std::vector<bdr::v101::Operation> batch = {
                        {bdr::v101::OpType::Put, unique_key, unique_value},
                        {bdr::v101::OpType::Put, "shared", shared_value},
                    };
                    const auto result = db.write_batch(std::move(batch));
                    assert(result.durable);
                    assert(result.operations == 2);

                    std::lock_guard<std::mutex> capture(capture_mutex);
                    sequences.push_back(result.sequence);
                    shared_value_by_sequence[result.sequence] = shared_value;
                }
            });
        }

        for (auto& worker : workers) worker.join();

        assert(db.last_sequence() == kTotal);
        assert(db.durable_sequence() == kTotal);
        assert(db.size() == static_cast<std::size_t>(kTotal + 1));
    }

    assert(sequences.size() == static_cast<std::size_t>(kTotal));
    std::sort(sequences.begin(), sequences.end());
    for (int i = 0; i < kTotal; ++i)
        assert(sequences[static_cast<std::size_t>(i)] == static_cast<std::uint64_t>(i + 1));

    {
        bdr::v105::ConcurrentDatabase reopened(path);
        assert(reopened.last_sequence() == kTotal);
        assert(reopened.durable_sequence() == kTotal);
        assert(reopened.size() == static_cast<std::size_t>(kTotal + 1));

        for (int t = 0; t < kThreads; ++t) {
            for (int i = 0; i < kOpsPerThread; ++i) {
                const std::string key = "t" + std::to_string(t) + ":" + std::to_string(i);
                const std::string expected = "v" + std::to_string(t) + ":" + std::to_string(i);
                const auto value = reopened.get(key);
                assert(value && *value == expected);
            }
        }

        const auto shared = reopened.get("shared");
        assert(shared);
        assert(*shared == shared_value_by_sequence.at(kTotal));

        // Simple operations share the same total ordering path as multi-op batches.
        const auto put_result = reopened.put("simple", "present");
        assert(put_result.sequence == static_cast<std::uint64_t>(kTotal + 1));
        const auto erase_result = reopened.erase("simple");
        assert(erase_result.sequence == static_cast<std::uint64_t>(kTotal + 2));
        assert(!reopened.get("simple"));
    }

    {
        bdr::v105::ConcurrentDatabase reopened_again(path);
        assert(reopened_again.last_sequence() == static_cast<std::uint64_t>(kTotal + 2));
        assert(reopened_again.durable_sequence() == static_cast<std::uint64_t>(kTotal + 2));
        assert(!reopened_again.get("simple"));
    }

    fs::remove(path, ec);
    return 0;
}
