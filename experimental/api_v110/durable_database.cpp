#include "durable_database.hpp"
#include "../api_v102/file_wal.hpp"
#include "../api_v106/migration.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <utility>

namespace bdr::v110 {

DurableDatabase::DurableDatabase(std::filesystem::path legacy_directory,
                                 std::filesystem::path bdw4_path)
    : bdw4_path_(std::move(bdw4_path)) {
    auto legacy = v106::read_v1_state(legacy_directory);
    state_ = std::move(legacy.values);
    last_sequence_ = legacy.sequence;
    durable_sequence_ = legacy.sequence;

    if (std::filesystem::exists(bdw4_path_)) {
        auto recovered = v102::recover_file(bdw4_path_, state_, last_sequence_, true);
        last_sequence_ = recovered.last_sequence;
        // Any complete frames recovered after reopening are the persisted prefix
        // available to this process. The API cannot infer an ACK that a previous
        // process did or did not receive, only the recoverable durable prefix.
        durable_sequence_ = recovered.last_sequence;
    }
}

void DurableDatabase::validate_operations(const std::vector<v101::Operation>& operations,
                                          DurabilityMode durability) const {
    if (operations.empty()) throw std::invalid_argument("write_batch requires at least one operation");
    if (durability == DurabilityMode::PerOperationSync && operations.size() != 1)
        throw std::invalid_argument("PerOperationSync requires exactly one operation");
    for (const auto& op : operations) {
        if (op.key.empty()) throw std::invalid_argument("operation key must not be empty");
        if (op.key.size() > (1u << 20)) throw std::invalid_argument("operation key too large");
        if (op.value.size() > (1u << 24)) throw std::invalid_argument("operation value too large");
    }
}

void DurableDatabase::apply(std::map<std::string, std::string>& state,
                            const std::vector<v101::Operation>& operations) {
    for (const auto& op : operations) {
        if (op.type == v101::OpType::Put) state[op.key] = op.value;
        else state.erase(op.key);
    }
}

BatchResult DurableDatabase::write_batch(std::vector<v101::Operation> operations,
                                         DurabilityMode durability) {
    std::lock_guard<std::mutex> lock(mutex_);
    validate_operations(operations, durability);

    const auto sequence = last_sequence_ + 1;
    const bool sync_now = durability != DurabilityMode::Async;
    v102::append_batch(bdw4_path_, sequence, operations, sync_now);

    // State becomes visible only after the complete frame append returned.
    apply(state_, operations);
    last_sequence_ = sequence;
    if (sync_now) durable_sequence_ = sequence;

    return {sequence, operations.size(), sync_now};
}

BatchResult DurableDatabase::put(std::string key,
                                 std::string value,
                                 DurabilityMode durability) {
    return write_batch({v101::Operation{v101::OpType::Put, std::move(key), std::move(value)}}, durability);
}

BatchResult DurableDatabase::erase(std::string key,
                                   DurabilityMode durability) {
    return write_batch({v101::Operation{v101::OpType::Delete, std::move(key), {}}}, durability);
}

BatchResult DurableDatabase::put_many(std::vector<std::pair<std::string, std::string>> entries,
                                      DurabilityMode durability) {
    std::vector<v101::Operation> operations;
    operations.reserve(entries.size());
    for (auto& [key, value] : entries)
        operations.push_back({v101::OpType::Put, std::move(key), std::move(value)});
    return write_batch(std::move(operations), durability);
}

BatchResult DurableDatabase::erase_many(std::vector<std::string> keys,
                                        DurabilityMode durability) {
    std::vector<v101::Operation> operations;
    operations.reserve(keys.size());
    for (auto& key : keys)
        operations.push_back({v101::OpType::Delete, std::move(key), {}});
    return write_batch(std::move(operations), durability);
}

void DurableDatabase::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (last_sequence_ == durable_sequence_) return;
    if (!std::filesystem::exists(bdw4_path_)) {
        durable_sequence_ = last_sequence_;
        return;
    }

    const int fd = ::open(bdw4_path_.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("V110 sync open failed");
    const int rc = ::fdatasync(fd);
    ::close(fd);
    if (rc != 0) throw std::runtime_error("V110 fdatasync failed");
    durable_sequence_ = last_sequence_;
}

std::optional<std::string> DurableDatabase::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = state_.find(key);
    if (it == state_.end()) return std::nullopt;
    return it->second;
}

std::uint64_t DurableDatabase::last_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_sequence_;
}

std::uint64_t DurableDatabase::durable_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return durable_sequence_;
}

std::size_t DurableDatabase::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.size();
}

} // namespace bdr::v110
