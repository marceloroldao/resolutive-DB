#include "bdr/atomic_database.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    const auto root = fs::temp_directory_path() / "bdr-v120-diagnostics";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    {
        auto db = bdr::AtomicDatabase::open(root);
        const auto r = db->put_many({
            {"memoria.topology.v1/node/a", "A"},
            {"memoria.topology.v1/event/12", "azul"},
            {"memoria.epistemic.v1/audit/12", "trusted"},
        }, bdr::DurabilityMode::BatchSync);
        assert(r.sequence == 1);
        assert(r.operations == 3);
        assert(r.durable);
        const auto live = db->diagnostics();
        assert(live.resident_records == 3);
        assert(live.last_sequence == 1);
        assert(live.durable_sequence == 1);
        assert(live.wal_bytes > 0);
    }

    const auto wal_path = root / "atomic.bdw4";
    const auto wal_bytes = fs::file_size(wal_path);

    {
        auto db = bdr::AtomicDatabase::open(root);
        const auto d = db->diagnostics();
        assert(d.wal_bytes == wal_bytes);
        assert(d.replayed_batches == 1);
        assert(d.replayed_operations == 3);
        assert(d.legacy_records == 0);
        assert(d.resident_records == 3);
        assert(d.legacy_sequence == 0);
        assert(d.last_sequence == 1);
        assert(d.durable_sequence == 1);
        assert(!d.repaired_torn_tail);
        assert(db->get("memoria.topology.v1/event/12").value() == "azul");

        std::cout << "wal_bytes=" << d.wal_bytes
                  << " replayed_batches=" << d.replayed_batches
                  << " replayed_operations=" << d.replayed_operations
                  << " legacy_load_us=" << d.legacy_load_us
                  << " wal_replay_us=" << d.wal_replay_us
                  << " resident_records=" << d.resident_records << '\n';
    }

    fs::remove_all(root, ec);
    return 0;
}
