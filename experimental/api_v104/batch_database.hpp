#pragma once

#include "../api_v101/atomic_wal.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bdr::v104 {

enum class DurabilityMode : std::uint8_t {
    Async = 0,
    BatchSync = 1,
};

struct BatchResult {
    std::uint64_t sequence = 0;
    std::size_t operations = 0;
    bool durable = false;
};

class BatchDatabase {
public:
    explicit BatchDatabase(std::filesystem::path wal_path);

    BatchResult write_batch(std::vector<v101::Operation> operations,
                            DurabilityMode durability = DurabilityMode::BatchSync);

    BatchResult put_many(std::vector<std::pair<std::string, std::string>> entries,
                         DurabilityMode durability = DurabilityMode::BatchSync);

    BatchResult erase_many(std::vector<std::string> keys,
                           DurabilityMode durability = DurabilityMode::BatchSync);

    std::optional<std::string> get(const std::string& key) const;
    std::uint64_t last_sequence() const noexcept;
    std::uint64_t durable_sequence() const noexcept;
    std::size_t size() const noexcept;

private:
    std::filesystem::path wal_path_;
    std::map<std::string, std::string> state_;
    std::uint64_t last_sequence_ = 0;
    std::uint64_t durable_sequence_ = 0;
};

} // namespace bdr::v104
