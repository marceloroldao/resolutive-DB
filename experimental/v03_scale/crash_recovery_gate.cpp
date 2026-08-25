#include "bdr/database.hpp"

#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

static void append_manifest(const fs::path& path, const std::string& line) {
    const int fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) _exit(91);
    const std::string data = line + "\n";
    const ssize_t n = ::write(fd, data.data(), data.size());
    if (n != static_cast<ssize_t>(data.size())) { ::close(fd); _exit(92); }
    if (::fsync(fd) != 0) { ::close(fd); _exit(93); }
    ::close(fd);
}

static std::map<std::string,std::string> load_manifest(const fs::path& path) {
    std::map<std::string,std::string> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        const auto p = line.find('\t');
        if (p == std::string::npos) continue;
        out[line.substr(0, p)] = line.substr(p + 1);
    }
    return out;
}

int main() {
    const fs::path dir = "v03_crash_db";
    const fs::path manifest = "v03_crash_manifest.tsv";
    fs::remove_all(dir);
    fs::remove(manifest);

    bdr::Options opt;
    opt.wal_batch = 64;
    opt.partition_count = 4096;
    opt.partition_max_load = 0.80;

    std::mt19937 rng(0xC0FFEEu);
    constexpr int rounds = 50;
    constexpr int writes_per_child = 2000;

    for (int round = 0; round < rounds; ++round) {
        const pid_t pid = ::fork();
        if (pid < 0) {
            std::cerr << "fork failed\n";
            return 2;
        }
        if (pid == 0) {
            try {
                auto db = bdr::Database::open(dir, opt);
                for (int i = 0; i < writes_per_child; ++i) {
                    const std::string key = "r" + std::to_string(round) + "_k" + std::to_string(i);
                    const std::string value = "value_" + std::to_string(round) + "_" + std::to_string(i);
                    const auto ticket = db->put(key, value);
                    db->wait(ticket);
                    append_manifest(manifest, key + "\t" + value);
                }
                db->close();
                _exit(0);
            } catch (...) {
                _exit(94);
            }
        }

        const useconds_t delay_us = 1000u + static_cast<useconds_t>(rng() % 20000u);
        ::usleep(delay_us);
        ::kill(pid, SIGKILL);
        int status = 0;
        ::waitpid(pid, &status, 0);

        const auto expected = load_manifest(manifest);
        auto reopened = bdr::Database::open(dir, opt);
        for (const auto& kv : expected) {
            auto got = reopened->get(kv.first);
            if (!got || *got != kv.second) {
                std::cerr << "durable key missing after crash round=" << round << " key=" << kv.first << "\n";
                return 3;
            }
        }
        if (reopened->last_sequence() != reopened->durable_sequence()) {
            std::cerr << "sequence mismatch after crash round=" << round
                      << " last=" << reopened->last_sequence()
                      << " durable=" << reopened->durable_sequence() << "\n";
            return 4;
        }
        if ((round + 1) % 5 == 0) reopened->checkpoint();
        reopened->close();

        std::cout << "round=" << round
                  << " kill_delay_us=" << delay_us
                  << " durable_manifest_keys=" << expected.size() << " PASS\n";
    }

    const auto expected = load_manifest(manifest);
    auto db = bdr::Database::open(dir, opt);
    for (const auto& kv : expected) {
        auto got = db->get(kv.first);
        if (!got || *got != kv.second) return 5;
    }
    db->sync();
    db->checkpoint();
    db->close();

    std::cout << "V03_CRASH PASS rounds=" << rounds
              << " durable_manifest_keys=" << expected.size() << "\n";
    return 0;
}
