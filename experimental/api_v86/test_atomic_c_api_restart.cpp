#include "bdr/atomic_c_api.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

static int fail(const char *message) {
    std::cerr << message << "\n";
    return 1;
}

static bdr_atomic_c_operation put_op(const std::string &k, const std::string &v) {
    return {BDR_ATOMIC_C_PUT, k.data(), k.size(), v.data(), v.size()};
}

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "bdr-atomic-c-api-restart";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    if (bdr_atomic_c_abi_version() != BDR_ATOMIC_C_ABI_VERSION) return fail("ABI version mismatch");

    bdr_atomic_c_handle *db = nullptr;
    auto status = bdr_atomic_c_open(dir.string().c_str(), &db);
    if (status != BDR_ATOMIC_C_OK || !db) return fail("open failed");

    const std::string k1 = "turn/u1/text";
    const std::string v1 = "Minha fonte principal e 24 V.";
    const std::string k2 = "turn/u1/provenance";
    const std::string v2 = "user_assertion";
    const std::string k3 = "turn/u1/relation";
    const std::string v3 = "fonte principal|is|24 V";

    bdr_atomic_c_operation ops[] = {put_op(k1, v1), put_op(k2, v2), put_op(k3, v3)};
    bdr_atomic_c_batch_result result{};
    status = bdr_atomic_c_write_batch(db, ops, 3, &result);
    if (status != BDR_ATOMIC_C_OK || result.operations != 3 || result.sequence == 0 || result.durable != 1)
        return fail("atomic write batch failed");

    uint64_t last = 0, durable = 0;
    if (bdr_atomic_c_last_sequence(db, &last) != BDR_ATOMIC_C_OK ||
        bdr_atomic_c_durable_sequence(db, &durable) != BDR_ATOMIC_C_OK ||
        last != result.sequence || durable != result.sequence)
        return fail("sequence observability mismatch");
    if (bdr_atomic_c_integrity_check(db) != BDR_ATOMIC_C_OK) return fail("integrity check failed");

    int exists = 0;
    if (bdr_atomic_c_exists(db, k1.data(), k1.size(), &exists) != BDR_ATOMIC_C_OK || exists != 1)
        return fail("written key not visible");
    bdr_atomic_c_close(db);

    db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("reopen failed");
    bdr_atomic_c_buffer out{nullptr, 0};
    if (bdr_atomic_c_get(db, k1.data(), k1.size(), &out) != BDR_ATOMIC_C_OK ||
        out.size != v1.size() || std::memcmp(out.data, v1.data(), v1.size()) != 0)
        return fail("recovered value mismatch");
    bdr_atomic_c_free_buffer(out);
    if (bdr_atomic_c_exists(db, k2.data(), k2.size(), &exists) != BDR_ATOMIC_C_OK || exists != 1)
        return fail("provenance missing after reopen");
    if (bdr_atomic_c_exists(db, k3.data(), k3.size(), &exists) != BDR_ATOMIC_C_OK || exists != 1)
        return fail("relation missing after reopen");

    bdr_atomic_c_operation del[] = {
        {BDR_ATOMIC_C_DELETE, k1.data(), k1.size(), nullptr, 0},
        {BDR_ATOMIC_C_DELETE, k2.data(), k2.size(), nullptr, 0},
        {BDR_ATOMIC_C_DELETE, k3.data(), k3.size(), nullptr, 0},
    };
    if (bdr_atomic_c_write_batch(db, del, 3, &result) != BDR_ATOMIC_C_OK || result.operations != 3)
        return fail("atomic delete batch failed");
    bdr_atomic_c_close(db);

    db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("second reopen failed");
    if (bdr_atomic_c_exists(db, k1.data(), k1.size(), &exists) != BDR_ATOMIC_C_OK || exists != 0)
        return fail("text survived delete batch");
    if (bdr_atomic_c_exists(db, k2.data(), k2.size(), &exists) != BDR_ATOMIC_C_OK || exists != 0)
        return fail("provenance survived delete batch");
    if (bdr_atomic_c_exists(db, k3.data(), k3.size(), &exists) != BDR_ATOMIC_C_OK || exists != 0)
        return fail("relation survived delete batch");
    bdr_atomic_c_close(db);

    std::filesystem::remove_all(dir, ec);
    std::cout << "atomic C ABI restart PASS\n";
    return 0;
}
