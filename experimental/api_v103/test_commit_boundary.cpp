#include "commit_boundary.hpp"
#include "../api_v102/file_wal.hpp"

#include <cassert>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using bdr::v101::OpType;
using bdr::v101::Operation;
using bdr::v103::FailPoint;
using bdr::v103::SimulatedCrash;

static std::vector<Operation> logical_memory() {
    return {
        {OpType::Put, "mem/42/payload", "hello"},
        {OpType::Put, "mem/42/node/0", "alpha"},
        {OpType::Put, "mem/42/occ/0", "1"},
        {OpType::Put, "mem/42/meta", "org=demo"},
    };
}

static bool complete_or_absent(const std::map<std::string, std::string>& state) {
    const char* keys[] = {"mem/42/payload", "mem/42/node/0", "mem/42/occ/0", "mem/42/meta"};
    std::size_t present = 0;
    for (const auto* key : keys) present += state.count(key) ? 1u : 0u;
    return present == 0 || present == 4;
}

static fs::path temp_path(const char* name) {
    auto p = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove(p, ec);
    return p;
}

int main() {
    const auto ops = logical_memory();

    // Crash before append: no ACK, no state.
    {
        const auto p = temp_path("bdr-v103-before.bdw4");
        bool crashed = false;
        try { bdr::v103::commit_batch(p, 1, ops, FailPoint::BeforeAppend); }
        catch (const SimulatedCrash&) { crashed = true; }
        assert(crashed);
        std::map<std::string, std::string> state;
        auto rr = bdr::v102::recover_file(p, state);
        assert(rr.last_sequence == 0);
        assert(state.empty());
    }

    // Every strict partial append must recover as absent and be repairable.
    const auto frame = bdr::v101::encode_batch(1, ops);
    for (std::size_t cut = 1; cut < frame.size(); ++cut) {
        const auto p = temp_path("bdr-v103-mid.bdw4");
        bool crashed = false;
        try { bdr::v103::commit_batch(p, 1, ops, FailPoint::MidAppend, cut); }
        catch (const SimulatedCrash&) { crashed = true; }
        assert(crashed);
        std::map<std::string, std::string> state;
        auto rr = bdr::v102::recover_file(p, state);
        assert(rr.last_sequence == 0);
        assert(state.empty());
        assert(rr.repaired_torn_tail);
        assert(fs::file_size(p) == 0);
    }

    // Complete append without durable sync has no ACK. Recovery may be complete or absent,
    // but must never be partial. On a normal close in this deterministic harness it is complete.
    {
        const auto p = temp_path("bdr-v103-presync.bdw4");
        bool crashed = false;
        try { bdr::v103::commit_batch(p, 1, ops, FailPoint::AfterAppendBeforeSync); }
        catch (const SimulatedCrash&) { crashed = true; }
        assert(crashed);
        std::map<std::string, std::string> state;
        auto rr = bdr::v102::recover_file(p, state);
        (void)rr;
        assert(complete_or_absent(state));
    }

    // After fdatasync but before ACK: client has no ACK, yet recovery must contain full batch.
    {
        const auto p = temp_path("bdr-v103-postsync.bdw4");
        bool crashed = false;
        try { bdr::v103::commit_batch(p, 1, ops, FailPoint::AfterSyncBeforeAck); }
        catch (const SimulatedCrash&) { crashed = true; }
        assert(crashed);
        std::map<std::string, std::string> state;
        auto rr = bdr::v102::recover_file(p, state);
        assert(rr.last_sequence == 1);
        assert(state.size() == 4);
        assert(complete_or_absent(state));
    }

    // Successful return is the ACK boundary and implies durable recovery.
    {
        const auto p = temp_path("bdr-v103-ack.bdw4");
        const auto out = bdr::v103::commit_batch(p, 1, ops, FailPoint::None);
        assert(out.acknowledged);
        assert(out.durable_boundary_crossed);
        assert(out.sequence == 1);
        std::map<std::string, std::string> state;
        auto rr = bdr::v102::recover_file(p, state);
        assert(rr.last_sequence == 1);
        assert(state.size() == 4);
    }

    // A durable committed prefix followed by a torn second batch preserves only the prefix.
    {
        const auto p = temp_path("bdr-v103-prefix.bdw4");
        const auto first = bdr::v103::commit_batch(p, 1, ops, FailPoint::None);
        assert(first.acknowledged);
        const std::vector<Operation> second{{OpType::Put, "mem/43/payload", "world"},
                                            {OpType::Put, "mem/43/meta", "org=demo"}};
        bool crashed = false;
        try { bdr::v103::commit_batch(p, 2, second, FailPoint::MidAppend, 7); }
        catch (const SimulatedCrash&) { crashed = true; }
        assert(crashed);
        std::map<std::string, std::string> state;
        auto rr = bdr::v102::recover_file(p, state);
        assert(rr.last_sequence == 1);
        assert(state.size() == 4);
        assert(state.count("mem/43/payload") == 0);
        assert(state.count("mem/43/meta") == 0);
    }

    return 0;
}
