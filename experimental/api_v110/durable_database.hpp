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
    std::uint64_t last_sequence() const;
    std::uint64_t durable_sequence() const;
    std::size_t size() const;

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
};

} // namespace bdr::v110
