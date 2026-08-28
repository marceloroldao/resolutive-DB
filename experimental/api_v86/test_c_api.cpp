#include "bdr/c_api.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "bdr_c_api_contract";
    fs::remove_all(root);
    fs::create_directories(root);

    assert(bdr_c_abi_version() == 1u);

    bdr_c_database* db = nullptr;
    assert(bdr_c_open(root.string().c_str(), nullptr, &db) == BDR_C_OK);
    assert(db != nullptr);

    const std::string key = "turn:1";
    const std::string value = "user=Minha fonte principal e 24 V\nassistant=Entendido.";
    assert(bdr_c_put(db, key.data(), key.size(), value.data(), value.size(), BDR_C_BATCH_SYNC) == BDR_C_OK);
    assert(bdr_c_size(db) == 1u);

    int contains = 0;
    assert(bdr_c_contains(db, key.data(), key.size(), &contains) == BDR_C_OK);
    assert(contains == 1);

    size_t needed = 0;
    assert(bdr_c_get(db, key.data(), key.size(), nullptr, 0, &needed) == BDR_C_BUFFER_TOO_SMALL);
    assert(needed == value.size());

    std::vector<char> buffer(needed);
    size_t actual = 0;
    assert(bdr_c_get(db, key.data(), key.size(), buffer.data(), buffer.size(), &actual) == BDR_C_OK);
    assert(actual == value.size());
    assert(std::string(buffer.data(), actual) == value);

    assert(bdr_c_sync(db) == BDR_C_OK);
    assert(bdr_c_durable_sequence(db) >= 1u);
    bdr_c_close(db);

    db = nullptr;
    assert(bdr_c_open(root.string().c_str(), nullptr, &db) == BDR_C_OK);
    needed = 0;
    assert(bdr_c_get(db, key.data(), key.size(), nullptr, 0, &needed) == BDR_C_BUFFER_TOO_SMALL);
    buffer.assign(needed, '\0');
    assert(bdr_c_get(db, key.data(), key.size(), buffer.data(), buffer.size(), &actual) == BDR_C_OK);
    assert(std::string(buffer.data(), actual) == value);
    bdr_c_close(db);

    fs::remove_all(root);
    return 0;
}
