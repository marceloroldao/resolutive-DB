#include "bdr/database.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
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
    const std::size_t total_ops = env_size("BDR_SCALE_OPS", 1000000);
    const std::size_t cycle_ops = env_size("BDR_SCALE_CYCLE_OPS", 10000);
    const std::size_t key_space = env_size("BDR_SCALE_KEY_SPACE", 100000);
    const std::size_t checkpoint_every = env_size("BDR_SCALE_CHECKPOINT_EVERY_CYCLES", 10);

    const fs::path dir = "v03_scale_db";
    fs::remove_all(dir);

    bdr::Options opt;
    opt.wal_batch = 512;
    opt.partition_count = 4096;
    opt.partition_max_load = 0.80;

    std::unordered_map<std::string, std::string> oracle;
    oracle.reserve(key_space);
    std::mt19937_64 rng(0xBD0301ULL);

    std::size_t done = 0;
    std::size_t cycle = 0;
    while (done < total_ops) {
        auto db = bdr::Database::open(dir, opt);
        const std::size_t n = std::min(cycle_ops, total_ops - done);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t id = rng() % key_space;
            const std::string key = "k" + std::to_string(id);
            const std::uint64_t selector = rng() % 100;
            if (selector < 70) {
                const std::string value = "v" + std::to_string(done + i) + "_" + std::to_string(rng());
                db->put(key, value);
                oracle[key] = value;
            } else if (selector < 90) {
                db->erase(key);
                oracle.erase(key);
            } else {
                auto got = db->get(key);
                auto it = oracle.find(key);
                const bool expect = it != oracle.end();
                if (expect != static_cast<bool>(got) || (expect && *got != it->second)) {
                    std::cerr << "online lookup mismatch cycle=" << cycle << " key=" << key << "\n";
                    return 2;
                }
            }
        }
        done += n;
        db->sync();
        if (checkpoint_every && ((cycle + 1) % checkpoint_every == 0)) db->checkpoint();
        db->close();

        auto reopened = bdr::Database::open(dir, opt);
        if (reopened->size() != oracle.size()) {
            std::cerr << "size mismatch cycle=" << cycle << " expected=" << oracle.size()
                      << " got=" << reopened->size() << "\n";
            return 3;
        }
        for (const auto& kv : oracle) {
            auto got = reopened->get(kv.first);
            if (!got || *got != kv.second) {
                std::cerr << "value mismatch cycle=" << cycle << " key=" << kv.first << "\n";
                return 4;
            }
        }
        if (reopened->last_sequence() != reopened->durable_sequence()) {
            std::cerr << "sequence mismatch cycle=" << cycle << " last=" << reopened->last_sequence()
                      << " durable=" << reopened->durable_sequence() << "\n";
            return 5;
        }
        reopened->close();
        ++cycle;
    }

    std::cout << "V03_SCALE PASS ops=" << total_ops
              << " cycles=" << cycle
              << " final_records=" << oracle.size() << "\n";
    return 0;
}
