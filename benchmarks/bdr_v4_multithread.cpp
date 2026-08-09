#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
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
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

struct Address { uint32_t rho; uint32_t phi; uint64_t sig; };
struct Encoder {
    uint32_t rho_count, phi_buckets;
    Address encode(const std::string& key) const {
        uint64_t h1 = mix64(fnv1a64(key, 1469598103934665603ULL));
        uint64_t h2 = mix64(fnv1a64(key, 1099511628211ULL));
        uint64_t h3 = mix64(h1 ^ (h2 << 1));
        return {(uint32_t)(h1 % rho_count), (uint32_t)(h2 % phi_buckets), h3};
    }
};

struct RHSlot { uint64_t key=0, value=0; uint32_t dist=0; bool used=false; };

class RobinHoodTable {
    std::vector<RHSlot> slots;
    size_t mask;
public:
    explicit RobinHoodTable(size_t c=8) : slots(c), mask(c-1) {}
    void insert(uint64_t key, uint64_t value) {
        size_t idx = mix64(key) & mask;
        RHSlot cur{key, value, 0, true};
        for (;;) {
            RHSlot& s = slots[idx];
            if (!s.used) { s = cur; return; }
            if (s.key == cur.key) { s.value = value; return; }
            if (s.dist < cur.dist) std::swap(s, cur);
            idx = (idx + 1) & mask;
            ++cur.dist;
        }
    }
    bool get(uint64_t key, uint64_t& value) const {
        size_t idx = mix64(key) & mask;
        uint32_t dist = 0;
        for (;;) {
            const RHSlot& s = slots[idx];
            if (!s.used || s.dist < dist) return false;
            if (s.key == key) { value = s.value; return true; }
            idx = (idx + 1) & mask;
            ++dist;
        }
    }
    size_t bytes() const { return slots.capacity() * sizeof(RHSlot); }
};

struct Partition {
    RobinHoodTable table;
    mutable std::shared_mutex mu;
    explicit Partition(size_t c) : table(c) {}
    Partition(Partition&& other) noexcept : table(std::move(other.table)) {}
    Partition& operator=(Partition&& other) noexcept { table = std::move(other.table); return *this; }
    Partition(const Partition&) = delete;
};

class BDRConcurrent {
    Encoder enc;
    std::vector<Partition> parts;
    uint64_t local_key(const Address& a) const { return ((uint64_t)a.phi << 48) ^ a.sig; }
public:
    BDRConcurrent(uint32_t m, uint32_t p, size_t n) : enc{m,p} {
        double avg = (double)n / m;
        size_t cap = 8;
        while (cap < (size_t)std::ceil((avg * 4.0 + 16.0) / 0.70)) cap <<= 1;
        parts.reserve(m);
        for (uint32_t i=0; i<m; ++i) parts.emplace_back(cap);
    }
    void insert(const std::string& k, uint64_t v) {
        Address a = enc.encode(k);
        Partition& pt = parts[a.rho];
        std::unique_lock<std::shared_mutex> g(pt.mu);
        pt.table.insert(local_key(a), v);
    }
    bool get(const std::string& k, uint64_t& v) const {
        Address a = enc.encode(k);
        const Partition& pt = parts[a.rho];
        std::shared_lock<std::shared_mutex> g(pt.mu);
        return pt.table.get(local_key(a), v);
    }
    size_t bytes() const {
        size_t b = parts.capacity() * sizeof(Partition);
        for (const auto& p : parts) b += p.table.bytes();
        return b;
    }
};

class FlatConcurrent {
    Encoder enc;
    mutable std::shared_mutex mu;
    std::unordered_map<uint64_t,uint64_t> map;
public:
    FlatConcurrent(uint32_t m, uint32_t p, size_t n) : enc{m,p} { map.reserve(n*2); }
    uint64_t flat_key(const std::string& k) const {
        Address a = enc.encode(k);
        return mix64(((uint64_t)a.rho << 32) ^ ((uint64_t)a.phi << 16) ^ a.sig);
    }
    void insert(const std::string& k, uint64_t v) {
        uint64_t x = flat_key(k);
        std::unique_lock<std::shared_mutex> g(mu);
        map[x] = v;
    }
    bool get(const std::string& k, uint64_t& v) const {
        uint64_t x = flat_key(k);
        std::shared_lock<std::shared_mutex> g(mu);
        auto it = map.find(x);
        if (it == map.end()) return false;
        v = it->second;
        return true;
    }
    size_t approx_bytes() const {
        return map.bucket_count()*sizeof(void*) + map.size()*(sizeof(std::pair<const uint64_t,uint64_t>) + 2*sizeof(void*));
    }
};

struct Result { double mops, p50, p95, p99, p999; };
static double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t i = (size_t)std::ceil(p*v.size()) - 1;
    if (i >= v.size()) i = v.size()-1;
    return v[i];
}

template<class DB>
Result bench(DB& db, const std::vector<std::string>& keys, int threads,
             int ops_per_thread, double write_ratio, uint64_t seed) {
    std::atomic<uint64_t> sink{0};
    std::vector<std::thread> workers;
    std::vector<std::vector<double>> latencies(threads);
    auto start = Clock::now();
    for (int t=0; t<threads; ++t) {
        workers.emplace_back([&,t] {
            std::mt19937_64 rng(seed + t*9973);
            std::uniform_int_distribution<size_t> pick(0, keys.size()-1);
            std::uniform_real_distribution<double> choose(0,1);
            auto& lv = latencies[t];
            lv.reserve(std::min(ops_per_thread, 5000));
            for (int i=0; i<ops_per_thread; ++i) {
                size_t idx = pick(rng);
                bool sample = i < 5000;
                auto t0 = sample ? Clock::now() : Clock::time_point{};
                uint64_t out = 0;
                if (choose(rng) < write_ratio) {
                    db.insert(keys[idx], (uint64_t)(idx+i+1));
                    out = idx;
                } else {
                    db.get(keys[idx], out);
                }
                if (sample) {
                    auto t1 = Clock::now();
                    lv.push_back(std::chrono::duration<double,std::nano>(t1-t0).count()/1000.0);
                }
                sink.fetch_xor(out, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : workers) th.join();
    auto end = Clock::now();
    std::vector<double> all;
    for (auto& v : latencies) all.insert(all.end(), v.begin(), v.end());
    double total = (double)threads * ops_per_thread;
    double seconds = std::chrono::duration<double>(end-start).count();
    return {total/seconds/1e6, percentile(all,.50), percentile(all,.95), percentile(all,.99), percentile(all,.999)};
}

int main() {
    constexpr size_t N = 300000;
    constexpr uint32_t M = 10000;
    constexpr uint32_t PHI = 65536;
    constexpr int OPS_PER_THREAD = 50000;

    std::vector<std::string> keys;
    keys.reserve(N);
    for (size_t i=0; i<N; ++i) {
        char b[48];
        std::snprintf(b, sizeof(b), "ETBRA_RESOLUTIVE_KEY_%08zu", i);
        keys.emplace_back(b);
    }

    BDRConcurrent bdr(M,PHI,N);
    FlatConcurrent flat(M,PHI,N);
    for (size_t i=0; i<N; ++i) { bdr.insert(keys[i],i); flat.insert(keys[i],i); }

    std::cout << "engine,mode,threads,mops,p50_us,p95_us,p99_us,p999_us,approx_bytes,bytes_per_record\n";
    for (double wr : {0.0, 0.1}) {
        for (int th : {1,2,4,8,16}) {
            Result rb = bench(bdr,keys,th,OPS_PER_THREAD,wr,42);
            Result rf = bench(flat,keys,th,OPS_PER_THREAD,wr,42);
            const char* mode = wr == 0.0 ? "read" : "mixed90r10w";
            std::cout << "BDR," << mode << ',' << th << ',' << rb.mops << ',' << rb.p50 << ',' << rb.p95 << ',' << rb.p99 << ',' << rb.p999 << ',' << bdr.bytes() << ',' << (double)bdr.bytes()/N << '\n';
            std::cout << "FlatHash," << mode << ',' << th << ',' << rf.mops << ',' << rf.p50 << ',' << rf.p95 << ',' << rf.p99 << ',' << rf.p999 << ',' << flat.approx_bytes() << ',' << (double)flat.approx_bytes()/N << '\n';
        }
    }
}
