#include "bdr/atomic_c_api.h"

#include <filesystem>
#include <iostream>
#include <string>

static int fail(const char *message) {
    std::cerr << message << "\n";
    return 1;
}

static bdr_atomic_c_operation put_op(const std::string& key, const std::string& value) {
    return {BDR_ATOMIC_C_PUT, key.data(), key.size(), value.data(), value.size()};
}

static bool missing(bdr_atomic_c_handle *db, const std::string& key) {
    bdr_atomic_c_buffer out{nullptr, 0};
    const auto status = bdr_atomic_c_get(db, key.data(), key.size(), &out);
    if (out.data) bdr_atomic_c_free_buffer(out);
    return status == BDR_ATOMIC_C_NOT_FOUND;
}

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "bdr-atomic-clear";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    bdr_atomic_c_handle *db = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("open failed");

    const std::string k1 = "alpha", v1 = "one";
    const std::string k2 = "beta", v2 = "two";
    auto ops = std::vector<bdr_atomic_c_operation>{put_op(k1, v1), put_op(k2, v2)};
    bdr_atomic_c_batch_result write{};
    if (bdr_atomic_c_write_batch(db, ops.data(), ops.size(), &write) != BDR_ATOMIC_C_OK ||
        write.sequence != 1 || write.operations != 2 || write.durable != 1)
        return fail("initial write failed");

    bdr_atomic_c_batch_result cleared{};
    if (bdr_atomic_c_clear(db, &cleared) != BDR_ATOMIC_C_OK ||
        cleared.sequence != 2 || cleared.operations != 2 || cleared.durable != 1)
        return fail("clear failed");
    if (!missing(db, k1) || !missing(db, k2))
        return fail("keys remain visible after clear");

    bdr_atomic_c_close(db);
    db = nullptr;

    if (bdr_atomic_c_open(dir.string().c_str(), &db) != BDR_ATOMIC_C_OK || !db)
        return fail("cold reopen failed");
    if (!missing(db, k1) || !missing(db, k2))
        return fail("keys recovered after clear");

    std::uint64_t last = 0;
    if (bdr_atomic_c_last_sequence(db, &last) != BDR_ATOMIC_C_OK || last != 2)
        return fail("sequence mismatch after clear reopen");
    if (bdr_atomic_c_integrity_check(db) != BDR_ATOMIC_C_OK)
        return fail("integrity failed after clear reopen");

    const std::string k3 = "gamma", v3 = "three";
    auto op3 = put_op(k3, v3);
    bdr_atomic_c_batch_result after{};
    if (bdr_atomic_c_write_batch(db, &op3, 1, &after) != BDR_ATOMIC_C_OK ||
        after.sequence != 3 || after.operations != 1 || after.durable != 1)
        return fail("post-clear write failed");

    bdr_atomic_c_close(db);
    fs::remove_all(dir, ec);
    std::cout << "BDR atomic logical clear PASS\n";
    return 0;
}
