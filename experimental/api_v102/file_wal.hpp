#pragma once

#include "../api_v101/atomic_wal.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace bdr::v102 {

struct FileReplayResult {
    std::uint64_t last_sequence = 0;
    std::size_t committed_batches = 0;
    std::uintmax_t last_good = 0;
    bool repaired_torn_tail = false;
};

void append_batch(const std::filesystem::path& path,
                  std::uint64_t sequence,
                  const std::vector<v101::Operation>& operations,
                  bool durable = true);

FileReplayResult recover_file(const std::filesystem::path& path,
                              std::map<std::string, std::string>& state,
                              std::uint64_t initial_sequence = 0,
                              bool repair_torn_tail = true);

} // namespace bdr::v102
