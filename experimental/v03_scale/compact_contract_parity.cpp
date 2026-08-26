#include "bdr/resolutive_index.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

static inline std::uint64_t mix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static inline std::uint64_t fnv1a64(const std::string& s,
                                    std::uint64_t seed = 1469598103934665603ULL) noexcept {
    std::uint64_t h = seed;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

static std::size_t next_pow2(std::size_t n) {
    std::size_t p = 8;
    while (p < n) p <<= 1;
    return p;
}

class CompactIndex {
    struct Slot {
        std::uint64_t fingerprint = 0;
        std::uint32_t key_off = 0;
        std::uint32_t value_off = 0;
        std::uint32_t key_len = 0;
        std::uint32_t value_len = 0;
        std::uint32_t dist = 0;
        bool used = false;
    };

    struct Partition {
        mutable std::shared_mutex mu;
        std::vector<Slot> slots{8};
        std::vector<char> arena;
        std::size_t records = 0;
        std::size_t garbage_bytes = 0;
        std::size_t compactions = 0;
        bool reclaim = true;

        std::string_view key_view(const Slot& s) const {
            return {arena.data() + s.key_off, s.key_len};
        }
        std::string value_copy(const Slot& s) const {
            return std::string(arena.data() + s.value_off, s.value_len);
        }
        std::string key_copy(const Slot& s) const {
            return std::string(arena.data() + s.key_off, s.key_len);
        }
        static std::size_t start(std::uint64_t fp, std::size_t mask) noexcept {
            return std::size_t(mix64(fp)) & mask;
        }

        Slot append(std::uint64_t fp, const std::string& key, const std::string& value) {
            if (arena.size() + key.size() + value.size() > 0xffffffffULL)
                throw std::runtime_error("compact arena offset overflow");
            Slot s;
            s.fingerprint = fp;
            s.used = true;
            s.key_off = static_cast<std::uint32_t>(arena.size());
            s.key_len = static_cast<std::uint32_t>(key.size());
            arena.insert(arena.end(), key.begin(), key.end());
            s.value_off = static_cast<std::uint32_t>(arena.size());
            s.value_len = static_cast<std::uint32_t>(value.size());
            arena.insert(arena.end(), value.begin(), value.end());
            return s;
        }

        void compact_arena() {
            std::vector<char> next;
            next.reserve(arena.size() - std::min(arena.size(), garbage_bytes));
            for (auto& s : slots) {
                if (!s.used) continue;
                const std::string k = key_copy(s);
                const std::string v = value_copy(s);
                s.key_off = static_cast<std::uint32_t>(next.size());
                next.insert(next.end(), k.begin(), k.end());
                s.value_off = static_cast<std::uint32_t>(next.size());
                next.insert(next.end(), v.begin(), v.end());
            }
            arena.swap(next);
            garbage_bytes = 0;
            ++compactions;
        }

        void maybe_compact() {
            if (reclaim && garbage_bytes >= 8192 && garbage_bytes * 2 >= arena.size()) compact_arena();
        }

        bool insert_nolock(std::uint64_t fp, const std::string& key, const std::string& value) {
            const std::size_t mask = slots.size() - 1;
            std::size_t idx = start(fp, mask);
            Slot cur = append(fp, key, value);
            for (;;) {
                Slot& slot = slots[idx];
                if (!slot.used) {
                    slot = cur;
                    ++records;
                    return true;
                }
                if (slot.fingerprint == fp && key_view(slot) == key) {
                    garbage_bytes += slot.key_len + slot.value_len;
                    slot = cur;
                    maybe_compact();
                    return false;
                }
                if (slot.dist < cur.dist) std::swap(slot, cur);
                idx = (idx + 1) & mask;
                if (++cur.dist >= slots.size()) throw std::runtime_error("compact partition overflow");
            }
        }

        void rehash(std::size_t new_cap) {
            auto old = std::move(slots);
            slots.assign(next_pow2(new_cap), Slot{});
            records = 0;
            for (auto s : old) {
                if (!s.used) continue;
                const std::size_t mask = slots.size() - 1;
                std::size_t idx = start(s.fingerprint, mask);
                s.dist = 0;
                for (;;) {
                    Slot& slot = slots[idx];
                    if (!slot.used) {
                        slot = s;
                        ++records;
                        break;
                    }
                    if (slot.dist < s.dist) std::swap(slot, s);
                    idx = (idx + 1) & mask;
                    ++s.dist;
                }
            }
        }

        bool put(std::uint64_t fp, const std::string& key, const std::string& value, double max_load) {
            std::unique_lock lk(mu);
            if (double(records + 1) / double(slots.size()) > max_load) rehash(slots.size() * 2);
            return insert_nolock(fp, key, value);
        }

        std::optional<std::string> get(std::uint64_t fp, const std::string& key) const {
            std::shared_lock lk(mu);
            const std::size_t mask = slots.size() - 1;
            std::size_t idx = start(fp, mask);
            std::uint32_t dist = 0;
            for (;;) {
                const Slot& slot = slots[idx];
                if (!slot.used || slot.dist < dist) return std::nullopt;
                if (slot.fingerprint == fp && key_view(slot) == key) return value_copy(slot);
                idx = (idx + 1) & mask;
                if (++dist >= slots.size()) return std::nullopt;
            }
        }

        bool erase(std::uint64_t fp, const std::string& key) {
            std::unique_lock lk(mu);
            const std::size_t mask = slots.size() - 1;
            std::size_t idx = start(fp, mask);
            std::uint32_t dist = 0;
            for (;;) {
                Slot& slot = slots[idx];
                if (!slot.used || slot.dist < dist) return false;
                if (slot.fingerprint == fp && key_view(slot) == key) break;
                idx = (idx + 1) & mask;
                if (++dist >= slots.size()) return false;
            }

            garbage_bytes += slots[idx].key_len + slots[idx].value_len;
            std::size_t hole = idx;
            std::size_t next = (hole + 1) & mask;
            while (slots[next].used && slots[next].dist > 0) {
                slots[hole] = slots[next];
                --slots[hole].dist;
                hole = next;
                next = (next + 1) & mask;
            }
            slots[hole] = Slot{};
            --records;
            maybe_compact();
            return true;
        }
    };

    struct Address { std::uint32_t rho; std::uint64_t fingerprint; };
    std::size_t partition_count_;
    double max_load_;
    std::vector<std::unique_ptr<Partition>> parts_;
    std::atomic<std::size_t> size_{0};

    Address encode(const std::string& key) const noexcept {
        const std::uint64_t h1 = mix64(fnv1a64(key));
        const std::uint64_t h2 = mix64(fnv1a64(key, 1099511628211ULL) ^ (h1 << 1));
        return {std::uint32_t(h1 % partition_count_), h2};
    }

public:
    CompactIndex(std::size_t partitions, double max_load)
        : partition_count_(partitions), max_load_(max_load) {
        if (!partitions) throw std::invalid_argument("partition_count must be > 0");
        if (!(max_load > 0.40 && max_load < 0.95)) throw std::invalid_argument("invalid max_load");
        parts_.reserve(partitions);
        for (std::size_t i = 0; i < partitions; ++i) parts_.push_back(std::make_unique<Partition>());
    }

    void put(const std::string& key, const std::string& value) {
        const auto a = encode(key);
        if (parts_[a.rho]->put(a.fingerprint, key, value, max_load_)) size_.fetch_add(1);
    }
    bool erase(const std::string& key) {
        const auto a = encode(key);
        if (!parts_[a.rho]->erase(a.fingerprint, key)) return false;
        size_.fetch_sub(1);
        return true;
    }
    std::optional<std::string> get(const std::string& key) const {
        const auto a = encode(key);
        return parts_[a.rho]->get(a.fingerprint, key);
    }
    bool contains(const std::string& key) const { return get(key).has_value(); }
    std::size_t size() const noexcept { return size_.load(); }

    std::vector<std::pair<std::string, std::string>> snapshot_items() const {
        std::vector<std::pair<std::string, std::string>> out;
        out.reserve(size());
        for (const auto& p : parts_) {
            std::shared_lock lk(p->mu);
            for (const auto& s : p->slots) if (s.used) out.emplace_back(p->key_copy(s), p->value_copy(s));
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        return out;
    }

    bdr::IndexStats stats() const {
        bdr::IndexStats st;
        st.partitions = partition_count_;
        st.records = size();
        double sum_load = 0.0;
        for (const auto& p : parts_) {
            std::shared_lock lk(p->mu);
            st.slots += p->slots.size();
            st.max_partition_records = std::max(st.max_partition_records, p->records);
            sum_load += p->slots.empty() ? 0.0 : double(p->records) / double(p->slots.size());
        }
        st.mean_load = partition_count_ ? sum_load / double(partition_count_) : 0.0;
        return st;
    }
};

static std::string key_for(std::size_t i) {
    if (i % 997 == 0) {
        std::string k = "bin";
        k.push_back('\0');
        k.push_back(char(i & 0xff));
        k += std::to_string(i);
        return k;
    }
    return "k" + std::to_string(i);
}

static std::string value_for(std::size_t i, std::size_t round) {
    std::string v;
    v.push_back(char((i + round) & 0xff));
    v.push_back('\0');
    v += "v:" + std::to_string(i) + ":" + std::to_string(round);
    v.push_back(char(0xff - ((i + round) & 0xff)));
    return v;
}

template <typename A, typename B>
static void require_equal(const A& a, const B& b, const char* what) {
    if (!(a == b)) throw std::runtime_error(std::string("parity failure: ") + what);
}

} // namespace

int main() {
    const std::size_t n = std::strtoull(std::getenv("BDR_PARITY_RECORDS") ? std::getenv("BDR_PARITY_RECORDS") : "200000", nullptr, 10);
    const std::size_t updates = std::strtoull(std::getenv("BDR_PARITY_UPDATES") ? std::getenv("BDR_PARITY_UPDATES") : "1000000", nullptr, 10);
    constexpr std::size_t partitions = 4096;
    constexpr double max_load = 0.78;

    bdr::ResolutiveIndex baseline(partitions, max_load);
    CompactIndex compact(partitions, max_load);
    std::unordered_map<std::string, std::string> oracle;
    oracle.reserve(n * 2);

    for (std::size_t i = 0; i < n; ++i) {
        auto k = key_for(i);
        auto v = value_for(i, 0);
        baseline.put(k, v);
        compact.put(k, v);
        oracle[k] = v;
    }

    for (std::size_t i = 0; i < updates; ++i) {
        const std::size_t id = (i * 11400714819323198485ULL) % n;
        auto k = key_for(id);
        if (i % 11 == 0) {
            const bool a = baseline.erase(k);
            const bool b = compact.erase(k);
            require_equal(a, b, "erase return");
            oracle.erase(k);
        } else {
            auto v = value_for(id, i + 1);
            baseline.put(k, v);
            compact.put(k, v);
            oracle[k] = v;
        }
        if (i % 37 == 0) {
            const std::size_t rid = (id + 17) % n;
            auto rk = key_for(rid);
            auto rv = value_for(rid, i + 1000000);
            baseline.put(rk, rv);
            compact.put(rk, rv);
            oracle[rk] = rv;
        }
    }

    require_equal(baseline.size(), compact.size(), "size baseline/compact");
    require_equal(baseline.size(), oracle.size(), "size oracle");

    for (std::size_t i = 0; i < n; ++i) {
        auto k = key_for(i);
        const auto oa = baseline.get(k);
        const auto ob = compact.get(k);
        const auto it = oracle.find(k);
        const std::optional<std::string> oo = it == oracle.end() ? std::nullopt : std::optional<std::string>(it->second);
        require_equal(oa, ob, "get baseline/compact");
        require_equal(oa, oo, "get oracle");
        require_equal(baseline.contains(k), compact.contains(k), "contains");
    }

    const auto sb = baseline.snapshot_items();
    const auto sc = compact.snapshot_items();
    require_equal(sb, sc, "snapshot_items");

    std::vector<std::pair<std::string, std::string>> so(oracle.begin(), oracle.end());
    std::sort(so.begin(), so.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    require_equal(sb, so, "snapshot oracle");

    const auto stb = baseline.stats();
    const auto stc = compact.stats();
    require_equal(stb.partitions, stc.partitions, "stats partitions");
    require_equal(stb.records, stc.records, "stats records");
    require_equal(stb.slots, stc.slots, "stats slots");
    require_equal(stb.max_partition_records, stc.max_partition_records, "stats max_partition_records");
    if (stb.mean_load != stc.mean_load) throw std::runtime_error("parity failure: stats mean_load");

    std::cout << "COMPACT_CONTRACT_PARITY PASS records=" << n
              << " updates=" << updates
              << " final_records=" << baseline.size()
              << " slots=" << stb.slots
              << " mean_load=" << stb.mean_load << "\n";
}
