#pragma once

#include "../api_v101/atomic_wal.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace bdr::v103 {

enum class FailPoint {
    None,
    BeforeAppend,
    MidAppend,
    AfterAppendBeforeSync,
    AfterSyncBeforeAck,
    AfterAck,
};

struct CommitOutcome {
    bool acknowledged = false;
    bool durable_boundary_crossed = false;
    std::uint64_t sequence = 0;
};

class SimulatedCrash final {};

CommitOutcome commit_batch(const std::filesystem::path& path,
                           std::uint64_t sequence,
                           const std::vector<v101::Operation>& operations,
                           FailPoint failpoint = FailPoint::None,
                           std::size_t mid_append_bytes = 0);

} // namespace bdr::v103
