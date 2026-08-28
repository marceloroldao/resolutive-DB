#include "bdr/atomic_c_api.h"
#include "../api_v101/atomic_wal.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static bdr_atomic_c_operation put_op(const std::string &k, const std::string &v) {
    return {BDR_ATOMIC_C_PUT, k.data(), k.size(), v.data(), v.size()};
}

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "bdr-atomic-c-api-torn-tail";
    const fs::path wal = dir / "atomic.bdw4";
    std::error_code ec;
    fs::remove_all(dir, ec);

    // Commit one logical memory through the public C ABI.
    bdr_atomic_c_handle *db = nullptr;
    assert(bdr_atomic_c_open(dir.string().c_str(), &db) == BDR_ATOMIC_C_OK);
    const std::string k1 = "turn/u1/text";
    const std::string v1 = "confirmed";
    const std::string k2 = "turn/u1/provenance";
    const std::string v2 = "user_assertion";
    bdr_atomic_c_operation committed[] = {put_op(k1, v1), put_op(k2, v2)};
    bdr_atomic_c_batch_result first{};
    assert(bdr_atomic_c_write_batch(db, committed, 2, &first) == BDR_ATOMIC_C_OK);
    assert(first.sequence == 1);
    assert(first.durable == 1);
    bdr_atomic_c_close(db);

    const auto committed_size = fs::file_size(wal);

    // Simulate a process dying midway through the next BDW4 frame. The corrupt
    // bytes are injected only as a crash fixture; recovery is observed solely
    // through the public atomic C ABI below.
    std::vector<bdr::v101::Operation> second_ops = {
        {bdr::v101::OpType::Put, "turn/u2/text", "must-not-appear"},
        {bdr::v101::OpType::Put, "turn/u2/provenance", "assistant_generated"},
    };
    const auto second = bdr::v101::encode_batch(2, second_ops);
    assert(second.size() > 4);
    {
        std::ofstream out(wal, std::ios::binary | std::ios::app);
        const auto torn_size = second.size() / 2;
        out.write(reinterpret_cast<const char *>(second.data()), static_cast<std::streamsize>(torn_size));
    }
    assert(fs::file_size(wal) > committed_size);

    // Reopen through the C ABI. The engine must repair the torn tail, retain
    // sequence 1, preserve committed keys, and expose none of sequence 2.
    db = nullptr;
    assert(bdr_atomic_c_open(dir.string().c_str(), &db) == BDR_ATOMIC_C_OK);
    assert(bdr_atomic_c_integrity_check(db) == BDR_ATOMIC_C_OK);

    uint64_t last = 0, durable = 0;
    assert(bdr_atomic_c_last_sequence(db, &last) == BDR_ATOMIC_C_OK);
    assert(bdr_atomic_c_durable_sequence(db, &durable) == BDR_ATOMIC_C_OK);
    assert(last == 1);
    assert(durable == 1);

    int exists = 0;
    assert(bdr_atomic_c_exists(db, k1.data(), k1.size(), &exists) == BDR_ATOMIC_C_OK && exists == 1);
    assert(bdr_atomic_c_exists(db, k2.data(), k2.size(), &exists) == BDR_ATOMIC_C_OK && exists == 1);
    const std::string bad1 = "turn/u2/text";
    const std::string bad2 = "turn/u2/provenance";
    assert(bdr_atomic_c_exists(db, bad1.data(), bad1.size(), &exists) == BDR_ATOMIC_C_OK && exists == 0);
    assert(bdr_atomic_c_exists(db, bad2.data(), bad2.size(), &exists) == BDR_ATOMIC_C_OK && exists == 0);
    bdr_atomic_c_close(db);

    // Recovery repairs the physical tail as well, making later reopens deterministic.
    assert(fs::file_size(wal) == committed_size);
    db = nullptr;
    assert(bdr_atomic_c_open(dir.string().c_str(), &db) == BDR_ATOMIC_C_OK);
    assert(bdr_atomic_c_last_sequence(db, &last) == BDR_ATOMIC_C_OK && last == 1);
    assert(bdr_atomic_c_integrity_check(db) == BDR_ATOMIC_C_OK);
    bdr_atomic_c_close(db);

    fs::remove_all(dir, ec);
    return 0;
}
