#pragma once

#include "../api_v104/batch_database.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bdr::v105 {

class ConcurrentDatabase {
public:
    explicit ConcurrentDatabase(std::filesystem::path wal_path);

    v104::BatchResult write_batch(std::vector<v101::Operation> operations,
                                  v104::DurabilityMode durability = v104::DurabilityMode::BatchSync);

    v104::BatchResult put(std::string key,
                          std::string value,
                          v104::DurabilityMode durability = v104::DurabilityMode::BatchSync);

    v104::BatchResult erase(std::string key,
                            v104::DurabilityMode durability = v104::DurabilityMode::BatchSync);

    v104::BatchResult put_many(std::vector<std::pair<std::string, std::string>> entries,
                               v104::DurabilityMode durability = v104::DurabilityMode::BatchSync);

    v104::BatchResult erase_many(std::vector<std::string> keys,
                                 v104::DurabilityMode durability = v104::DurabilityMode::BatchSync);

    std::optional<std::string> get(const std::string& key) const;
    std::uint64_t last_sequence() const;
    std::uint64_t durable_sequence() const;
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    v104::BatchDatabase database_;
};

} // namespace bdr::v105
