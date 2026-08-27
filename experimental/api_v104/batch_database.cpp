#include "batch_database.hpp"
#include "../api_v102/file_wal.hpp"
#include "../api_v103/commit_boundary.hpp"

#include <stdexcept>
#include <utility>

namespace bdr::v104 {

BatchDatabase::BatchDatabase(std::filesystem::path wal_path)
    : wal_path_(std::move(wal_path)) {
    auto recovered = v102::recover_file(wal_path_, state_);
    last_sequence_ = recovered.last_sequence;
    durable_sequence_ = recovered.last_sequence;
}

BatchResult BatchDatabase::write_batch(std::vector<v101::Operation> operations,
                                       DurabilityMode durability) {
    if (operations.empty()) throw std::invalid_argument("write_batch requires at least one operation");
    for (const auto& op : operations) {
        if (op.key.empty()) throw std::invalid_argument("batch operation key must not be empty");
    }

    const std::uint64_t sequence = last_sequence_ + 1;
    bool durable = false;

    if (durability == DurabilityMode::BatchSync) {
        const auto outcome = v103::commit_batch(wal_path_, sequence, operations);
        if (!outcome.acknowledged || !outcome.durable_boundary_crossed)
            throw std::runtime_error("V104 durable batch was not acknowledged");
        durable = true;
    } else {
        v102::append_batch(wal_path_, sequence, operations, false);
    }

    // Mutate the visible state only after the complete atomic frame has been appended.
    for (const auto& op : operations) {
        if (op.type == v101::OpType::Put) state_[op.key] = op.value;
        else state_.erase(op.key);
    }

    last_sequence_ = sequence;
    if (durable) durable_sequence_ = sequence;

    return {sequence, operations.size(), durable};
}

BatchResult BatchDatabase::put_many(std::vector<std::pair<std::string, std::string>> entries,
                                    DurabilityMode durability) {
    std::vector<v101::Operation> operations;
    operations.reserve(entries.size());
    for (auto& [key, value] : entries)
        operations.push_back({v101::OpType::Put, std::move(key), std::move(value)});
    return write_batch(std::move(operations), durability);
}

BatchResult BatchDatabase::erase_many(std::vector<std::string> keys,
                                      DurabilityMode durability) {
    std::vector<v101::Operation> operations;
    operations.reserve(keys.size());
    for (auto& key : keys)
        operations.push_back({v101::OpType::Delete, std::move(key), {}});
    return write_batch(std::move(operations), durability);
}

std::optional<std::string> BatchDatabase::get(const std::string& key) const {
    auto it = state_.find(key);
    if (it == state_.end()) return std::nullopt;
    return it->second;
}

std::uint64_t BatchDatabase::last_sequence() const noexcept { return last_sequence_; }
std::uint64_t BatchDatabase::durable_sequence() const noexcept { return durable_sequence_; }
std::size_t BatchDatabase::size() const noexcept { return state_.size(); }

} // namespace bdr::v104
