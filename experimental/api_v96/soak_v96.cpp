#include "bdr/database.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

int main() {
    const fs::path dir = "v96_soak_db";
    fs::remove_all(dir);

    bdr::Options opt;
    opt.wal_batch = 512;
    opt.partition_count = 4096;
    opt.partition_max_load = 0.80;

    std::unordered_map<std::string, std::string> oracle;
    std::mt19937_64 rng(0xB0DDBA5EULL);

    constexpr std::size_t cycles = 100;
    constexpr std::size_t ops_per_cycle = 5000;

    for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
        auto db = bdr::Database::open(dir, opt);
        for (std::size_t i = 0; i < ops_per_cycle; ++i) {
            const std::uint64_t id = rng() % 20000;
            const std::string key = "k" + std::to_string(id);
            if ((rng() % 100) < 72) {
                const std::string value = "v" + std::to_string(cycle) + "_" + std::to_string(i) + "_" + std::to_string(rng());
                db->put(key, value);
                oracle[key] = value;
            } else {
                db->erase(key);
                oracle.erase(key);
            }
        }
        db->sync();
        if ((cycle % 5) == 0) db->checkpoint();
        db->close();

        auto reopened = bdr::Database::open(dir, opt);
        if (reopened->size() != oracle.size()) {
            std::cerr << "size mismatch at cycle " << cycle << " expected=" << oracle.size()
                      << " got=" << reopened->size() << "\n";
            return 2;
        }
        for (const auto& kv : oracle) {
            auto got = reopened->get(kv.first);
            if (!got || *got != kv.second) {
                std::cerr << "value mismatch at cycle " << cycle << " key=" << kv.first << "\n";
                return 3;
            }
        }
        if (reopened->last_sequence() != reopened->durable_sequence()) {
            std::cerr << "sequence durability mismatch at cycle " << cycle << "\n";
            return 4;
        }
        reopened->close();
    }

    std::cout << "V96 soak PASS cycles=" << cycles
              << " total_mutations=" << (cycles * ops_per_cycle)
              << " final_records=" << oracle.size() << "\n";
    return 0;
}
