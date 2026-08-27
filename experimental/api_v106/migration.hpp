#pragma once

#include "../api_v101/atomic_wal.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bdr::v106 {

struct LegacyState {
    std::map<std::string, std::string> values;
    std::uint64_t sequence = 0;
};

LegacyState read_v1_state(const std::filesystem::path& directory);

class MigratedDatabase {
public:
    explicit MigratedDatabase(std::filesystem::path legacy_directory,
                              std::filesystem::path bdw4_path);

    std::uint64_t write_batch(const std::vector<v101::Operation>& operations);
    std::optional<std::string> get(const std::string& key) const;
    std::uint64_t last_sequence() const noexcept { return sequence_; }
    std::size_t size() const noexcept { return state_.size(); }

private:
    std::filesystem::path bdw4_path_;
    std::map<std::string, std::string> state_;
    std::uint64_t sequence_ = 0;
};

} // namespace bdr::v106
