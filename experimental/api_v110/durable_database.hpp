#pragma once

#include "../api_v101/atomic_wal.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bdr::v110 {

enum class DurabilityMode : std::uint8_t {
    Async = 0,
    BatchSync = 1,
    PerOperationSync = 2,
};

struct BatchResult {
    std::uint64_t sequence = 0;
    std::size_t operations = 0;
    bool durable = false;
};

struct Diagnostics {
    std::uintmax_t wal_bytes = 0;
    std::size_t replayed_batches = 0;
    std::size_t replayed_operations = 0;
    std::size_t legacy_records = 0;
    std::size_t resident_records = 0;
    std::uint64_t legacy_sequence = 0;
    std::uint64_t last_sequence = 0;
    std::uint64_t durable_sequence = 0;
    bool repaired_torn_tail = false;
    std::uint64_t legacy_load_us = 0;
    std::uint64_t wal_read_us = 0;
    std::uint64_t wal_decode_apply_us = 0;
    std::uint64_t wal_replay_us = 0;
};

class DurableDatabase {
public:
    DurableDatabase(std::filesystem::path legacy_directory,
                    std::filesystem::path bdw4_path);

    BatchResult write_batch(std::vector<v101::Operation> operations,
                            DurabilityMode durability = DurabilityMode::BatchSync);

    BatchResult put(std::string key,
                    std::string value,
                    DurabilityMode durability = DurabilityMode::Async);
    BatchResult erase(std::string key,
                      DurabilityMode durability = DurabilityMode::Async);

    BatchResult put_many(std::vector<std::pair<std::string, std::string>> entries,
                         DurabilityMode durability = DurabilityMode::BatchSync);
    BatchResult erase_many(std::vector<std::string> keys,
                           DurabilityMode durability = DurabilityMode::BatchSync);

    void sync();

    std::optional<std::string> get(const std::string& key) const;
    std::vector<std::optional<std::string>> get_many(const std::vector<std::string>& keys) const;
    std::uint64_t last_sequence() const;
    std::uint64_t durable_sequence() const;
    std::size_t size() const;
    Diagnostics diagnostics() const;

private:
    void validate_operations(const std::vector<v101::Operation>& operations,
                             DurabilityMode durability) const;
    static void apply(std::map<std::string, std::string>& state,
                      const std::vector<v101::Operation>& operations);

    std::filesystem::path bdw4_path_;
    mutable std::mutex mutex_;
    std::map<std::string, std::string> state_;
    std::uint64_t last_sequence_ = 0;
    std::uint64_t durable_sequence_ = 0;
    Diagnostics diagnostics_;
};

} // namespace bdr::v110
