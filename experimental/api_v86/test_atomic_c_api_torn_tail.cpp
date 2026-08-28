#include "bdr/atomic_c_api.h"
#include "../api_v101/atomic_wal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static int fail(const char *message) {
    std::cerr << message << "\n";
    return 1;
}

static bdr_atomic_c_operation put_op(const std::string &k, const std::string &v) {
    return {BDR_ATOMIC_C_PUT, k.data(), k.size(), v.data(), v.size()};
}

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "bdr-atomic-c-api-torn-tail";
    const fs::path wal = dir / "atomic.bdw4";
    std::error_code ec;
    fs::remove_all(dir, ec);

    bdr_atomic_c_handle *db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("open/create failed");

    const std::string k1 = "turn/u1/text";
    const std::string v1 = "confirmed";
    const std::string k2 = "turn/u1/provenance";
    const std::string v2 = "user_assertion";
    bdr_atomic_c_operation committed[] = {put_op(k1, v1), put_op(k2, v2)};
    bdr_atomic_c_batch_result first{};
    if (bdr_atomic_c_write_batch(db, committed, 2, &first) != BDR_ATOMIC_C_OK ||
        first.sequence != 1 || first.operations != 2 || first.durable != 1)
        return fail("committed batch failed");
    bdr_atomic_c_close(db);

    if (!fs::exists(wal)) return fail("atomic WAL was not created");
    const auto committed_size = fs::file_size(wal);
    if (committed_size == 0) return fail("committed WAL is empty");

    std::vector<bdr::v101::Operation> second_ops = {
        {bdr::v101::OpType::Put, "turn/u2/text", "must-not-appear"},
        {bdr::v101::OpType::Put, "turn/u2/provenance", "assistant_generated"},
    };
    const auto second = bdr::v101::encode_batch(2, second_ops);
    if (second.size() <= 4) return fail("encoded crash fixture unexpectedly small");
    {
        std::ofstream out(wal, std::ios::binary | std::ios::app);
        if (!out) return fail("cannot append torn-tail fixture");
        const auto torn_size = second.size() / 2;
        out.write(reinterpret_cast<const char *>(second.data()), static_cast<std::streamsize>(torn_size));
        if (!out) return fail("cannot write torn-tail fixture");
    }
    if (fs::file_size(wal) <= committed_size) return fail("torn-tail fixture was not appended");

    db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("reopen after torn tail failed");
    if (bdr_atomic_c_integrity_check(db) != BDR_ATOMIC_C_OK)
        return fail("integrity check failed after recovery");

    uint64_t last = 0, durable = 0;
    if (bdr_atomic_c_last_sequence(db, &last) != BDR_ATOMIC_C_OK ||
        bdr_atomic_c_durable_sequence(db, &durable) != BDR_ATOMIC_C_OK ||
        last != 1 || durable != 1)
        return fail("recovered sequence mismatch");

    int exists = 0;
    if (bdr_atomic_c_exists(db, k1.data(), k1.size(), &exists) != BDR_ATOMIC_C_OK || exists != 1)
        return fail("committed text missing after recovery");
    if (bdr_atomic_c_exists(db, k2.data(), k2.size(), &exists) != BDR_ATOMIC_C_OK || exists != 1)
        return fail("committed provenance missing after recovery");
    const std::string bad1 = "turn/u2/text";
    const std::string bad2 = "turn/u2/provenance";
    if (bdr_atomic_c_exists(db, bad1.data(), bad1.size(), &exists) != BDR_ATOMIC_C_OK || exists != 0)
        return fail("partial batch text became visible");
    if (bdr_atomic_c_exists(db, bad2.data(), bad2.size(), &exists) != BDR_ATOMIC_C_OK || exists != 0)
        return fail("partial batch provenance became visible");
    bdr_atomic_c_close(db);

    if (fs::file_size(wal) != committed_size) return fail("torn WAL tail was not physically repaired");

    db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("second deterministic reopen failed");
    if (bdr_atomic_c_last_sequence(db, &last) != BDR_ATOMIC_C_OK || last != 1)
        return fail("sequence changed after deterministic reopen");
    if (bdr_atomic_c_integrity_check(db) != BDR_ATOMIC_C_OK)
        return fail("integrity failed on deterministic reopen");
    bdr_atomic_c_close(db);

    fs::remove_all(dir, ec);
    std::cout << "atomic C ABI torn-tail recovery PASS\n";
    return 0;
}
