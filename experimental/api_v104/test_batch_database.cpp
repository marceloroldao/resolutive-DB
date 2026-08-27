#include "batch_database.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using bdr::v104::BatchDatabase;
using bdr::v104::DurabilityMode;
using bdr::v101::OpType;
using bdr::v101::Operation;

static fs::path temp_path(const char* name) {
    auto p = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove(p, ec);
    return p;
}

int main() {
    const auto p = temp_path("bdr-v104-api.bdw4");

    {
        BatchDatabase db(p);
        assert(db.last_sequence() == 0);
        assert(db.durable_sequence() == 0);

        auto r1 = db.write_batch({
            {OpType::Put, "mem/1/payload", "hello"},
            {OpType::Put, "mem/1/node/0", "alpha"},
            {OpType::Put, "mem/1/occ/0", "1"},
            {OpType::Put, "mem/1/meta", "org=demo"},
        });
        assert(r1.sequence == 1);
        assert(r1.operations == 4);
        assert(r1.durable);
        assert(db.last_sequence() == 1);
        assert(db.durable_sequence() == 1);
        assert(db.size() == 4);

        auto r2 = db.put_many({{"mem/2/payload", "world"}, {"mem/2/meta", "org=demo"}});
        assert(r2.sequence == 2);
        assert(r2.operations == 2);
        assert(r2.durable);
        assert(db.size() == 6);

        auto r3 = db.erase_many({"mem/1/node/0", "mem/1/occ/0"});
        assert(r3.sequence == 3);
        assert(r3.operations == 2);
        assert(r3.durable);
        assert(db.size() == 4);
        assert(!db.get("mem/1/node/0"));
        assert(!db.get("mem/1/occ/0"));

        // Async batches are visible immediately but are not claimed durable yet.
        auto r4 = db.put_many({{"async/a", "A"}, {"async/b", "B"}}, DurabilityMode::Async);
        assert(r4.sequence == 4);
        assert(!r4.durable);
        assert(db.last_sequence() == 4);
        assert(db.durable_sequence() == 3);
        assert(db.get("async/a").value() == "A");

        // A later BatchSync establishes a durability boundary covering the WAL prefix.
        auto r5 = db.put_many({{"sync/c", "C"}}, DurabilityMode::BatchSync);
        assert(r5.sequence == 5);
        assert(r5.durable);
        assert(db.last_sequence() == 5);
        assert(db.durable_sequence() == 5);
    }

    // Reopen must reconstruct all durably covered batches, including the async prefix
    // that was flushed by the later BatchSync boundary.
    {
        BatchDatabase db(p);
        assert(db.last_sequence() == 5);
        assert(db.durable_sequence() == 5);
        assert(db.get("mem/1/payload").value() == "hello");
        assert(db.get("mem/1/meta").value() == "org=demo");
        assert(!db.get("mem/1/node/0"));
        assert(!db.get("mem/1/occ/0"));
        assert(db.get("mem/2/payload").value() == "world");
        assert(db.get("async/a").value() == "A");
        assert(db.get("async/b").value() == "B");
        assert(db.get("sync/c").value() == "C");
    }

    bool empty_rejected = false;
    try {
        BatchDatabase db(temp_path("bdr-v104-empty.bdw4"));
        db.write_batch({});
    } catch (const std::invalid_argument&) {
        empty_rejected = true;
    }
    assert(empty_rejected);

    return 0;
}
