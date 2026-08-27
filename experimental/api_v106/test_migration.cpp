#include "migration.hpp"
#include "bdr/database.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::vector<std::uint8_t> read_bytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static std::map<std::string, std::vector<std::uint8_t>> capture_legacy(const fs::path& dir) {
    std::map<std::string, std::vector<std::uint8_t>> out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        const auto ext = e.path().extension().string();
        if (ext == ".bdr3" || ext == ".bdw3")
            out[e.path().filename().string()] = read_bytes(e.path());
    }
    return out;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_v106_migration";
    const fs::path legacy_dir = root / "legacy-v1";
    const fs::path bdw4 = root / "v1_1.bdw4";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(legacy_dir);

    std::uint64_t legacy_sequence = 0;
    {
        bdr::Options options;
        options.reserve_bytes = 0;
        options.keep_size_preallocation = false;
        options.wal_batch = 1;
        auto db = bdr::Database::open(legacy_dir, options);

        db->put_sync("alpha", "A1");
        db->put_sync("beta", "B1");
        db->checkpoint();

        db->put_sync("beta", "B2");
        db->erase_sync("alpha");
        db->put_sync("gamma", "G1");
        db->sync();
        legacy_sequence = db->durable_sequence();
        assert(legacy_sequence >= 5);
        db->close();
    }

    const auto legacy_before = capture_legacy(legacy_dir);
    assert(!legacy_before.empty());
    bool saw_bdr3 = false, saw_bdw3 = false;
    for (const auto& [name, _] : legacy_before) {
        if (fs::path(name).extension() == ".bdr3") saw_bdr3 = true;
        if (fs::path(name).extension() == ".bdw3") saw_bdw3 = true;
    }
    assert(saw_bdr3 && saw_bdw3);

    {
        const auto imported = bdr::v106::read_v1_state(legacy_dir);
        assert(imported.sequence == legacy_sequence);
        assert(imported.values.count("alpha") == 0);
        assert(imported.values.at("beta") == "B2");
        assert(imported.values.at("gamma") == "G1");
    }

    std::uint64_t migrated_sequence = 0;
    {
        bdr::v106::MigratedDatabase db(legacy_dir, bdw4);
        assert(db.last_sequence() == legacy_sequence);
        assert(!db.get("alpha"));
        assert(db.get("beta") && *db.get("beta") == "B2");
        assert(db.get("gamma") && *db.get("gamma") == "G1");

        const std::vector<bdr::v101::Operation> batch = {
            {bdr::v101::OpType::Put, "delta", "D1"},
            {bdr::v101::OpType::Put, "beta", "B3"},
            {bdr::v101::OpType::Delete, "gamma", {}},
        };
        migrated_sequence = db.write_batch(batch);
        assert(migrated_sequence == legacy_sequence + 1);
        assert(db.get("delta") && *db.get("delta") == "D1");
        assert(db.get("beta") && *db.get("beta") == "B3");
        assert(!db.get("gamma"));
    }

    // v1.0 persistence is immutable during side-by-side migration.
    const auto legacy_after = capture_legacy(legacy_dir);
    assert(legacy_after == legacy_before);

    {
        bdr::v106::MigratedDatabase reopened(legacy_dir, bdw4);
        assert(reopened.last_sequence() == migrated_sequence);
        assert(!reopened.get("alpha"));
        assert(reopened.get("beta") && *reopened.get("beta") == "B3");
        assert(!reopened.get("gamma"));
        assert(reopened.get("delta") && *reopened.get("delta") == "D1");
    }

    // Reopen must not rewrite legacy data either.
    assert(capture_legacy(legacy_dir) == legacy_before);

    fs::remove_all(root, ec);
    return 0;
}
