#include "bdr/resolutive_index.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace bdr {
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

} // namespace

struct ResolutiveIndex::Partition {
    struct Slot {
        std::uint64_t fingerprint = 0;
        std::uint32_t dist = 0;
        bool used = false;
        std::string key;
        std::string value;
    };

    mutable std::shared_mutex mu;
    std::vector<Slot> slots;
    std::size_t records = 0;

    explicit Partition(std::size_t cap = 8) : slots(next_pow2(cap)) {}

    static std::size_t start(std::uint64_t fp, std::size_t mask) noexcept {
        return std::size_t(mix64(fp)) & mask;
    }

    void rehash(std::size_t new_cap) {
        std::vector<Slot> old;
        old.swap(slots);
        slots.assign(next_pow2(new_cap), Slot{});
        records = 0;
        for (auto& s : old) {
            if (!s.used) continue;
            insert_nolock(s.fingerprint, std::move(s.key), std::move(s.value));
        }
    }

    bool insert_nolock(std::uint64_t fp, std::string key, std::string value) {
        const std::size_t mask = slots.size() - 1;
        std::size_t idx = start(fp, mask);
        Slot cur;
        cur.fingerprint = fp;
        cur.dist = 0;
        cur.used = true;
        cur.key = std::move(key);
        cur.value = std::move(value);

        for (;;) {
            Slot& slot = slots[idx];
            if (!slot.used) {
                slot = std::move(cur);
                ++records;
                return true;
            }
            if (slot.fingerprint == cur.fingerprint && slot.key == cur.key) {
                slot.value = std::move(cur.value);
                return false;
            }
            if (slot.dist < cur.dist) std::swap(slot, cur);
            idx = (idx + 1) & mask;
            ++cur.dist;
            if (cur.dist >= slots.size()) throw std::runtime_error("Robin Hood partition overflow");
        }
    }

    bool put(std::uint64_t fp, const std::string& key, const std::string& value, double max_load) {
        std::unique_lock<std::shared_mutex> lk(mu);
        if (double(records + 1) / double(slots.size()) > max_load) rehash(slots.size() * 2);
        return insert_nolock(fp, key, value);
    }

    std::optional<std::string> get(std::uint64_t fp, const std::string& key) const {
        std::shared_lock<std::shared_mutex> lk(mu);
        const std::size_t mask = slots.size() - 1;
        std::size_t idx = start(fp, mask);
        std::uint32_t dist = 0;
        for (;;) {
            const Slot& slot = slots[idx];
            if (!slot.used || slot.dist < dist) return std::nullopt;
            if (slot.fingerprint == fp && slot.key == key) return slot.value;
            idx = (idx + 1) & mask;
            ++dist;
            if (dist >= slots.size()) return std::nullopt;
        }
    }

    bool erase(std::uint64_t fp, const std::string& key) {
        std::unique_lock<std::shared_mutex> lk(mu);
        const std::size_t mask = slots.size() - 1;
        std::size_t idx = start(fp, mask);
        std::uint32_t dist = 0;
        for (;;) {
            Slot& slot = slots[idx];
            if (!slot.used || slot.dist < dist) return false;
            if (slot.fingerprint == fp && slot.key == key) break;
            idx = (idx + 1) & mask;
            ++dist;
            if (dist >= slots.size()) return false;
        }

        std::size_t hole = idx;
        std::size_t next = (hole + 1) & mask;
        while (slots[next].used && slots[next].dist > 0) {
            slots[hole] = std::move(slots[next]);
            --slots[hole].dist;
            hole = next;
            next = (next + 1) & mask;
        }
        slots[hole] = Slot{};
        --records;
        return true;
    }
};

ResolutiveIndex::ResolutiveIndex(std::size_t partitions, double max_load)
    : partition_count_(partitions), max_load_(max_load) {
    if (partition_count_ == 0) throw std::invalid_argument("partition_count must be > 0");
    if (!(max_load_ > 0.40 && max_load_ < 0.95))
        throw std::invalid_argument("partition_max_load must be between 0.40 and 0.95");
    parts_.reserve(partition_count_);
    for (std::size_t i = 0; i < partition_count_; ++i)
        parts_.push_back(std::make_unique<Partition>());
}

ResolutiveIndex::~ResolutiveIndex() = default;

ResolutiveIndex::Address ResolutiveIndex::encode(const std::string& key) const noexcept {
    const std::uint64_t h1 = mix64(fnv1a64(key));
    const std::uint64_t h2 = mix64(fnv1a64(key, 1099511628211ULL) ^ (h1 << 1));
    return {std::uint32_t(h1 % partition_count_), h2};
}

void ResolutiveIndex::put(const std::string& key, const std::string& value) {
    const auto a = encode(key);
    if (parts_[a.rho]->put(a.fingerprint, key, value, max_load_))
        size_.fetch_add(1, std::memory_order_acq_rel);
}

bool ResolutiveIndex::erase(const std::string& key) {
    const auto a = encode(key);
    if (!parts_[a.rho]->erase(a.fingerprint, key)) return false;
    size_.fetch_sub(1, std::memory_order_acq_rel);
    return true;
}

std::optional<std::string> ResolutiveIndex::get(const std::string& key) const {
    const auto a = encode(key);
    return parts_[a.rho]->get(a.fingerprint, key);
}

bool ResolutiveIndex::contains(const std::string& key) const { return get(key).has_value(); }

std::size_t ResolutiveIndex::size() const noexcept { return size_.load(std::memory_order_acquire); }

std::vector<std::pair<std::string, std::string>> ResolutiveIndex::snapshot_items() const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(size());
    for (const auto& p : parts_) {
        std::shared_lock<std::shared_mutex> lk(p->mu);
        for (const auto& s : p->slots) if (s.used) out.emplace_back(s.key, s.value);
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return out;
}

IndexStats ResolutiveIndex::stats() const {
    IndexStats st;
    st.partitions = partition_count_;
    st.records = size();
    std::size_t max_records = 0;
    double sum_load = 0.0;
    for (const auto& p : parts_) {
        std::shared_lock<std::shared_mutex> lk(p->mu);
        st.slots += p->slots.size();
        max_records = std::max(max_records, p->records);
        sum_load += p->slots.empty() ? 0.0 : double(p->records) / double(p->slots.size());
    }
    st.max_partition_records = max_records;
    st.mean_load = partition_count_ ? sum_load / double(partition_count_) : 0.0;
    return st;
}

} // namespace bdr
