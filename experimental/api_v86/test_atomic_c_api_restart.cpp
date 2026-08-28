#include "bdr/atomic_c_api.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>

static bdr_atomic_c_operation put_op(const std::string &k, const std::string &v) {
    return {BDR_ATOMIC_C_PUT, k.data(), k.size(), v.data(), v.size()};
}

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "bdr-atomic-c-api-restart";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    assert(bdr_atomic_c_abi_version() == BDR_ATOMIC_C_ABI_VERSION);

    bdr_atomic_c_handle *db = nullptr;
    assert(bdr_atomic_c_open(dir.string().c_str(), &db) == BDR_ATOMIC_C_OK);

    const std::string k1 = "turn/u1/text";
    const std::string v1 = "Minha fonte principal e 24 V.";
    const std::string k2 = "turn/u1/provenance";
    const std::string v2 = "user_assertion";
    const std::string k3 = "turn/u1/relation";
    const std::string v3 = "fonte principal|is|24 V";

    bdr_atomic_c_operation ops[] = {put_op(k1, v1), put_op(k2, v2), put_op(k3, v3)};
    bdr_atomic_c_batch_result result{};
    assert(bdr_atomic_c_write_batch(db, ops, 3, &result) == BDR_ATOMIC_C_OK);
    assert(result.operations == 3);
    assert(result.sequence > 0);
    assert(result.durable == 1);

    uint64_t last = 0, durable = 0;
    assert(bdr_atomic_c_last_sequence(db, &last) == BDR_ATOMIC_C_OK);
    assert(bdr_atomic_c_durable_sequence(db, &durable) == BDR_ATOMIC_C_OK);
    assert(last == result.sequence);
    assert(durable == result.sequence);
    assert(bdr_atomic_c_integrity_check(db) == BDR_ATOMIC_C_OK);

    int exists = 0;
    assert(bdr_atomic_c_exists(db, k1.data(), k1.size(), &exists) == BDR_ATOMIC_C_OK);
    assert(exists == 1);
    bdr_atomic_c_close(db);

    db = nullptr;
    assert(bdr_atomic_c_open(dir.string().c_str(), &db) == BDR_ATOMIC_C_OK);
    bdr_atomic_c_buffer out{nullptr, 0};
    assert(bdr_atomic_c_get(db, k1.data(), k1.size(), &out) == BDR_ATOMIC_C_OK);
    assert(out.size == v1.size());
    assert(std::memcmp(out.data, v1.data(), v1.size()) == 0);
    bdr_atomic_c_free_buffer(out);
    assert(bdr_atomic_c_exists(db, k2.data(), k2.size(), &exists) == BDR_ATOMIC_C_OK && exists == 1);
    assert(bdr_atomic_c_exists(db, k3.data(), k3.size(), &exists) == BDR_ATOMIC_C_OK && exists == 1);

    bdr_atomic_c_operation del[] = {
        {BDR_ATOMIC_C_DELETE, k1.data(), k1.size(), nullptr, 0},
        {BDR_ATOMIC_C_DELETE, k2.data(), k2.size(), nullptr, 0},
        {BDR_ATOMIC_C_DELETE, k3.data(), k3.size(), nullptr, 0},
    };
    assert(bdr_atomic_c_write_batch(db, del, 3, &result) == BDR_ATOMIC_C_OK);
    bdr_atomic_c_close(db);

    db = nullptr;
    assert(bdr_atomic_c_open(dir.string().c_str(), &db) == BDR_ATOMIC_C_OK);
    assert(bdr_atomic_c_exists(db, k1.data(), k1.size(), &exists) == BDR_ATOMIC_C_OK && exists == 0);
    assert(bdr_atomic_c_exists(db, k2.data(), k2.size(), &exists) == BDR_ATOMIC_C_OK && exists == 0);
    assert(bdr_atomic_c_exists(db, k3.data(), k3.size(), &exists) == BDR_ATOMIC_C_OK && exists == 0);
    bdr_atomic_c_close(db);

    std::filesystem::remove_all(dir, ec);
    return 0;
}
