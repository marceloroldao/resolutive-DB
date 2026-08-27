#include "../api_v107/integrated_database.hpp"
#include "../api_v101/atomic_wal.hpp"
#include "bdr/database.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using bdr::v101::OpType;
using bdr::v101::Operation;

static std::vector<std::uint8_t> read_bytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static void write_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    assert(f);
    if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    f.flush();
    assert(f.good());
}

static std::map<std::string, std::vector<std::uint8_t>> capture_legacy(const fs::path& dir) {
    std::map<std::string, std::vector<std::uint8_t>> out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        const auto ext = e.path().extension().string();
        if (ext == ".bdr3" || ext == ".bdw3") out[e.path().filename().string()] = read_bytes(e.path());
    }
    return out;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_v109_integrated_crash";
    const fs::path legacy = root / "legacy";
    const fs::path prefix_wal = root / "prefix.bdw4";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(legacy);

    std::uint64_t legacy_sequence = 0;
    {
        bdr::Options options;
        options.reserve_bytes = 0;
        options.keep_size_preallocation = false;
        options.wal_batch = 1;
        auto db = bdr::Database::open(legacy, options);
        db->put_sync("legacy/a", "A1");
        db->put_sync("legacy/b", "B1");
        db->checkpoint();
        db->put_sync("legacy/b", "B2");
        db->put_sync("legacy/c", "C1");
        db->sync();
        legacy_sequence = db->durable_sequence();
        db->close();
    }

    const auto legacy_before = capture_legacy(legacy);
    assert(!legacy_before.empty());

    const std::vector<Operation> prefix_batch = {
        {OpType::Put, "prefix/keep", "P1"},
        {OpType::Put, "prefix/delete-later", "alive"},
    };

    {
        bdr::v107::IntegratedDatabase db(legacy, prefix_wal);
        assert(db.last_sequence() == legacy_sequence);
        const auto result = db.write_batch(prefix_batch);
        assert(result.sequence == legacy_sequence + 1);
        assert(result.durable);
    }

    const auto prefix = read_bytes(prefix_wal);
    assert(!prefix.empty());
    const std::uint64_t prefix_sequence = legacy_sequence + 1;

    const std::vector<Operation> crash_batch = {
        {OpType::Put, "crash/x", "X2"},
        {OpType::Put, "crash/y", "Y2"},
        {OpType::Delete, "prefix/delete-later", {}},
        {OpType::Put, "crash/z", std::string(257, 'z')},
    };
    const auto frame = bdr::v101::encode_batch(prefix_sequence + 1, crash_batch);
    assert(frame.size() > 16);

    // Every incomplete byte prefix of the next batch simulates a crash/torn tail.
    // Recovery must expose the previously durable prefix and none of the new batch.
    for (std::size_t cut = 0; cut < frame.size(); ++cut) {
        const fs::path wal = root / ("case-" + std::to_string(cut) + ".bdw4");
        std::vector<std::uint8_t> bytes = prefix;
        bytes.insert(bytes.end(), frame.begin(), frame.begin() + std::ptrdiff_t(cut));
        write_bytes(wal, bytes);

        {
            bdr::v107::IntegratedDatabase db(legacy, wal);
            assert(db.last_sequence() == prefix_sequence);
            assert(db.durable_sequence() == prefix_sequence);
            assert(db.get("prefix/keep") && *db.get("prefix/keep") == "P1");
            assert(db.get("prefix/delete-later") && *db.get("prefix/delete-later") == "alive");
            assert(!db.get("crash/x"));
            assert(!db.get("crash/y"));
            assert(!db.get("crash/z"));
            assert(db.get("legacy/b") && *db.get("legacy/b") == "B2");
        }

        // Any torn bytes must be repaired back to exactly the committed prefix.
        assert(read_bytes(wal) == prefix);
        assert(capture_legacy(legacy) == legacy_before);
        fs::remove(wal, ec);
    }

    // A complete frame must commit atomically and survive repeated reopen.
    const fs::path complete_wal = root / "complete.bdw4";
    std::vector<std::uint8_t> complete = prefix;
    complete.insert(complete.end(), frame.begin(), frame.end());
    write_bytes(complete_wal, complete);

    for (int reopen = 0; reopen < 5; ++reopen) {
        bdr::v107::IntegratedDatabase db(legacy, complete_wal);
        assert(db.last_sequence() == prefix_sequence + 1);
        assert(db.durable_sequence() == prefix_sequence + 1);
        assert(db.get("prefix/keep") && *db.get("prefix/keep") == "P1");
        assert(!db.get("prefix/delete-later"));
        assert(db.get("crash/x") && *db.get("crash/x") == "X2");
        assert(db.get("crash/y") && *db.get("crash/y") == "Y2");
        assert(db.get("crash/z") && db.get("crash/z")->size() == 257);
        assert(db.get("legacy/c") && *db.get("legacy/c") == "C1");
        assert(capture_legacy(legacy) == legacy_before);
    }

    // Corruption inside a structurally complete new frame must be rejected,
    // never silently interpreted as a partial commit.
    if (frame.size() > 24) {
        const fs::path corrupt_wal = root / "corrupt.bdw4";
        auto corrupted = complete;
        corrupted[prefix.size() + frame.size() / 2] ^= 0x5Au;
        write_bytes(corrupt_wal, corrupted);
        bool rejected = false;
        try {
            bdr::v107::IntegratedDatabase db(legacy, corrupt_wal);
        } catch (...) {
            rejected = true;
        }
        assert(rejected);
        assert(capture_legacy(legacy) == legacy_before);
    }

    std::cout << "V109 PASS truncation_points=" << frame.size()
              << " complete_reopens=5"
              << " legacy_sequence=" << legacy_sequence
              << " committed_prefix_sequence=" << prefix_sequence << "\n";

    fs::remove_all(root, ec);
    return 0;
}
