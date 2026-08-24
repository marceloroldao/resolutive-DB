#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using Clock = std::chrono::steady_clock;

static inline uint64_t fnv1a64(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

static inline uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct Address {
    uint32_t rho_R;
    uint16_t phi;
    float theta;
    float f_nu;
    uint64_t fingerprint;
};

struct ResolutiveEncoder {
    uint32_t partitions;
    uint32_t phase_buckets;

    Address encode(const std::string& key) const {
        uint64_t h0 = mix64(fnv1a64(key));
        uint64_t h1 = mix64(h0 ^ 0xD6E8FEB86659FD93ULL);
        uint64_t h2 = mix64(h1 ^ 0xA5A35625AA5A3563ULL);

        Address a;
        a.rho_R = static_cast<uint32_t>(h0 % partitions);
        a.phi = static_cast<uint16_t>(h1 % phase_buckets);
        a.theta = static_cast<float>((h1 >> 11) *
            (6.2831853071795864769 / 9007199254740992.0));
        a.f_nu = static_cast<float>((h2 & 0xFFFFFFULL) /
            double(0x1000000ULL));
        a.fingerprint = h2;
        return a;
    }
};

struct NativeMap {
    std::unordered_map<std::string, uint64_t> map;

    void reserve(size_t n) { map.reserve(n * 2); }
    void insert(const std::string& k, uint64_t v) { map[k] = v; }
    bool get(const std::string& k, uint64_t& v) const {
        auto it = map.find(k);
        if (it == map.end()) return false;
        v = it->second;
        return true;
    }
};

struct FastPartitioned {
    uint32_t partitions;
    std::vector<std::unordered_map<uint64_t, uint64_t>> buckets;

    explicit FastPartitioned(uint32_t p) : partitions(p), buckets(p) {}

    void reserve_total(size_t n) {
        size_t r = (n / partitions) * 2 + 8;
        for (auto& b : buckets) b.reserve(r);
    }

    uint64_t h(const std::string& k) const { return mix64(fnv1a64(k)); }

    void insert(const std::string& k, uint64_t v) {
        uint64_t x = h(k);
        buckets[x % partitions][x] = v;
    }

    bool get(const std::string& k, uint64_t& v) const {
        uint64_t x = h(k);
        const auto& b = buckets[x % partitions];
        auto it = b.find(x);
        if (it == b.end()) return false;
        v = it->second;
        return true;
    }
};

struct ResolutiveBDR {
    ResolutiveEncoder enc;
    std::vector<std::unordered_map<uint64_t, uint64_t>> buckets;

    explicit ResolutiveBDR(uint32_t p, uint32_t ph) : enc{p, ph}, buckets(p) {}

    void reserve_total(size_t n) {
        size_t r = (n / enc.partitions) * 2 + 8;
        for (auto& b : buckets) b.reserve(r);
    }

    static uint64_t local_key(const Address& a) {
        return mix64(a.fingerprint ^ (uint64_t(a.phi) << 48));
    }

    void insert(const std::string& k, uint64_t v) {
        auto a = enc.encode(k);
        buckets[a.rho_R][local_key(a)] = v;
    }

    bool get(const std::string& k, uint64_t& v) const {
        auto a = enc.encode(k);
        const auto& b = buckets[a.rho_R];
        auto it = b.find(local_key(a));
        if (it == b.end()) return false;
        v = it->second;
        return true;
    }
};

struct Stats {
    double mean_us;
    double p50_us;
    double p99_us;
    double mops;
};

template <class DB>
Stats bench_hits(const DB& db, const std::vector<std::string>& keys, size_t q) {
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<size_t> pick(0, keys.size() - 1);
    std::vector<double> lat;
    lat.reserve(q);
    volatile uint64_t sink = 0;

    auto all0 = Clock::now();
    for (size_t i = 0; i < q; ++i) {
        const auto& k = keys[pick(rng)];
        uint64_t v = 0;
        auto t0 = Clock::now();
        bool ok = db.get(k, v);
        auto t1 = Clock::now();
        if (!ok) {
            std::cerr << "lookup failure\n";
            std::exit(2);
        }
        sink ^= v;
        lat.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    auto all1 = Clock::now();

    std::sort(lat.begin(), lat.end());
    double total = std::chrono::duration<double>(all1 - all0).count();
    double sum = std::accumulate(lat.begin(), lat.end(), 0.0);
    return {
        sum / q,
        lat[q / 2],
        lat[std::min(q - 1, static_cast<size_t>(std::floor(q * 0.99)))],
        q / total / 1e6
    };
}

template <class DB>
double build(DB& db, const std::vector<std::string>& keys) {
    auto t0 = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) db.insert(keys[i], i);
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

int main(int argc, char** argv) {
    std::vector<size_t> Ns = {100000, 1000000};
    if (argc > 1) {
        Ns.clear();
        for (int i = 1; i < argc; ++i) Ns.push_back(std::stoull(argv[i]));
    }

    const uint32_t PARTITIONS = 10000;
    const uint32_t PHASE_BUCKETS = 65536;
    const size_t QUERIES = 200000;

    std::cout << "N,engine,build_s,lookup_mean_us,p50_us,p99_us,Mops\n";

    for (size_t N : Ns) {
        std::vector<std::string> keys;
        keys.reserve(N);
        char buf[64];
        for (size_t i = 0; i < N; ++i) {
            std::snprintf(buf, sizeof(buf), "ETBRA_RESOLUTIVE_KEY_%012zu", i);
            keys.emplace_back(buf);
        }

        {
            NativeMap db;
            db.reserve(N);
            double b = build(db, keys);
            auto s = bench_hits(db, keys, std::min(QUERIES, N * 2));
            std::cout << N << ",native_unordered_map," << b << ','
                      << s.mean_us << ',' << s.p50_us << ',' << s.p99_us << ','
                      << s.mops << '\n';
        }

        {
            FastPartitioned db(PARTITIONS);
            db.reserve_total(N);
            double b = build(db, keys);
            auto s = bench_hits(db, keys, std::min(QUERIES, N * 2));
            std::cout << N << ",partitioned_fast_hash," << b << ','
                      << s.mean_us << ',' << s.p50_us << ',' << s.p99_us << ','
                      << s.mops << '\n';
        }

        {
            ResolutiveBDR db(PARTITIONS, PHASE_BUCKETS);
            db.reserve_total(N);
            double b = build(db, keys);
            auto s = bench_hits(db, keys, std::min(QUERIES, N * 2));
            std::cout << N << ",resolutive_rho_phi_fp," << b << ','
                      << s.mean_us << ',' << s.p50_us << ',' << s.p99_us << ','
                      << s.mops << '\n';
        }
    }
}
