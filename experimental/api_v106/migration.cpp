#include "migration.hpp"
#include "../api_v102/file_wal.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace bdr::v106 {
namespace fs = std::filesystem;

namespace {

uint32_t crc32b(const void* p, std::size_t n, uint32_t seed = 0) {
    return uint32_t(::crc32(seed, static_cast<const Bytef*>(p), uInt(n)));
}
uint32_t be32(const std::uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
uint64_t be64(const std::uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}
void put32(std::vector<std::uint8_t>& b, uint32_t v) {
    for (int i = 3; i >= 0; --i) b.push_back(uint8_t(v >> (8 * i)));
}
void put64(std::vector<std::uint8_t>& b, uint64_t v) {
    for (int i = 7; i >= 0; --i) b.push_back(uint8_t(v >> (8 * i)));
}

std::vector<std::uint8_t> read_all(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("V106 file open failed: " + path.string());
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

#pragma pack(push, 1)
struct Wal3Header {
    char magic[4];
    std::uint16_t version;
    std::uint16_t header_size;
    std::uint32_t flags;
    std::uint64_t segment_id;
    std::uint64_t first_sequence;
    std::uint32_t crc;
};
#pragma pack(pop)

uint32_t wal_hcrc(const Wal3Header& h) { return crc32b(&h, sizeof(h) - 4); }

void replay_snapshot(const fs::path& path, LegacyState& out) {
    const auto b = read_all(path);
    if (b.size() < 28) throw std::runtime_error("V106 BDR3 too short");
    if (std::memcmp(b.data(), "BDR3", 4) != 0 || be32(b.data() + 4) != 3)
        throw std::runtime_error("V106 BDR3 header invalid");
    const uint32_t stored_crc = be32(b.data() + b.size() - 4);
    if (crc32b(b.data(), b.size() - 4) != stored_crc)
        throw std::runtime_error("V106 BDR3 CRC mismatch");

    const uint64_t seq = be64(b.data() + 8);
    const uint64_t count = be64(b.data() + 16);
    std::size_t pos = 24;
    std::map<std::string, std::string> candidate;
    for (uint64_t i = 0; i < count; ++i) {
        if (pos + 8 > b.size() - 4) throw std::runtime_error("V106 BDR3 record header truncated");
        const uint32_t kl = be32(b.data() + pos);
        const uint32_t vl = be32(b.data() + pos + 4);
        pos += 8;
        if (!kl || kl > (1u << 20) || vl > (1u << 24) || pos + uint64_t(kl) + vl > b.size() - 4)
            throw std::runtime_error("V106 BDR3 record bounds invalid");
        std::string key(reinterpret_cast<const char*>(b.data() + pos), kl);
        pos += kl;
        std::string value(reinterpret_cast<const char*>(b.data() + pos), vl);
        pos += vl;
        candidate[std::move(key)] = std::move(value);
    }
    if (pos != b.size() - 4) throw std::runtime_error("V106 BDR3 trailing bytes");
    out.values = std::move(candidate);
    out.sequence = seq;
}

void replay_wal(const fs::path& path, LegacyState& out) {
    const auto b = read_all(path);
    if (b.size() < sizeof(Wal3Header)) throw std::runtime_error("V106 BDW3 header truncated");
    Wal3Header h{};
    std::memcpy(&h, b.data(), sizeof(h));
    if (std::memcmp(h.magic, "BDW3", 4) || h.version != 3 || h.header_size != sizeof(h) || wal_hcrc(h) != h.crc)
        throw std::runtime_error("V106 BDW3 header invalid");

    std::size_t pos = sizeof(Wal3Header);
    uint64_t expected = std::max<uint64_t>(out.sequence + 1, h.first_sequence);
    while (pos < b.size()) {
        if (pos + 4 > b.size()) break; // legal torn final tail
        const uint32_t total = be32(b.data() + pos);
        constexpr uint32_t MIN = 4 + 8 + 1 + 4 + 4 + 4 + 4;
        if (total < MIN || total > (1u << 24)) throw std::runtime_error("V106 BDW3 total_len invalid");
        if (pos + total > b.size()) break; // legal torn final tail

        const std::uint8_t* q = b.data() + pos + 4;
        const uint64_t rseq = be64(q); q += 8;
        const uint8_t op = *q++;
        const uint32_t kl = be32(q); q += 4;
        const uint32_t vl = be32(q); q += 4;
        const uint32_t hc = be32(q); q += 4;

        std::vector<std::uint8_t> hh;
        put64(hh, rseq); hh.push_back(op); put32(hh, kl); put32(hh, vl);
        if (crc32b(hh.data(), hh.size()) != hc || !kl || kl > (1u << 20) || vl > (1u << 24))
            throw std::runtime_error("V106 BDW3 frame header invalid");
        if (op != 1 && op != 2) throw std::runtime_error("V106 BDW3 op invalid");

        const std::size_t payload_start = std::size_t(q - b.data());
        const std::size_t record_end = pos + total;
        if (payload_start + uint64_t(kl) + vl + 4 != record_end)
            throw std::runtime_error("V106 BDW3 payload bounds invalid");

        const uint32_t stored_crc = be32(b.data() + record_end - 4);
        if (crc32b(b.data() + pos, total - 4) != stored_crc)
            throw std::runtime_error("V106 BDW3 record CRC mismatch");

        const std::string key(reinterpret_cast<const char*>(q), kl); q += kl;
        const std::string value(reinterpret_cast<const char*>(q), vl);
        if (rseq > out.sequence) {
            if (rseq != expected) throw std::runtime_error("V106 BDW3 sequence gap");
            if (op == 1) out.values[key] = value;
            else out.values.erase(key);
            out.sequence = rseq;
            ++expected;
        }
        pos += total;
    }
}

void apply(std::map<std::string, std::string>& state, const std::vector<v101::Operation>& operations) {
    for (const auto& op : operations) {
        if (op.type == v101::OpType::Put) state[op.key] = op.value;
        else state.erase(op.key);
    }
}

} // namespace

LegacyState read_v1_state(const fs::path& directory) {
    LegacyState out;
    const fs::path snapshot = directory / "snapshot.bdr3";
    if (fs::exists(snapshot)) replay_snapshot(snapshot, out);

    std::vector<fs::path> wals;
    for (const auto& entry : fs::directory_iterator(directory))
        if (entry.is_regular_file() && entry.path().extension() == ".bdw3") wals.push_back(entry.path());
    std::sort(wals.begin(), wals.end());
    for (const auto& wal : wals) replay_wal(wal, out);
    return out;
}

MigratedDatabase::MigratedDatabase(fs::path legacy_directory, fs::path bdw4_path)
    : bdw4_path_(std::move(bdw4_path)) {
    auto legacy = read_v1_state(legacy_directory);
    state_ = std::move(legacy.values);
    sequence_ = legacy.sequence;
    if (fs::exists(bdw4_path_)) {
        auto rr = v102::recover_file(bdw4_path_, state_, sequence_, true);
        sequence_ = rr.last_sequence;
    }
}

std::uint64_t MigratedDatabase::write_batch(const std::vector<v101::Operation>& operations) {
    if (operations.empty()) throw std::invalid_argument("V106 empty batch");
    const uint64_t next = sequence_ + 1;
    v102::append_batch(bdw4_path_, next, operations, true);
    apply(state_, operations);
    sequence_ = next;
    return sequence_;
}

std::optional<std::string> MigratedDatabase::get(const std::string& key) const {
    const auto it = state_.find(key);
    if (it == state_.end()) return std::nullopt;
    return it->second;
}

} // namespace bdr::v106
