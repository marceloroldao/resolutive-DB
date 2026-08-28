#include "bdr/c_api.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "bdr-c-api-restart";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    const std::string key = "memoria-state";
    const std::string value = "semantic+provenance+episode";
    bdr_c_handle *db = nullptr;
    bdr_c_buffer out{nullptr, 0};

    assert(bdr_c_open(dir.string().c_str(), &db) == BDR_C_OK);
    assert(bdr_c_put(db, key.data(), key.size(), value.data(), value.size()) == BDR_C_OK);
    assert(bdr_c_sync(db) == BDR_C_OK);
    assert(bdr_c_checkpoint(db) == BDR_C_OK);
    bdr_c_close(db);

    db = nullptr;
    assert(bdr_c_open(dir.string().c_str(), &db) == BDR_C_OK);
    assert(bdr_c_get(db, key.data(), key.size(), &out) == BDR_C_OK);
    assert(out.size == value.size());
    assert(std::memcmp(out.data, value.data(), value.size()) == 0);
    bdr_c_free_buffer(out);

    assert(bdr_c_delete(db, key.data(), key.size()) == BDR_C_OK);
    assert(bdr_c_sync(db) == BDR_C_OK);
    bdr_c_close(db);

    db = nullptr;
    assert(bdr_c_open(dir.string().c_str(), &db) == BDR_C_OK);
    out = {nullptr, 0};
    assert(bdr_c_get(db, key.data(), key.size(), &out) == BDR_C_NOT_FOUND);
    bdr_c_close(db);

    std::filesystem::remove_all(dir, ec);
    return 0;
}
