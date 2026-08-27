#include "bdr/database.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

static std::size_t env_size(const char* name, std::size_t def) {
    if (const char* v = std::getenv(name)) {
        try { return static_cast<std::size_t>(std::stoull(v)); } catch (...) {}
    }
    return def;
}

int main() {
    try {
        const std::size_t cycles = env_size("BDR_V1_CHURN_CYCLES", 200);
        const std::size_t ops_per_cycle = env_size("BDR_V1_CHURN_OPS_PER_CYCLE", 10000);
        const std::size_t key_space = env_size("BDR_V1_CHURN_KEY_SPACE", 50000);
        const fs::path dir = "v1_checkpoint_churn_db";
        fs::remove_all(dir);

        bdr::Options opt;
        opt.keep_size_preallocation = false;
        opt.reserve_bytes = 0;
        opt.wal_batch = 512;
        opt.partition_count = 4096;
        opt.partition_max_load = 0.80;

        std::unordered_map<std::string,std::string> oracle;
        oracle.reserve(key_space);
        std::mt19937_64 rng(0xBD0100C1ULL);
        std::uint64_t accepted_mutations = 0;

        for (std::size_t cycle=0; cycle<cycles; ++cycle) {
            auto db = bdr::Database::open(dir,opt);
            bdr::Ticket last{};

            for (std::size_t j=0; j<ops_per_cycle; ++j) {
                const auto id = rng() % key_space;
                const std::string k = "k-" + std::to_string(id);
                const auto sel = rng() % 100;
                if (sel < 72) {
                    const std::string v = "c" + std::to_string(cycle) + "-o" + std::to_string(j) + "-" + std::to_string(rng());
                    last = db->put(k,v);
                    oracle[k] = v;
                    ++accepted_mutations;
                } else if (sel < 94) {
                    last = db->erase(k);
                    oracle.erase(k);
                    ++accepted_mutations;
                } else {
                    const auto got = db->get(k);
                    const auto it = oracle.find(k);
                    const bool expected = it != oracle.end();
                    if (expected != static_cast<bool>(got) || (expected && *got != it->second))
                        throw std::runtime_error("online lookup mismatch cycle=" + std::to_string(cycle));
                }
            }

            if (last) db->wait(last);
            db->sync();
            db->checkpoint();
            if (db->last_sequence()!=accepted_mutations || db->durable_sequence()!=accepted_mutations)
                throw std::runtime_error("sequence mismatch before close cycle=" + std::to_string(cycle));
            db->close();

            auto reopened = bdr::Database::open(dir,opt);
            if (reopened->size()!=oracle.size())
                throw std::runtime_error("size mismatch after reopen cycle=" + std::to_string(cycle));
            if (reopened->last_sequence()!=accepted_mutations || reopened->durable_sequence()!=accepted_mutations)
                throw std::runtime_error("sequence mismatch after reopen cycle=" + std::to_string(cycle));

            // Full oracle validation every cycle: this intentionally makes the
            // gate sensitive to stale values, delete resurrection, and snapshot drift.
            for (const auto& [k,v] : oracle) {
                const auto got = reopened->get(k);
                if (!got || *got!=v)
                    throw std::runtime_error("oracle mismatch after reopen cycle=" + std::to_string(cycle));
            }
            reopened->close();
        }

        auto final_db = bdr::Database::open(dir,opt);
        if (final_db->size()!=oracle.size()) throw std::runtime_error("final size mismatch");
        if (final_db->durable_sequence()!=accepted_mutations) throw std::runtime_error("final sequence mismatch");
        final_db->close();

        std::cout << "V1_CHECKPOINT_CHURN PASS cycles=" << cycles
                  << " ops_per_cycle=" << ops_per_cycle
                  << " accepted_mutations=" << accepted_mutations
                  << " final_records=" << oracle.size() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "V1_CHECKPOINT_CHURN FAIL: " << e.what() << "\n";
        return 1;
    }
}
