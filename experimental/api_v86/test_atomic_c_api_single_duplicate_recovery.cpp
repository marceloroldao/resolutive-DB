#include "bdr/atomic_c_api.h"
#include "../api_v101/atomic_wal.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using bdr::v101::OpType;
using bdr::v101::Operation;

static int fail(const char *message) {
    std::cerr << message << "\n";
    return 1;
}

static void append_frame(
    std::ofstream& out,
    std::uint64_t sequence,
    std::vector<Operation> operations
) {
    const auto frame = bdr::v101::encode_batch(sequence, operations);
    out.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
    if (!out) throw std::runtime_error("failed to write fixture frame");
}

static bdr_atomic_c_operation put_op(const std::string& key, const std::string& value) {
    return {BDR_ATOMIC_C_PUT, key.data(), key.size(), value.data(), value.size()};
}

static bool get_equals(bdr_atomic_c_handle *db, const std::string& key, const std::string& expected) {
    bdr_atomic_c_buffer out{nullptr, 0};
    const auto status = bdr_atomic_c_get(db, key.data(), key.size(), &out);
    const bool ok = status == BDR_ATOMIC_C_OK &&
                    out.size == expected.size() &&
                    std::memcmp(out.data, expected.data(), expected.size()) == 0;
    if (out.data) bdr_atomic_c_free_buffer(out);
    return ok;
}

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "bdr-single-duplicate-c-api";
    const auto wal = dir / "atomic.bdw4";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    {
        std::ofstream out(wal, std::ios::binary | std::ios::trunc);
        if (!out) return fail("fixture WAL open failed");

        for (std::uint64_t sequence = 1; sequence <= 600; ++sequence) {
            append_frame(out, sequence, {Operation{OpType::Put, "counter", std::to_string(sequence)}});
        }
        append_frame(
            out,
            600,
            {
                Operation{OpType::Put, "legacy-marker", "frame-601-duplicate-600"},
                Operation{OpType::Put, "counter", "duplicate-600"}
            }
        );
        for (std::uint64_t sequence = 601; sequence <= 33789; ++sequence) {
            append_frame(out, sequence, {Operation{OpType::Put, "counter", std::to_string(sequence)}});
        }
    }

    bdr_atomic_c_handle *db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("C API failed to open production-shape legacy WAL");

    std::uint64_t last = 0;
    if (bdr_atomic_c_last_sequence(db, &last) != BDR_ATOMIC_C_OK || last != 33789)
        return fail("C API recovered incorrect logical last sequence");
    if (!get_equals(db, "legacy-marker", "frame-601-duplicate-600"))
        return fail("C API lost duplicate frame operations");
    if (!get_equals(db, "counter", "33789"))
        return fail("C API lost post-duplicate continuation");

    const std::string key = "post-recovery";
    const std::string value = "sequence-33790";
    auto op = put_op(key, value);
    bdr_atomic_c_batch_result result{};
    if (bdr_atomic_c_write_batch(db, &op, 1, &result) != BDR_ATOMIC_C_OK)
        return fail("post-recovery write failed");
    if (result.sequence != 33790)
        return fail("post-recovery write did not continue at max logical sequence + 1");
    if (bdr_atomic_c_sync(db) != BDR_ATOMIC_C_OK)
        return fail("post-recovery sync failed");
    bdr_atomic_c_close(db);

    db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("second cold reopen failed");
    if (!get_equals(db, key, value))
        return fail("post-recovery write not durable after second cold reopen");
    if (bdr_atomic_c_last_sequence(db, &last) != BDR_ATOMIC_C_OK || last != 33790)
        return fail("second cold reopen sequence mismatch");
    if (bdr_atomic_c_integrity_check(db) != BDR_ATOMIC_C_OK)
        return fail("integrity check failed after recovery and append");
    bdr_atomic_c_close(db);

    fs::remove_all(dir, ec);
    std::cout << "BDW4 single duplicate C API recovery PASS\n";
    return 0;
}
