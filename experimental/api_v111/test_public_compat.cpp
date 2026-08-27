#include "bdr/database.hpp"
#include "bdr/atomic_database.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_v111_public_compat";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    // Frozen v1.0 source surface: this block intentionally uses only calls
    // available in v1.0 and must continue to compile unchanged.
    {
        bdr::Options options;
        options.reserve_bytes = 0;
        options.keep_size_preallocation = false;
        auto db = bdr::Database::open(root, options);
        auto t1 = db->put("legacy-a", "A");
        db->wait(t1);
        db->put_sync("legacy-b", "B");
        db->erase_sync("legacy-a");
        db->sync();
        assert(!db->contains("legacy-a"));
        assert(db->get("legacy-b") && *db->get("legacy-b") == "B");
        db->close();
    }

    // Additive v1.1 public atomic surface.
    {
        auto db = bdr::AtomicDatabase::open(root);
        const auto base = db->last_sequence();

        std::vector<bdr::Operation> ops = {
            {bdr::OperationType::Put, "memory/payload", "P"},
            {bdr::OperationType::Put, "memory/node/1", "N1"},
            {bdr::OperationType::Put, "memory/meta", "M"},
        };
        const auto r1 = db->write_batch(std::move(ops), bdr::DurabilityMode::BatchSync);
        assert(r1.sequence == base + 1);
        assert(r1.operations == 3);
        assert(r1.durable);
        assert(db->durable_sequence() == r1.sequence);

        const auto r2 = db->put("async", "visible", bdr::DurabilityMode::Async);
        assert(!r2.durable);
        assert(db->last_sequence() == r2.sequence);
        assert(db->durable_sequence() == r1.sequence);
        db->sync();
        assert(db->durable_sequence() == r2.sequence);

        const auto r3 = db->put_many({{"bulk-a", "1"}, {"bulk-b", "2"}});
        assert(r3.durable && r3.operations == 2);
        assert(db->contains("bulk-a") && db->contains("bulk-b"));

        bool rejected = false;
        try {
            db->write_batch({
                {bdr::OperationType::Put, "x", "1"},
                {bdr::OperationType::Put, "y", "2"},
            }, bdr::DurabilityMode::PerOperationSync);
        } catch (...) {
            rejected = true;
        }
        assert(rejected);
    }

    // Reopen must reconstruct legacy + BDW4 state through the public API.
    {
        auto db = bdr::AtomicDatabase::open(root);
        assert(db->get("legacy-b") && *db->get("legacy-b") == "B");
        assert(db->get("memory/payload") && *db->get("memory/payload") == "P");
        assert(db->get("memory/node/1") && *db->get("memory/node/1") == "N1");
        assert(db->get("memory/meta") && *db->get("memory/meta") == "M");
        assert(db->get("async") && *db->get("async") == "visible");
        assert(db->get("bulk-b") && *db->get("bulk-b") == "2");
    }

    fs::remove_all(root, ec);
    return 0;
}
