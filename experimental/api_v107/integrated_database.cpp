#include "integrated_database.hpp"

#include <stdexcept>

namespace bdr::v107 {

IntegratedDatabase::IntegratedDatabase(std::filesystem::path legacy_directory,
                                       std::filesystem::path bdw4_path)
    : database_(std::move(legacy_directory), std::move(bdw4_path)) {}

BatchResult IntegratedDatabase::write_batch(std::vector<v101::Operation> operations) {
    if (operations.empty()) throw std::invalid_argument("V107 empty batch");
    std::lock_guard<std::mutex> lock(mutex_);
    const auto seq = database_.write_batch(operations);
    return BatchResult{seq, operations.size(), true};
}

BatchResult IntegratedDatabase::put(std::string key, std::string value) {
    return write_batch({v101::Operation{v101::OpType::Put, std::move(key), std::move(value)}});
}

BatchResult IntegratedDatabase::erase(std::string key) {
    return write_batch({v101::Operation{v101::OpType::Delete, std::move(key), {}}});
}

BatchResult IntegratedDatabase::put_many(std::vector<std::pair<std::string, std::string>> entries) {
    std::vector<v101::Operation> ops;
    ops.reserve(entries.size());
    for (auto& [key, value] : entries)
        ops.push_back(v101::Operation{v101::OpType::Put, std::move(key), std::move(value)});
    return write_batch(std::move(ops));
}

BatchResult IntegratedDatabase::erase_many(std::vector<std::string> keys) {
    std::vector<v101::Operation> ops;
    ops.reserve(keys.size());
    for (auto& key : keys)
        ops.push_back(v101::Operation{v101::OpType::Delete, std::move(key), {}});
    return write_batch(std::move(ops));
}

std::optional<std::string> IntegratedDatabase::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.get(key);
}

std::uint64_t IntegratedDatabase::last_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.last_sequence();
}

std::uint64_t IntegratedDatabase::durable_sequence() const {
    return last_sequence();
}

std::size_t IntegratedDatabase::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.size();
}

} // namespace bdr::v107
