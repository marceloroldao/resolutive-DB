#include "concurrent_database.hpp"

namespace bdr::v105 {

ConcurrentDatabase::ConcurrentDatabase(std::filesystem::path wal_path)
    : database_(std::move(wal_path)) {}

v104::BatchResult ConcurrentDatabase::write_batch(std::vector<v101::Operation> operations,
                                                   v104::DurabilityMode durability) {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.write_batch(std::move(operations), durability);
}

v104::BatchResult ConcurrentDatabase::put(std::string key,
                                          std::string value,
                                          v104::DurabilityMode durability) {
    std::vector<v101::Operation> operations;
    operations.push_back({v101::OpType::Put, std::move(key), std::move(value)});
    return write_batch(std::move(operations), durability);
}

v104::BatchResult ConcurrentDatabase::erase(std::string key,
                                            v104::DurabilityMode durability) {
    std::vector<v101::Operation> operations;
    operations.push_back({v101::OpType::Delete, std::move(key), {}});
    return write_batch(std::move(operations), durability);
}

v104::BatchResult ConcurrentDatabase::put_many(std::vector<std::pair<std::string, std::string>> entries,
                                               v104::DurabilityMode durability) {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.put_many(std::move(entries), durability);
}

v104::BatchResult ConcurrentDatabase::erase_many(std::vector<std::string> keys,
                                                 v104::DurabilityMode durability) {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.erase_many(std::move(keys), durability);
}

std::optional<std::string> ConcurrentDatabase::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.get(key);
}

std::uint64_t ConcurrentDatabase::last_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.last_sequence();
}

std::uint64_t ConcurrentDatabase::durable_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.durable_sequence();
}

std::size_t ConcurrentDatabase::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return database_.size();
}

} // namespace bdr::v105
