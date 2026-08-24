#include "bdr/database.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static void require(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

static fs::path active_wal(const fs::path& dir) {
    fs::path found;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".bdw3" && (found.empty() || e.path().filename() > found.filename()))
            found = e.path();
    }
    if (found.empty()) throw std::runtime_error("active WAL not found");
    return found;
}

int main() {
    fs::path dir = "v55-api-test-db";
    fs::remove_all(dir);

    bdr::Options opt;
    opt.reserve_bytes = 8ull * 1024ull * 1024ull;
    opt.wal_batch = 64;
    opt.keep_size_preallocation = true;

    uint64_t seq_before_tail = 0;
    {
        auto db = bdr::Database::open(dir, opt);
        auto t1 = db->put("alpha", "A");
        auto t2 = db->put("beta", "B");
        auto t3 = db->put("gamma", "C");

        require(db->get("alpha").value_or("") == "A", "async put not immediately visible");
        require(t1.sequence < t2.sequence && t2.sequence < t3.sequence, "tickets not monotonic");
        db->wait(t3);
        require(db->durable_sequence() >= t3.sequence, "wait did not establish durability");

        db->put_sync("delta", "D");
        db->erase_sync("beta");
        require(!db->get("beta").has_value(), "erase_sync did not remove key");

        db->checkpoint();
        auto t4 = db->put("epsilon", "E");
        db->wait(t4);
        seq_before_tail = db->last_sequence();
        require(db->durable_sequence() == seq_before_tail, "sync frontier mismatch");
        db->close();
    }

    {
        auto db = bdr::Database::open(dir, opt);
        require(db->get("alpha").value_or("") == "A", "alpha missing after reopen");
        require(!db->get("beta").has_value(), "deleted beta resurrected after reopen");
        require(db->get("gamma").value_or("") == "C", "gamma missing after reopen");
        require(db->get("delta").value_or("") == "D", "delta missing after reopen");
        require(db->get("epsilon").value_or("") == "E", "epsilon missing after reopen");
        require(db->last_sequence() == seq_before_tail, "sequence changed after clean reopen");
        db->close();
    }

    // Simulate a physically written but unconfirmed torn tail. No durable ticket
    // covers these bytes. Recovery must trim them and preserve every confirmed op.
    fs::path wal = active_wal(dir);
    {
        std::ofstream f(wal, std::ios::binary | std::ios::app);
        const char partial_len[3] = {0, 0, 64};
        f.write(partial_len, sizeof(partial_len));
    }

    {
        auto db = bdr::Database::open(dir, opt);
        require(db->get("epsilon").value_or("") == "E", "valid prefix lost after tail repair");
        require(db->last_sequence() == seq_before_tail, "tail repair changed durable sequence");
        db->put_sync("after-repair", "OK");
        db->checkpoint();
        db->close();
    }

    {
        auto db = bdr::Database::open(dir, opt);
        require(db->get("after-repair").value_or("") == "OK", "write after tail repair not durable");
        require(!db->get("beta").has_value(), "deleted key resurrected after second checkpoint");
        require(db->size() == 5, "unexpected final record count");
        db->close();
    }

    std::cout << "async_visibility,ticket_order,wait_durability,delete,reopen,checkpoint,torn_tail_repair,post_repair_write,pass\n"
              << "1,1,1,1,1,1,1,1,1\n";
    return 0;
}
