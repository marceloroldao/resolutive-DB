#include <bdr/database.hpp>
#include <bdr/atomic_database.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

static std::vector<std::pair<std::string, std::string>> logical_memory(
    std::size_t id, std::size_t records, std::size_t value_bytes) {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(records);
    const std::string prefix = "memory/" + std::to_string(id) + "/";
    for (std::size_t i = 0; i < records; ++i) {
        std::string role;
        if (i == 0) role = "payload";
        else if (i == 1) role = "metadata";
        else if (i % 2 == 0) role = "node/" + std::to_string(i);
        else role = "occurrence/" + std::to_string(i);
        std::string value(value_bytes, char('a' + ((id + i) % 26)));
        value += ":" + std::to_string(id) + ":" + std::to_string(i);
        out.emplace_back(prefix + role, std::move(value));
    }
    return out;
}

static std::uintmax_t directory_bytes(const fs::path& root) {
    std::uintmax_t total = 0;
    if (!fs::exists(root)) return 0;
    for (const auto& e : fs::recursive_directory_iterator(root))
        if (e.is_regular_file()) total += e.file_size();
    return total;
}

int main(int argc, char** argv) {
    const std::size_t memories = argc > 1 ? std::stoull(argv[1]) : 512;
    const std::size_t records_per_memory = argc > 2 ? std::stoull(argv[2]) : 24;
    const std::size_t value_bytes = argc > 3 ? std::stoull(argv[3]) : 256;
    assert(memories && records_per_memory >= 4);

    const fs::path root = fs::temp_directory_path() / "bdr_v112_memoria_atomic";
    const fs::path legacy_dir = root / "legacy";
    const fs::path atomic_dir = root / "atomic";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(legacy_dir);
    fs::create_directories(atomic_dir);

    bdr::Options options;
    options.reserve_bytes = 0;
    options.keep_size_preallocation = false;
    options.wal_batch = 512;

    double legacy_write_ms = 0.0;
    double legacy_reopen_ms = 0.0;
    {
        auto db = bdr::Database::open(legacy_dir, options);
        const auto start = Clock::now();
        for (std::size_t m = 0; m < memories; ++m) {
            auto rows = logical_memory(m, records_per_memory, value_bytes);
            for (auto& [key, value] : rows) db->put(std::move(key), std::move(value));
            // Same durability cadence as the atomic path: one durability boundary
            // per logical memory, but without an all-or-nothing recovery frame.
            db->sync();
        }
        legacy_write_ms = ms_since(start);
        db->close();
    }
    {
        const auto start = Clock::now();
        auto db = bdr::Database::open(legacy_dir, options);
        for (std::size_t m = 0; m < memories; ++m) {
            auto rows = logical_memory(m, records_per_memory, value_bytes);
            for (const auto& [key, value] : rows) {
                const auto got = db->get(key);
                assert(got && *got == value);
            }
        }
        legacy_reopen_ms = ms_since(start);
        db->close();
    }

    double atomic_write_ms = 0.0;
    double atomic_reopen_ms = 0.0;
    std::uint64_t atomic_last_sequence = 0;
    {
        auto db = bdr::AtomicDatabase::open(atomic_dir);
        const auto start = Clock::now();
        for (std::size_t m = 0; m < memories; ++m) {
            auto rows = logical_memory(m, records_per_memory, value_bytes);
            std::vector<bdr::Operation> ops;
            ops.reserve(rows.size());
            for (auto& [key, value] : rows)
                ops.push_back({bdr::OperationType::Put, std::move(key), std::move(value)});
            const auto result = db->write_batch(std::move(ops), bdr::DurabilityMode::BatchSync);
            assert(result.durable);
            assert(result.operations == records_per_memory);
            atomic_last_sequence = result.sequence;
        }
        atomic_write_ms = ms_since(start);
        assert(db->last_sequence() == memories);
        assert(db->durable_sequence() == memories);
    }
    {
        const auto start = Clock::now();
        auto db = bdr::AtomicDatabase::open(atomic_dir);
        assert(db->last_sequence() == atomic_last_sequence);
        for (std::size_t m = 0; m < memories; ++m) {
            auto rows = logical_memory(m, records_per_memory, value_bytes);
            for (const auto& [key, value] : rows) {
                const auto got = db->get(key);
                assert(got && *got == value);
            }
        }
        atomic_reopen_ms = ms_since(start);
    }

    const double write_ratio = legacy_write_ms > 0 ? atomic_write_ms / legacy_write_ms : 0.0;
    const double reopen_ratio = legacy_reopen_ms > 0 ? atomic_reopen_ms / legacy_reopen_ms : 0.0;

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"status\": \"PASS\",\n"
              << "  \"memories\": " << memories << ",\n"
              << "  \"records_per_memory\": " << records_per_memory << ",\n"
              << "  \"logical_records\": " << memories * records_per_memory << ",\n"
              << "  \"legacy_write_ms\": " << legacy_write_ms << ",\n"
              << "  \"atomic_write_ms\": " << atomic_write_ms << ",\n"
              << "  \"atomic_over_legacy_write\": " << write_ratio << ",\n"
              << "  \"legacy_reopen_verify_ms\": " << legacy_reopen_ms << ",\n"
              << "  \"atomic_reopen_verify_ms\": " << atomic_reopen_ms << ",\n"
              << "  \"atomic_over_legacy_reopen\": " << reopen_ratio << ",\n"
              << "  \"legacy_disk_bytes\": " << directory_bytes(legacy_dir) << ",\n"
              << "  \"atomic_disk_bytes\": " << directory_bytes(atomic_dir) << ",\n"
              << "  \"atomic_commits\": " << atomic_last_sequence << "\n"
              << "}\n";

    // Broad RC guardrail: atomic logical-memory semantics must not cause a
    // catastrophic throughput/reopen regression versus the v1.0 batch cadence.
    assert(write_ratio <= 3.0);
    assert(reopen_ratio <= 3.0);

    fs::remove_all(root, ec);
    return 0;
}
