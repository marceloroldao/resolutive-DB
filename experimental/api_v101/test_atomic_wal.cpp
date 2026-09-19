#include "atomic_wal.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using bdr::v101::OpType;
using bdr::v101::Operation;

static std::vector<Operation> logical_memory() {
    return {
        {OpType::Put, "mem/42/payload", "hello world"},
        {OpType::Put, "mem/42/node/0", "hello"},
        {OpType::Put, "mem/42/node/1", "world"},
        {OpType::Put, "mem/42/occurrence/0", "0:5"},
        {OpType::Put, "mem/42/metadata", "source=test"},
    };
}

static void require_absent(const std::map<std::string, std::string>& state) {
    for (const auto& op : logical_memory()) {
        assert(state.find(op.key) == state.end());
    }
}

static void require_complete(const std::map<std::string, std::string>& state) {
    for (const auto& op : logical_memory()) {
        auto it = state.find(op.key);
        assert(it != state.end());
        assert(it->second == op.value);
    }
}

int main() {
    using namespace bdr::v101;

    const auto batch = encode_batch(1, logical_memory());
    assert(batch.size() > 32);

    // Every strict truncation of the first logical-memory frame must recover
    // as "not committed". No subset may become visible.
    for (std::size_t cut = 0; cut < batch.size(); ++cut) {
        std::vector<std::uint8_t> torn(batch.begin(), batch.begin() + cut);
        std::map<std::string, std::string> state{{"preexisting", "stable"}};
        const auto rr = replay(torn, state, 0);
        assert(rr.committed_batches == 0);
        assert(rr.last_sequence == 0);
        assert(state.at("preexisting") == "stable");
        require_absent(state);
        if (cut != 0) assert(rr.torn_tail);
    }

    // Complete frame commits the entire logical memory under one sequence.
    {
        std::map<std::string, std::string> state{{"preexisting", "stable"}};
        const auto rr = replay(batch, state, 0);
        assert(!rr.torn_tail);
        assert(rr.committed_batches == 1);
        assert(rr.last_sequence == 1);
        assert(rr.last_good == batch.size());
        require_complete(state);
    }

    // A committed prefix followed by a torn second batch keeps only the first
    // complete batch. This models crash recovery at the WAL tail.
    const auto second = encode_batch(2, {
        {OpType::Put, "mem/43/payload", "second"},
        {OpType::Delete, "preexisting", ""},
    });
    for (std::size_t cut = 0; cut < second.size(); ++cut) {
        std::vector<std::uint8_t> wal = batch;
        wal.insert(wal.end(), second.begin(), second.begin() + cut);
        std::map<std::string, std::string> state{{"preexisting", "stable"}};
        const auto rr = replay(wal, state, 0);
        assert(rr.committed_batches == 1);
        assert(rr.last_sequence == 1);
        require_complete(state);
        assert(state.at("preexisting") == "stable");
        assert(state.find("mem/43/payload") == state.end());
    }

    // Two complete frames commit in deterministic sequence order.
    {
        std::vector<std::uint8_t> wal = batch;
        wal.insert(wal.end(), second.begin(), second.end());
        std::map<std::string, std::string> state{{"preexisting", "stable"}};
        const auto rr = replay(wal, state, 0);
        assert(rr.committed_batches == 2);
        assert(rr.last_sequence == 2);
        require_complete(state);
        assert(state.at("mem/43/payload") == "second");
        assert(state.find("preexisting") == state.end());
    }

    // Legacy Memoria.ia opened two independent BDR handles on the same WAL.
    // The only compatibility exception is the exact leading 1,1 restart;
    // both complete CRC-valid frames are applied, then strict sequencing resumes.
    {
        const auto legacy_second = encode_batch(1, {
            {OpType::Put, "legacy/second-handle", "preserved"},
        });
        const auto legacy_third = encode_batch(2, {
            {OpType::Put, "legacy/continued", "preserved"},
        });
        std::vector<std::uint8_t> wal = batch;
        wal.insert(wal.end(), legacy_second.begin(), legacy_second.end());
        wal.insert(wal.end(), legacy_third.begin(), legacy_third.end());
        std::map<std::string, std::string> state;
        const auto rr = replay(wal, state, 0);
        assert(rr.committed_batches == 3);
        assert(rr.last_sequence == 2);
        require_complete(state);
        assert(state.at("legacy/second-handle") == "preserved");
        assert(state.at("legacy/continued") == "preserved");
    }

    // The compatibility rule is deliberately bounded: a third sequence-1
    // frame remains corruption and must fail closed.
    {
        const auto legacy_second = encode_batch(1, {{OpType::Put, "legacy/a", "1"}});
        const auto illegal_third = encode_batch(1, {{OpType::Put, "legacy/b", "2"}});
        std::vector<std::uint8_t> wal = batch;
        wal.insert(wal.end(), legacy_second.begin(), legacy_second.end());
        wal.insert(wal.end(), illegal_third.begin(), illegal_third.end());
        std::map<std::string, std::string> state;
        bool failed = false;
        try { (void)replay(wal, state, 0); }
        catch (const std::runtime_error&) { failed = true; }
        assert(failed);
    }

    // The exception is only at the WAL head. A duplicate later sequence
    // remains a hard sequence gap.
    {
        const auto seq2 = encode_batch(2, {{OpType::Put, "strict/a", "1"}});
        const auto duplicate2 = encode_batch(2, {{OpType::Put, "strict/b", "2"}});
        std::vector<std::uint8_t> wal = batch;
        wal.insert(wal.end(), seq2.begin(), seq2.end());
        wal.insert(wal.end(), duplicate2.begin(), duplicate2.end());
        std::map<std::string, std::string> state;
        bool failed = false;
        try { (void)replay(wal, state, 0); }
        catch (const std::runtime_error&) { failed = true; }
        assert(failed);
    }

    // Corruption of a complete frame is not a torn-tail case: it must fail
    // closed instead of accepting an unverifiable logical memory.
    {
        auto corrupted = batch;
        corrupted[corrupted.size() / 2] ^= 0x01;
        std::map<std::string, std::string> state;
        bool failed = false;
        try {
            (void)replay(corrupted, state, 0);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
        require_absent(state);
    }

    // Sequence gaps are rejected deterministically.
    {
        const auto gap = encode_batch(2, logical_memory());
        std::map<std::string, std::string> state;
        bool failed = false;
        try {
            (void)replay(gap, state, 0);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
        require_absent(state);
    }

    std::cout << "V101 PASS: BDW4 atomic batch framing is all-or-none across "
              << batch.size() - 1 << " strict first-frame truncation points\n";
    return 0;
}
