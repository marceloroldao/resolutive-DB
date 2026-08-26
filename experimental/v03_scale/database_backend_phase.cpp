#include "bdr/database.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;

static std::string value_for(std::size_t i) {
    std::string v(32, char('a' + (i % 26)));
    v += ":" + std::to_string(i);
    return v;
}

static double elapsed(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("usage: database_backend_phase <insert|prepare|reopen|checkpoint>");
    const std::string mode = argv[1];
    const std::size_t n = std::strtoull(std::getenv("BDR_PHASE_RECORDS") ? std::getenv("BDR_PHASE_RECORDS") : "1000000", nullptr, 10);
    const char* label_env = std::getenv("BDR_BACKEND_LABEL");
    const std::string label = label_env ? label_env : "unknown";
    const fs::path dir = "v03-phase-" + label + "-" + std::to_string(n);

    bdr::Options options;
    options.reserve_bytes = 0;
    options.keep_size_preallocation = false;
    options.wal_batch = 512;
    options.partition_count = 4096;
    options.partition_max_load = 0.78;

    if (mode == "insert" || mode == "prepare") {
        fs::remove_all(dir);
        auto db = bdr::Database::open(dir, options);
        auto t0 = Clock::now();
        for (std::size_t i = 0; i < n; ++i) db->put("k" + std::to_string(i), value_for(i));
        db->sync();
        const double insert_s = elapsed(t0);
        if (db->size() != n || db->durable_sequence() != n) throw std::runtime_error("insert state mismatch");
        for (std::size_t i = 0; i < std::min<std::size_t>(10000, n); ++i) {
            const std::size_t id = (i * 9973ULL) % n;
            auto got = db->get("k" + std::to_string(id));
            if (!got || *got != value_for(id)) throw std::runtime_error("insert verify mismatch");
        }
        std::cout << "DATABASE_PHASE PASS backend=" << label << " mode=insert records=" << n
                  << " seconds=" << insert_s << " ops_per_sec=" << (n / insert_s) << "\n";
        if (mode == "prepare") {
            t0 = Clock::now();
            db->checkpoint();
            std::cout << "DATABASE_PHASE PASS backend=" << label << " mode=prepare_checkpoint records=" << n
                      << " seconds=" << elapsed(t0) << "\n";
        }
        db->close();
        return 0;
    }

    if (mode == "reopen") {
        auto t0 = Clock::now();
        auto db = bdr::Database::open(dir, options);
        const double open_s = elapsed(t0);
        if (db->size() != n || db->durable_sequence() != n) throw std::runtime_error("reopen state mismatch");
        for (std::size_t i = 0; i < std::min<std::size_t>(10000, n); ++i) {
            const std::size_t id = (i * 9973ULL) % n;
            auto got = db->get("k" + std::to_string(id));
            if (!got || *got != value_for(id)) throw std::runtime_error("reopen verify mismatch");
        }
        std::cout << "DATABASE_PHASE PASS backend=" << label << " mode=reopen records=" << n
                  << " seconds=" << open_s << "\n";
        db->close();
        return 0;
    }

    if (mode == "checkpoint") {
        auto db = bdr::Database::open(dir, options);
        if (db->size() != n) throw std::runtime_error("checkpoint reopen mismatch");
        auto t0 = Clock::now();
        db->checkpoint();
        std::cout << "DATABASE_PHASE PASS backend=" << label << " mode=checkpoint records=" << n
                  << " seconds=" << elapsed(t0) << "\n";
        db->close();
        return 0;
    }

    throw std::runtime_error("unknown mode");
}
