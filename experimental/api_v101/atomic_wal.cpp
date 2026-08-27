#include "atomic_wal.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <zlib.h>

namespace bdr::v101 {
namespace {

constexpr std::size_t kFixedPrefix = 4 + 4 + 4 + 8 + 4;
constexpr std::size_t kTrailer = 4;
constexpr std::uint32_t kVersion = 4;
constexpr std::size_t kMaxKey = 1u << 20;
constexpr std::size_t kMaxValue = 1u << 24;
constexpr std::size_t kMaxFrame = 1u << 27;

std::uint32_t crc32b(const void* data, std::size_t size, std::uint32_t seed = 0) {
    return static_cast<std::uint32_t>(::crc32(seed,
        static_cast<const Bytef*>(data), static_cast<uInt>(size)));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

void put64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}

std::uint64_t be64(const std::uint8_t* p) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) value = (value << 8) | p[i];
    return value;
}

void validate_operation(const Operation& op) {
    if (op.key.empty() || op.key.size() > kMaxKey || op.value.size() > kMaxValue) {
        throw std::invalid_argument("invalid V101 batch key/value size");
    }
    if (op.type == OpType::Delete && !op.value.empty()) {
        throw std::invalid_argument("delete operation must have empty value");
    }
}

} // namespace

std::vector<std::uint8_t> encode_batch(std::uint64_t sequence,
                                       const std::vector<Operation>& operations) {
    if (sequence == 0) throw std::invalid_argument("batch sequence must be non-zero");
    if (operations.empty()) throw std::invalid_argument("empty atomic batch is not allowed");
    if (operations.size() > 1'000'000) throw std::invalid_argument("atomic batch too large");

    std::vector<std::uint8_t> body;
    body.reserve(64);
    body.insert(body.end(), {'B', 'D', 'W', '4'});
    put32(body, kVersion);
    put64(body, sequence);
    put32(body, static_cast<std::uint32_t>(operations.size()));

    for (const auto& op : operations) {
        validate_operation(op);
        body.push_back(static_cast<std::uint8_t>(op.type));
        put32(body, static_cast<std::uint32_t>(op.key.size()));
        put32(body, static_cast<std::uint32_t>(op.value.size()));
        body.insert(body.end(), op.key.begin(), op.key.end());
        body.insert(body.end(), op.value.begin(), op.value.end());
    }

    const std::size_t total_size = 4 + body.size() + kTrailer;
    if (total_size > kMaxFrame) throw std::invalid_argument("atomic batch frame too large");

    std::vector<std::uint8_t> frame;
    frame.reserve(total_size);
    put32(frame, static_cast<std::uint32_t>(total_size));
    frame.insert(frame.end(), body.begin(), body.end());
    put32(frame, crc32b(frame.data(), frame.size()));
    return frame;
}

ReplayResult replay(const std::vector<std::uint8_t>& bytes,
                    std::map<std::string, std::string>& state,
                    std::uint64_t initial_sequence) {
    ReplayResult result{};
    result.last_sequence = initial_sequence;

    std::size_t pos = 0;
    while (pos < bytes.size()) {
        if (bytes.size() - pos < 4) {
            result.torn_tail = true;
            break;
        }

        const std::uint32_t total = be32(bytes.data() + pos);
        if (total < kFixedPrefix + kTrailer || total > kMaxFrame) {
            throw std::runtime_error("BDW4 total length invalid");
        }
        if (bytes.size() - pos < total) {
            result.torn_tail = true;
            break;
        }

        const auto* frame = bytes.data() + pos;
        if (std::memcmp(frame + 4, "BDW4", 4) != 0) {
            throw std::runtime_error("BDW4 magic mismatch");
        }
        if (be32(frame + 8) != kVersion) {
            throw std::runtime_error("BDW4 version unsupported");
        }

        const std::uint32_t expected_crc = be32(frame + total - 4);
        const std::uint32_t actual_crc = crc32b(frame, total - 4);
        if (expected_crc != actual_crc) {
            throw std::runtime_error("BDW4 frame CRC mismatch");
        }

        const std::uint64_t sequence = be64(frame + 12);
        const std::uint32_t count = be32(frame + 20);
        if (sequence != result.last_sequence + 1) {
            throw std::runtime_error("BDW4 sequence gap");
        }
        if (count == 0 || count > 1'000'000) {
            throw std::runtime_error("BDW4 operation count invalid");
        }

        std::vector<Operation> decoded;
        decoded.reserve(count);
        std::size_t cursor = 24;
        const std::size_t payload_end = total - 4;

        for (std::uint32_t i = 0; i < count; ++i) {
            if (cursor + 9 > payload_end) throw std::runtime_error("BDW4 operation header truncated");
            const auto raw_type = frame[cursor++];
            const auto key_len = be32(frame + cursor); cursor += 4;
            const auto value_len = be32(frame + cursor); cursor += 4;
            if (key_len == 0 || key_len > kMaxKey || value_len > kMaxValue) {
                throw std::runtime_error("BDW4 operation bounds invalid");
            }
            if (cursor + std::size_t(key_len) + std::size_t(value_len) > payload_end) {
                throw std::runtime_error("BDW4 operation payload truncated");
            }

            OpType type;
            if (raw_type == static_cast<std::uint8_t>(OpType::Put)) type = OpType::Put;
            else if (raw_type == static_cast<std::uint8_t>(OpType::Delete)) type = OpType::Delete;
            else throw std::runtime_error("BDW4 operation type invalid");

            Operation op;
            op.type = type;
            op.key.assign(reinterpret_cast<const char*>(frame + cursor), key_len);
            cursor += key_len;
            op.value.assign(reinterpret_cast<const char*>(frame + cursor), value_len);
            cursor += value_len;
            if (type == OpType::Delete && value_len != 0) {
                throw std::runtime_error("BDW4 delete contains value");
            }
            decoded.push_back(std::move(op));
        }

        if (cursor != payload_end) throw std::runtime_error("BDW4 trailing payload bytes");

        // Atomic visibility boundary: no mutation occurs until the entire frame,
        // CRC, sequence and every contained operation have been validated.
        auto next_state = state;
        for (const auto& op : decoded) {
            if (op.type == OpType::Put) next_state[op.key] = op.value;
            else next_state.erase(op.key);
        }
        state.swap(next_state);

        result.last_sequence = sequence;
        ++result.committed_batches;
        pos += total;
        result.last_good = pos;
    }

    return result;
}

} // namespace bdr::v101
