#include "bdr/database.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static void require(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

int main() {
    const fs::path dir = "v86-contract-db";
    fs::remove_all(dir);

    bdr::Options opt;
    opt.reserve_bytes = 8ull * 1024ull * 1024ull;
    opt.wal_batch = 64;
    opt.partition_count = 64;
    opt.partition_max_load = 0.78;

    {
        auto db = bdr::Database::open(dir, opt);
        auto t1 = db->put("alpha", "1");
        auto t2 = db->put("beta", "2");
        db->wait(t2);
        require(db->durable_sequence() >= t2.sequence, "ticket not durable");
        require(db->get("alpha").value_or("") == "1", "alpha missing");
        require(db->get("beta").value_or("") == "2", "beta missing");

        db->put_sync("alpha", "updated");
        db->erase_sync("beta");
        require(db->get("alpha").value_or("") == "updated", "update failed");
        require(!db->contains("beta"), "delete failed");

        bool second_open_failed = false;
        try {
            auto second = bdr::Database::open(dir, opt);
            (void)second;
        } catch (const std::exception&) {
            second_open_failed = true;
        }
        require(second_open_failed, "process lock failed");

        db->checkpoint();
        require(db->last_sequence() == db->durable_sequence(), "sequence mismatch before close");
        db->close();
    }

    {
        auto db = bdr::Database::open(dir, opt);
        require(db->get("alpha").value_or("") == "updated", "reopen lost alpha");
        require(!db->contains("beta"), "reopen resurrected beta");
        db->put_sync("gamma", "3");
        require(db->get("gamma").value_or("") == "3", "post-reopen write failed");
        db->checkpoint();
        db->close();
    }

    {
        auto db = bdr::Database::open(dir, opt);
        require(db->get("alpha").value_or("") == "updated", "second reopen lost alpha");
        require(db->get("gamma").value_or("") == "3", "second reopen lost gamma");
        require(db->size() == 2, "unexpected cardinality");
        const auto st = db->index_stats();
        require(st.records == 2, "index stats mismatch");
        db->close();
    }

    fs::remove_all(dir);
    std::cout << "V86_CONTRACT_PASS\n";
    return 0;
}
