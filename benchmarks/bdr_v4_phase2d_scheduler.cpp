#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using Clock = std::chrono::steady_clock;

static inline uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static inline uint64_t fnv1a64(const std::string& s, uint64_t seed=1469598103934665603ULL) {
    uint64_t h = seed;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

struct Address {
    uint32_t rho;
    uint32_t phi;
    uint64_t sig;
};

struct Encoder {
    uint32_t rho_count;
    uint32_t phi_buckets;

    Address encode(const std::string& key) const {
        const uint64_t h1 = mix64(fnv1a64(key, 1469598103934665603ULL));
        const uint64_t h2 = mix64(fnv1a64(key, 1099511628211ULL));
        const uint64_t h3 = mix64(h1 ^ (h2 << 1));
        return {
            static_cast<uint32_t>(h1 % rho_count),
            static_cast<uint32_t>(h2 % phi_buckets),
            h3,
        };
    }
};

class ShardedBaseline {
    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<uint64_t, uint64_t> map;
    };

    Encoder encoder;
    std::vector<Shard> shards;

    static uint64_t make_key(const Address& a) {
        return mix64((static_cast<uint64_t>(a.rho) << 32) ^
                     (static_cast<uint64_t>(a.phi) << 16) ^ a.sig);
    }

public:
    ShardedBaseline(uint32_t rho_count, uint32_t phi_buckets, size_t n, int shard_count)
        : encoder{rho_count, phi_buckets}, shards(shard_count) {
        for (auto& shard : shards) {
            shard.map.reserve(n / shard_count * 2 + 64);
        }
    }

    size_t route(const Address& a) const {
        return a.rho % shards.size();
    }

    void put(const std::string& key, uint64_t value) {
        const Address a = encoder.encode(key);
        auto& shard = shards[route(a)];
        std::unique_lock lock(shard.mutex);
        shard.map[make_key(a)] = value;
    }

    bool get(const std::string& key, uint64_t& value) const {
        const Address a = encoder.encode(key);
        auto& shard = shards[route(a)];
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(make_key(a));
        if (it == shard.map.end()) return false;
        value = it->second;
        return true;
    }
};

class Phase2DScheduler {
    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<uint64_t, uint64_t> map;
    };

    Encoder encoder;
    std::vector<Shard> shards;
    uint64_t salt;

    static uint64_t make_key(const Address& a) {
        return mix64((static_cast<uint64_t>(a.rho) << 32) ^
                     (static_cast<uint64_t>(a.phi) << 16) ^ a.sig);
    }

public:
    Phase2DScheduler(uint32_t rho_count, uint32_t phi_buckets, size_t n,
                     int shard_count, uint64_t routing_salt)
        : encoder{rho_count, phi_buckets}, shards(shard_count), salt(routing_salt) {
        for (auto& shard : shards) {
            shard.map.reserve(n / shard_count * 2 + 64);
        }
    }

    size_t route(const Address& a) const {
        const uint64_t g = mix64(static_cast<uint64_t>(a.rho) ^ salt);
        return static_cast<size_t>((a.phi + g) % shards.size());
    }

    void put(const std::string& key, uint64_t value) {
        const Address a = encoder.encode(key);
        auto& shard = shards[route(a)];
        std::unique_lock lock(shard.mutex);
        shard.map[make_key(a)] = value;
    }

    bool get(const std::string& key, uint64_t& value) const {
        const Address a = encoder.encode(key);
        auto& shard = shards[route(a)];
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(make_key(a));
        if (it == shard.map.end()) return false;
        value = it->second;
        return true;
    }
};

template <class DB>
static double run_workload(DB& db,
                           const std::vector<std::string>& keys,
                           int threads,
                           int ops_per_thread,
                           double write_ratio,
                           uint64_t seed) {
    std::vector<std::thread> workers;
    workers.reserve(threads);
    std::atomic<uint64_t> sink{0};

    const auto t0 = Clock::now();
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&, t] {
            std::mt19937_64 rng(seed + static_cast<uint64_t>(t) * 1315423911ULL);
            std::uniform_int_distribution<size_t> pick(0, keys.size() - 1);
            std::uniform_real_distribution<double> probability(0.0, 1.0);
            uint64_t value = 0;
            uint64_t local_sink = 0;

            for (int i = 0; i < ops_per_thread; ++i) {
                const size_t idx = pick(rng);
                if (probability(rng) < write_ratio) {
                    db.put(keys[idx], idx);
                } else if (db.get(keys[idx], value)) {
                    local_sink ^= value;
                }
            }
            sink.fetch_xor(local_sink, std::memory_order_relaxed);
        });
    }
    for (auto& worker : workers) worker.join();
    const auto t1 = Clock::now();

    const double seconds = std::chrono::duration<double>(t1 - t0).count();
    return static_cast<double>(threads * ops_per_thread) / seconds / 1e6;
}

int main() {
    constexpr size_t N = 300'000;
    constexpr uint32_t RHO_BUCKETS = 10'000;
    constexpr uint32_t PHI_BUCKETS = 65'536;
    constexpr int OPS_PER_THREAD = 50'000;
    constexpr int REPEATS = 7;

    std::vector<std::string> keys;
    keys.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "ETBRA_RESOLUTIVE_KEY_%08zu", i);
        keys.emplace_back(buffer);
    }

    std::cout << "threads,write_ratio,sharded_mops,phase2d_mops\n";

    for (int threads : {2, 4, 5}) {
        ShardedBaseline baseline(RHO_BUCKETS, PHI_BUCKETS, N, threads);
        Phase2DScheduler phase2d(RHO_BUCKETS, PHI_BUCKETS, N, threads,
                                 0xA5A5A5A55A5A5A5AULL);

        for (size_t i = 0; i < N; ++i) {
            baseline.put(keys[i], i);
            phase2d.put(keys[i], i);
        }

        for (double write_ratio : {0.0, 0.1}) {
            std::vector<double> baseline_runs;
            std::vector<double> phase2d_runs;
            baseline_runs.reserve(REPEATS);
            phase2d_runs.reserve(REPEATS);

            for (int r = 0; r < REPEATS; ++r) {
                const uint64_t seed = 42 + static_cast<uint64_t>(r) * 100;
                baseline_runs.push_back(run_workload(
                    baseline, keys, threads, OPS_PER_THREAD, write_ratio, seed));
                phase2d_runs.push_back(run_workload(
                    phase2d, keys, threads, OPS_PER_THREAD, write_ratio, seed));
            }

            std::sort(baseline_runs.begin(), baseline_runs.end());
            std::sort(phase2d_runs.begin(), phase2d_runs.end());
            const double baseline_median = baseline_runs[baseline_runs.size() / 2];
            const double phase2d_median = phase2d_runs[phase2d_runs.size() / 2];

            std::cout << threads << ',' << write_ratio << ','
                      << baseline_median << ',' << phase2d_median << '\n';
        }
    }
}
