#include "bdr/database.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static constexpr std::uint64_t RECORDS = 30000;

static std::string key(std::uint64_t i) { return "k-" + std::to_string(i); }
static std::string initial(std::uint64_t i) { return "v0-" + std::to_string(i); }
static std::string updated(std::uint64_t i) { return "v1-" + std::to_string(i); }
static std::string tail(std::uint64_t i) { return "tail-" + std::to_string(i); }

static std::uint64_t count_mod(std::uint64_t mod, std::uint64_t rem) {
    if (RECORDS <= rem) return 0;
    return 1 + (RECORDS - 1 - rem) / mod;
}

static std::uint64_t expected_sequence() {
    return RECORDS + count_mod(3,0) + count_mod(5,0) + count_mod(7,1) + count_mod(11,2);
}

static void write_dataset(const fs::path& dir) {
    fs::remove_all(dir);
    bdr::Options o;
    o.keep_size_preallocation = false;
    o.reserve_bytes = 0;
    auto db = bdr::Database::open(dir, o);

    bdr::Ticket last{};
    for (std::uint64_t i=0;i<RECORDS;++i) last = db->put(key(i), initial(i));
    db->wait(last);

    for (std::uint64_t i=0;i<RECORDS;i+=3) last = db->put(key(i), updated(i));
    for (std::uint64_t i=0;i<RECORDS;i+=5) last = db->erase(key(i));
    db->wait(last);
    db->checkpoint();

    // Leave a durable WAL tail after the checkpoint so the reader must
    // combine BDR3 snapshot replay with BDW3 WAL replay.
    for (std::uint64_t i=1;i<RECORDS;i+=7) last = db->put(key(i), tail(i));
    for (std::uint64_t i=2;i<RECORDS;i+=11) last = db->erase(key(i));
    db->wait(last);
    db->sync();

    if (db->durable_sequence() != expected_sequence())
        throw std::runtime_error("writer durable sequence mismatch");
    db->close();
}

static void verify_dataset(const fs::path& dir) {
    bdr::Options o;
    o.keep_size_preallocation = false;
    o.reserve_bytes = 0;
    auto db = bdr::Database::open(dir, o);

    if (db->durable_sequence() != expected_sequence())
        throw std::runtime_error("reader durable sequence mismatch");

    std::uint64_t expected_size = 0;
    for (std::uint64_t i=0;i<RECORDS;++i) {
        const auto got = db->get(key(i));
        const bool tail_delete = (i % 11 == 2);
        const bool tail_put = (i % 7 == 1);
        const bool checkpoint_delete = (i % 5 == 0);

        if (tail_delete || (!tail_put && checkpoint_delete)) {
            if (got) throw std::runtime_error("deleted key resurrected at " + std::to_string(i));
            continue;
        }

        ++expected_size;
        std::string expected;
        if (tail_put) expected = tail(i);
        else if (i % 3 == 0) expected = updated(i);
        else expected = initial(i);

        if (!got || *got != expected)
            throw std::runtime_error("value mismatch at " + std::to_string(i));
    }

    if (db->size() != expected_size)
        throw std::runtime_error("reader size mismatch");
    db->close();
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: cross_compat <write|verify> <dir>");
        const std::string mode = argv[1];
        const fs::path dir = argv[2];
        if (mode == "write") write_dataset(dir);
        else if (mode == "verify") verify_dataset(dir);
        else throw std::runtime_error("unknown mode");
        std::cout << "V1_CROSS_COMPAT PASS mode=" << mode
                  << " sequence=" << expected_sequence()
                  << " records=" << RECORDS << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "V1_CROSS_COMPAT FAIL: " << e.what() << "\n";
        return 1;
    }
}
