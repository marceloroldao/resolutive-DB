#pragma once

#include "../api_v106/migration.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bdr::v107 {

struct BatchResult {
    std::uint64_t sequence = 0;
    std::size_t operations = 0;
    bool durable = true;
};

class IntegratedDatabase {
public:
    IntegratedDatabase(std::filesystem::path legacy_directory,
                       std::filesystem::path bdw4_path);

    BatchResult write_batch(std::vector<v101::Operation> operations);
    BatchResult put(std::string key, std::string value);
    BatchResult erase(std::string key);
    BatchResult put_many(std::vector<std::pair<std::string, std::string>> entries);
    BatchResult erase_many(std::vector<std::string> keys);

    std::optional<std::string> get(const std::string& key) const;
    std::uint64_t last_sequence() const;
    std::uint64_t durable_sequence() const;
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    v106::MigratedDatabase database_;
};

} // namespace bdr::v107
