#include "durable_database.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using bdr::v101::OpType;
using bdr::v101::Operation;
using bdr::v110::DurabilityMode;

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_v110_durability";
    const fs::path legacy = root / "legacy";
    const fs::path wal = root / "v11.bdw4";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(legacy);

    std::uint64_t final_sequence = 0;
    {
        bdr::v110::DurableDatabase db(legacy, wal);
        assert(db.last_sequence() == 0);
        assert(db.durable_sequence() == 0);

        auto a = db.write_batch({
            {OpType::Put, "async/a", "A"},
            {OpType::Put, "async/b", "B"},
        }, DurabilityMode::Async);
        assert(a.sequence == 1 && !a.durable && a.operations == 2);
        assert(db.last_sequence() == 1);
        assert(db.durable_sequence() == 0);

        auto b = db.put("async/c", "C", DurabilityMode::Async);
        assert(b.sequence == 2 && !b.durable);
        assert(db.last_sequence() == 2);
        assert(db.durable_sequence() == 0);

        db.sync();
        assert(db.durable_sequence() == 2);

        auto c = db.put_many({{"batch/d", "D"}, {"batch/e", "E"}}, DurabilityMode::BatchSync);
        assert(c.sequence == 3 && c.durable && c.operations == 2);
        assert(db.durable_sequence() == 3);

        auto d = db.put("single/f", "F", DurabilityMode::PerOperationSync);
        assert(d.sequence == 4 && d.durable && d.operations == 1);
        assert(db.durable_sequence() == 4);

        bool rejected = false;
        try {
            db.write_batch({
                {OpType::Put, "bad/1", "x"},
                {OpType::Put, "bad/2", "y"},
            }, DurabilityMode::PerOperationSync);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        assert(db.last_sequence() == 4);

        auto e = db.erase_many({"async/a", "batch/d"}, DurabilityMode::BatchSync);
        assert(e.sequence == 5 && e.durable && e.operations == 2);
        assert(!db.get("async/a"));
        assert(!db.get("batch/d"));
        assert(db.get("async/b") && *db.get("async/b") == "B");
        final_sequence = e.sequence;
    }

    {
        bdr::v110::DurableDatabase reopened(legacy, wal);
        assert(reopened.last_sequence() == final_sequence);
        assert(reopened.durable_sequence() == final_sequence);
        assert(!reopened.get("async/a"));
        assert(reopened.get("async/b") && *reopened.get("async/b") == "B");
        assert(reopened.get("async/c") && *reopened.get("async/c") == "C");
        assert(!reopened.get("batch/d"));
        assert(reopened.get("batch/e") && *reopened.get("batch/e") == "E");
        assert(reopened.get("single/f") && *reopened.get("single/f") == "F");
    }

    std::cout << "V110 PASS final_sequence=" << final_sequence << "\n";
    fs::remove_all(root, ec);
    return 0;
}
