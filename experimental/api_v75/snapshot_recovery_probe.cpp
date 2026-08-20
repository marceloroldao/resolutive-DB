#include "bdr/database.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <sys/resource.h>

namespace fs = std::filesystem;

static long max_rss_kb() {
    struct rusage u{};
    if (getrusage(RUSAGE_SELF, &u) != 0) return -1;
    return u.ru_maxrss;
}

static std::string key_for(std::size_t i) {
    return "v75-key-" + std::to_string(i);
}

static std::string value_for(std::size_t i) {
    return "payload-" + std::to_string(i) + std::string(48, char('a' + (i % 26)));
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: snapshot_recovery_probe <prepare|recover> <path> <N>\n";
        return 2;
    }
    const std::string mode = argv[1];
    const fs::path root = argv[2];
    const std::size_t n = std::stoull(argv[3]);

    bdr::Options opt;
    opt.reserve_bytes = 0;
    opt.wal_batch = 512;
    opt.partition_count = 4096;
    opt.partition_max_load = 0.78;

    if (mode == "prepare") {
        fs::remove_all(root);
        auto db = bdr::Database::open(root, opt);
        bdr::Ticket last{};
        for (std::size_t i = 0; i < n; ++i) {
            last = db->put(key_for(i), value_for(i));
            if ((i & 127u) == 127u) db->wait(last);
        }
        db->wait(last);
        db->checkpoint();
        if (db->size() != n) return 3;
        db->close();
        const auto snap = root / "snapshot.bdr3";
        std::cout << "prepare_n=" << n << ",snapshot_bytes=" << fs::file_size(snap) << "\n";
        return 0;
    }

    if (mode == "recover") {
        const long before = max_rss_kb();
        auto db = bdr::Database::open(root, opt);
        const long after = max_rss_kb();
        if (db->size() != n) {
            std::cerr << "size mismatch: " << db->size() << " vs " << n << "\n";
            return 4;
        }
        const std::size_t samples[] = {0, n/7, n/3, n/2, n ? n-1 : 0};
        for (auto i : samples) {
            if (n == 0) break;
            auto got = db->get(key_for(i));
            if (!got || *got != value_for(i)) {
                std::cerr << "payload mismatch at " << i << "\n";
                return 5;
            }
        }
        if (db->last_sequence() != db->durable_sequence()) {
            std::cerr << "frontier mismatch\n";
            return 6;
        }
        std::cout << "recover_n=" << n
                  << ",rss_before_kb=" << before
                  << ",rss_peak_kb=" << after
                  << ",sequence=" << db->last_sequence() << "\n";
        db->close();
        return 0;
    }

    std::cerr << "unknown mode\n";
    return 7;
}
