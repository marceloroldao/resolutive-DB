#include "durable_database.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using bdr::v101::OpType;
using bdr::v110::DurabilityMode;

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_rc1_per_operation_multiop_repro";
    const fs::path legacy = root / "legacy";
    const fs::path wal = root / "repro.bdw4";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(legacy);

    {
        bdr::v110::DurableDatabase db(legacy, wal);

        const auto async_result = db.write_batch({
            {OpType::Put, "async/a", "A"},
            {OpType::Put, "async/b", "B"},
        }, DurabilityMode::Async);
        assert(async_result.operations == 2);
        db.sync();
        assert(db.durable_sequence() == db.last_sequence());

        const auto batch_result = db.write_batch({
            {OpType::Put, "batch/a", "A"},
            {OpType::Put, "batch/b", "B"},
        }, DurabilityMode::BatchSync);
        assert(batch_result.operations == 2);
        assert(batch_result.durable);
        assert(db.durable_sequence() == db.last_sequence());

        const auto single_result = db.write_batch({
            {OpType::Put, "per/single", "OK"},
        }, DurabilityMode::PerOperationSync);
        assert(single_result.operations == 1);
        assert(single_result.durable);

        const auto sequence_before = db.last_sequence();
        bool reproduced = false;
        try {
            (void)db.write_batch({
                {OpType::Put, "per/a", "A"},
                {OpType::Put, "per/b", "B"},
            }, DurabilityMode::PerOperationSync);
        } catch (const std::invalid_argument& ex) {
            reproduced = std::string(ex.what()) == "PerOperationSync requires exactly one operation";
        }
        assert(reproduced);
        assert(db.last_sequence() == sequence_before);
        assert(!db.get("per/a"));
        assert(!db.get("per/b"));
    }

    {
        bdr::v110::DurableDatabase reopened(legacy, wal);
        assert(reopened.get("async/a") && *reopened.get("async/a") == "A");
        assert(reopened.get("async/b") && *reopened.get("async/b") == "B");
        assert(reopened.get("batch/a") && *reopened.get("batch/a") == "A");
        assert(reopened.get("batch/b") && *reopened.get("batch/b") == "B");
        assert(reopened.get("per/single") && *reopened.get("per/single") == "OK");
        assert(!reopened.get("per/a"));
        assert(!reopened.get("per/b"));
        assert(reopened.durable_sequence() == reopened.last_sequence());
    }

    std::cout << "RC1 REPRO PASS: multi-op PerOperationSync rejected atomically\n";
    fs::remove_all(root, ec);
    return 0;
}
