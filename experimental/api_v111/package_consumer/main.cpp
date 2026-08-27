#include <bdr/database.hpp>
#include <bdr/atomic_database.hpp>

#include <cassert>
#include <filesystem>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "bdr_v111_external";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    // v1.0 source compatibility through the installed package.
    {
        bdr::Options options;
        options.reserve_bytes = 0;
        options.keep_size_preallocation = false;
        auto db = bdr::Database::open(root, options);
        db->put_sync("legacy", "ok");
        db->sync();
        assert(db->get("legacy") && *db->get("legacy") == "ok");
        db->close();
    }

    // v1.1 additive atomic surface through the same installed target.
    {
        auto db = bdr::AtomicDatabase::open(root);
        const auto r = db->put_many({{"a", "1"}, {"b", "2"}}, bdr::DurabilityMode::BatchSync);
        assert(r.durable && r.operations == 2);
        assert(db->get("legacy") && *db->get("legacy") == "ok");
        assert(db->get("a") && *db->get("a") == "1");
    }

    std::filesystem::remove_all(root, ec);
    return 0;
}
