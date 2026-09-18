#include "bdr/atomic_c_api.h"

#include <cstring>
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

static bool get_equals(bdr_atomic_c_handle *db, const std::string& key, const std::string& expected) {
    bdr_atomic_c_buffer out{nullptr, 0};
    const auto status = bdr_atomic_c_get(db, key.data(), key.size(), &out);
    const bool ok = status == BDR_ATOMIC_C_OK && out.size == expected.size() &&
                    std::memcmp(out.data, expected.data(), expected.size()) == 0;
    if (out.data) bdr_atomic_c_free_buffer(out);
    return ok;
}

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "bdr-bdw4-sequence-regression";
    std::error_code ec;
    fs::remove_all(dir, ec);

    bdr_atomic_c_handle *persistence = nullptr;
    bdr_atomic_c_handle *concepts = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &persistence) != BDR_ATOMIC_C_OK || !persistence)
        return fail("persistence open failed");
    if (bdr_atomic_c_open(dir.string().c_str(), &concepts) != BDR_ATOMIC_C_OK || !concepts)
        return fail("concept open failed");

    bdr_atomic_c_batch_result result{};
    const std::string concept_key = "memoria-mobile/v1/org/concept-state/meta/fingerprint";
    const std::string concept_value = "sha256:legacy";
    auto concept_put = put_op(concept_key, concept_value);
    if (bdr_atomic_c_write_batch(concepts, &concept_put, 1, &result) != BDR_ATOMIC_C_OK ||
        result.sequence != 1)
        return fail("concept sequence 1 write failed");

    // The persistence handle was opened before the concept write and therefore
    // still believes sequence 0 is current. This reproduces the legacy collision.
    const std::string ep1_key = "memoria-mobile/v1/org/episode/000001/episode_id";
    const std::string ep1_value = "ep-1";
    auto ep1_put = put_op(ep1_key, ep1_value);
    if (bdr_atomic_c_write_batch(persistence, &ep1_put, 1, &result) != BDR_ATOMIC_C_OK ||
        result.sequence != 1)
        return fail("stale persistence sequence 1 write failed");

    const std::string ep2_key = "memoria-mobile/v1/org/episode/000002/episode_id";
    const std::string ep2_value = "ep-2";
    auto ep2_put = put_op(ep2_key, ep2_value);
    if (bdr_atomic_c_write_batch(persistence, &ep2_put, 1, &result) != BDR_ATOMIC_C_OK ||
        result.sequence != 2)
        return fail("persistence sequence 2 write failed");

    bdr_atomic_c_close(concepts);
    bdr_atomic_c_close(persistence);

    bdr_atomic_c_handle *reopened = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &reopened) != BDR_ATOMIC_C_OK || !reopened)
        return fail("legacy sequence-regression reopen failed");
    if (!get_equals(reopened, concept_key, concept_value) ||
        !get_equals(reopened, ep1_key, ep1_value) ||
        !get_equals(reopened, ep2_key, ep2_value))
        return fail("physical-order recovery lost a valid frame");

    uint64_t last = 0;
    if (bdr_atomic_c_last_sequence(reopened, &last) != BDR_ATOMIC_C_OK || last != 2)
        return fail("recovered last sequence must be max observed sequence");

    const std::string ep3_key = "memoria-mobile/v1/org/episode/000003/episode_id";
    const std::string ep3_value = "ep-3";
    auto ep3_put = put_op(ep3_key, ep3_value);
    if (bdr_atomic_c_write_batch(reopened, &ep3_put, 1, &result) != BDR_ATOMIC_C_OK ||
        result.sequence != 3)
        return fail("post-recovery write did not continue at max+1");
    bdr_atomic_c_close(reopened);

    reopened = nullptr;
    if (bdr_atomic_c_open(dir.string().c_str(), &reopened) != BDR_ATOMIC_C_OK || !reopened)
        return fail("second reopen failed");
    if (!get_equals(reopened, ep3_key, ep3_value))
        return fail("post-recovery write not durable");
    bdr_atomic_c_close(reopened);

    fs::remove_all(dir, ec);
    std::cout << "BDW4 sequence regression recovery PASS\n";
    return 0;
}
