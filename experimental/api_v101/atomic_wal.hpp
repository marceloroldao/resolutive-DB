#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bdr::v101 {

enum class OpType : std::uint8_t {
    Put = 1,
    Delete = 2,
};

struct Operation {
    OpType type = OpType::Put;
    std::string key;
    std::string value;
};

struct ReplayResult {
    std::size_t last_good = 0;
    bool torn_tail = false;
    std::uint64_t last_sequence = 0;
    std::size_t committed_batches = 0;
};

std::vector<std::uint8_t> encode_batch(std::uint64_t sequence,
                                       const std::vector<Operation>& operations);

ReplayResult replay(const std::vector<std::uint8_t>& bytes,
                    std::map<std::string, std::string>& state,
                    std::uint64_t initial_sequence = 0);

} // namespace bdr::v101
