#include "../api_v101/atomic_wal.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using bdr::v101::OpType;
using bdr::v101::Operation;

static int fail(const char *message) {
    std::cerr << message << "\n";
    return 1;
}

static void append_frame(
    std::vector<std::uint8_t>& wal,
    std::uint64_t sequence,
    std::vector<Operation> operations
) {
    const auto frame = bdr::v101::encode_batch(sequence, operations);
    wal.insert(wal.end(), frame.begin(), frame.end());
}

static bool rejects(const std::vector<std::uint64_t>& sequences) {
    std::vector<std::uint8_t> wal;
    for (std::size_t i = 0; i < sequences.size(); ++i) {
        append_frame(
            wal,
            sequences[i],
            {Operation{OpType::Put, "k", std::to_string(i)}}
        );
    }
    std::map<std::string, std::string> state;
    try {
        (void)bdr::v101::replay(wal, state, 0);
    } catch (const std::runtime_error& exc) {
        return std::string(exc.what()) == "BDW4 sequence gap";
    }
    return false;
}

int main() {
    // Exact production shape observed in the read-only inspector:
    // frames 1..600 are monotonic, physical frame 601 repeats sequence 600,
    // and the WAL then resumes 601..33789 with no other anomaly.
    std::vector<std::uint8_t> production_shape;
    production_shape.reserve(3 * 1024 * 1024);

    for (std::uint64_t sequence = 1; sequence <= 600; ++sequence) {
        append_frame(
            production_shape,
            sequence,
            {Operation{OpType::Put, "counter", std::to_string(sequence)}}
        );
    }

    append_frame(
        production_shape,
        600,
        {
            Operation{OpType::Put, "legacy-marker", "frame-601-duplicate-600"},
            Operation{OpType::Put, "counter", "duplicate-600"}
        }
    );

    for (std::uint64_t sequence = 601; sequence <= 33789; ++sequence) {
        append_frame(
            production_shape,
            sequence,
            {Operation{OpType::Put, "counter", std::to_string(sequence)}}
        );
    }

    std::map<std::string, std::string> state;
    const auto result = bdr::v101::replay(production_shape, state, 0);
    if (result.torn_tail) return fail("production-shape replay reported torn tail");
    if (result.last_good != production_shape.size()) return fail("production-shape replay did not consume full WAL");
    if (result.committed_batches != 33790) return fail("production-shape physical batch count mismatch");
    if (result.last_sequence != 33789) return fail("production-shape logical last sequence mismatch");
    if (state["legacy-marker"] != "frame-601-duplicate-600") return fail("duplicate frame operations were not replayed");
    if (state["counter"] != "33789") return fail("post-duplicate monotonic continuation was not replayed");

    // Existing RC3 head compatibility remains accepted.
    {
        std::vector<std::uint8_t> wal;
        append_frame(wal, 1, {Operation{OpType::Put, "a", "first-1"}});
        append_frame(wal, 1, {Operation{OpType::Put, "b", "second-1"}});
        append_frame(wal, 2, {Operation{OpType::Put, "c", "2"}});
        std::map<std::string, std::string> recovered;
        const auto head = bdr::v101::replay(wal, recovered, 0);
        if (head.committed_batches != 3 || head.last_sequence != 2) return fail("head duplicate compatibility regressed");
        if (recovered["a"] != "first-1" || recovered["b"] != "second-1" || recovered["c"] != "2")
            return fail("head duplicate operations were not preserved");
    }

    // The compatibility budget is exactly one adjacent duplicate for a replay
    // starting at sequence zero. Anything broader remains fail-closed.
    if (!rejects({1, 1, 1})) return fail("triple duplicate was accepted");
    if (!rejects({1, 1, 2, 2})) return fail("second duplicate was accepted");
    if (!rejects({1, 2, 2, 2, 3})) return fail("repeated duplicate run was accepted");
    if (!rejects({1, 2, 1})) return fail("sequence regression was accepted");
    if (!rejects({1, 2, 4})) return fail("forward gap was accepted");

    // Compatibility is intentionally disabled when replay begins from a
    // non-zero snapshot sequence.
    {
        std::vector<std::uint8_t> wal;
        append_frame(wal, 600, {Operation{OpType::Put, "k", "duplicate"}});
        std::map<std::string, std::string> recovered;
        try {
            (void)bdr::v101::replay(wal, recovered, 600);
        } catch (const std::runtime_error& exc) {
            if (std::string(exc.what()) == "BDW4 sequence gap") {
                std::cout << "BDW4 bounded single-duplicate recovery PASS\n";
                return 0;
            }
            return fail("non-zero initial replay failed for unexpected reason");
        }
        return fail("non-zero initial replay incorrectly accepted duplicate");
    }
}
