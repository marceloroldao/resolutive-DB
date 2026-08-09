#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
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

struct RHSlot {
    uint64_t key = 0;
    uint64_t value = 0;
    uint32_t dist = 0;
    bool used = false;
};

class RobinHoodTable {
    std::vector<RHSlot> slots;
    size_t mask;

public:
    explicit RobinHoodTable(size_t capacity_pow2 = 8)
        : slots(capacity_pow2), mask(capacity_pow2 - 1) {}

    void insert(uint64_t key, uint64_t value) {
        size_t idx = mix64(key) & mask;
        RHSlot cur{key, value, 0, true};

        for (;;) {
            RHSlot& slot = slots[idx];
            if (!slot.used) {
                slot = cur;
                return;
            }
            if (slot.key == cur.key) {
                slot.value = value;
                return;
            }
            if (slot.dist < cur.dist) {
                std::swap(slot, cur);
            }
            idx = (idx + 1) & mask;
            ++cur.dist;
        }
    }

    bool get(uint64_t key, uint64_t& value) const {
        size_t idx = mix64(key) & mask;
        uint32_t dist = 0;

        for (;;) {
            const RHSlot& slot = slots[idx];
            if (!slot.used || slot.dist < dist) {
                return false;
            }
            if (slot.key == key) {
                value = slot.value;
                return true;
            }
            idx = (idx + 1) & mask;
            ++dist;
        }
    }
};

class BDRV4 {
    Encoder encoder;
    size_t per_bucket_capacity;
    std::vector<RobinHoodTable> buckets;

public:
    BDRV4(uint32_t rho_count, uint32_t phi_buckets, size_t expected_n)
        : encoder{rho_count, phi_buckets} {
        const double average = static_cast<double>(expected_n) / rho_count;
        size_t capacity = 8;

        // Conservative fixed local capacity for this benchmark. A production
        // engine should resize per partition or build from measured occupancy.
        while (capacity < static_cast<size_t>(std::ceil((average * 4.0 + 16.0) / 0.70))) {
            capacity <<= 1;
        }
        per_bucket_capacity = capacity;

        buckets.reserve(rho_count);
        for (uint32_t i = 0; i < rho_count; ++i) {
            buckets.emplace_back(capacity);
        }
    }

    void insert(const std::string& key, uint64_t value) {
        const Address a = encoder.encode(key);
        const uint64_t local_key = (static_cast<uint64_t>(a.phi) << 48) ^ a.sig;
        buckets[a.rho].insert(local_key, value);
    }

    bool get(const std::string& key, uint64_t& value) const {
        const Address a = encoder.encode(key);
        const uint64_t local_key = (static_cast<uint64_t>(a.phi) << 48) ^ a.sig;
        return buckets[a.rho].get(local_key, value);
    }

    size_t slot_count() const {
        return buckets.size() * per_bucket_capacity;
    }
};

struct FlatHash {
    Encoder encoder;
    std::unordered_map<uint64_t, uint64_t> map;

    FlatHash(uint32_t rho_count, uint32_t phi_buckets, size_t n)
        : encoder{rho_count, phi_buckets} {
        map.reserve(n * 2);
    }

    uint64_t flat_key(const std::string& key) const {
        const Address a = encoder.encode(key);
        return mix64((static_cast<uint64_t>(a.rho) << 32) ^
                     (static_cast<uint64_t>(a.phi) << 16) ^ a.sig);
    }

    void insert(const std::string& key, uint64_t value) {
        map[flat_key(key)] = value;
    }

    bool get(const std::string& key, uint64_t& value) const {
        auto it = map.find(flat_key(key));
        if (it == map.end()) {
            return false;
        }
        value = it->second;
        return true;
    }
};

static double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    size_t idx = static_cast<size_t>(std::ceil(p * values.size())) - 1;
    if (idx >= values.size()) {
        idx = values.size() - 1;
    }
    return values[idx];
}

static volatile uint64_t BENCH_SINK = 0;

template <class F>
static std::pair<double, double> latency_stats(F&& fn, const std::vector<std::string>& queries) {
    std::vector<double> ns;
    ns.reserve(queries.size());
    uint64_t out = 0;

    for (const auto& q : queries) {
        const auto t0 = Clock::now();
        const bool ok = fn(q, out);
        BENCH_SINK ^= (out + static_cast<uint64_t>(ok));
        const auto t1 = Clock::now();
        ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }

    std::vector<double> copy = ns;
    std::nth_element(copy.begin(), copy.begin() + copy.size() / 2, copy.end());
    return {
        copy[copy.size() / 2] / 1000.0,
        percentile(ns, 0.99) / 1000.0,
    };
}

int main() {
    // The checked-in default is intentionally bounded so the benchmark runs on
    // modest machines. Extend this vector to 1M/10M for larger hosts.
    const std::vector<size_t> sizes = {10'000, 100'000, 300'000};
    constexpr uint32_t M = 10'000;
    constexpr uint32_t PHI_BUCKETS = 65'536;
    constexpr int QUERY_COUNT = 5'000;

    std::mt19937_64 rng(42);

    std::cout
        << "N,lambda,bdr_insert_s,flat_insert_s,"
        << "bdr_hit_med_us,bdr_hit_p99_us,flat_hit_med_us,flat_hit_p99_us,"
        << "bdr_miss_med_us,bdr_miss_p99_us,flat_miss_med_us,flat_miss_p99_us,"
        << "fidelity\n";

    for (size_t n : sizes) {
        std::vector<std::string> keys;
        keys.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            char buffer[40];
            std::snprintf(buffer, sizeof(buffer), "ETBRA_RESOLUTIVE_KEY_%08zu", i);
            keys.emplace_back(buffer);
        }

        BDRV4 bdr(M, PHI_BUCKETS, n);
        FlatHash flat(M, PHI_BUCKETS, n);

        const auto b0 = Clock::now();
        for (size_t i = 0; i < n; ++i) {
            bdr.insert(keys[i], i);
        }
        const auto b1 = Clock::now();

        const auto f0 = Clock::now();
        for (size_t i = 0; i < n; ++i) {
            flat.insert(keys[i], i);
        }
        const auto f1 = Clock::now();

        std::uniform_int_distribution<size_t> dist(0, n - 1);
        std::vector<std::string> hit_queries;
        std::vector<std::string> miss_queries;
        hit_queries.reserve(QUERY_COUNT);
        miss_queries.reserve(QUERY_COUNT);

        for (int i = 0; i < QUERY_COUNT; ++i) {
            hit_queries.push_back(keys[dist(rng)]);
            miss_queries.push_back("MISS_KEY_" + std::to_string(n) + "_" + std::to_string(i));
        }

        const auto bdr_hit = latency_stats(
            [&](const std::string& q, uint64_t& v) { return bdr.get(q, v); },
            hit_queries);
        const auto flat_hit = latency_stats(
            [&](const std::string& q, uint64_t& v) { return flat.get(q, v); },
            hit_queries);
        const auto bdr_miss = latency_stats(
            [&](const std::string& q, uint64_t& v) { return bdr.get(q, v); },
            miss_queries);
        const auto flat_miss = latency_stats(
            [&](const std::string& q, uint64_t& v) { return flat.get(q, v); },
            miss_queries);

        size_t ok = 0;
        uint64_t value = 0;
        for (int i = 0; i < 1'000; ++i) {
            const size_t idx = dist(rng);
            if (bdr.get(keys[idx], value) && value == idx) {
                ++ok;
            }
        }

        std::cout
            << n << ','
            << static_cast<double>(n) / M << ','
            << std::chrono::duration<double>(b1 - b0).count() << ','
            << std::chrono::duration<double>(f1 - f0).count() << ','
            << bdr_hit.first << ',' << bdr_hit.second << ','
            << flat_hit.first << ',' << flat_hit.second << ','
            << bdr_miss.first << ',' << bdr_miss.second << ','
            << flat_miss.first << ',' << flat_miss.second << ','
            << (100.0 * ok / 1000.0) << '\n';
    }

    return static_cast<int>(BENCH_SINK & 0U);
}
