#pragma once

#include "bdr/database.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace bdr {

class ResolutiveIndex {
public:
    ResolutiveIndex(std::size_t partitions, double max_load);
    ~ResolutiveIndex();
    ResolutiveIndex(const ResolutiveIndex&) = delete;
    ResolutiveIndex& operator=(const ResolutiveIndex&) = delete;

    void put(const std::string& key, const std::string& value);
    bool erase(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;
    bool contains(const std::string& key) const;
    std::size_t size() const noexcept;
    IndexStats stats() const;
    std::vector<std::pair<std::string, std::string>> snapshot_items() const;

private:
    struct Partition;
    struct Address { std::uint32_t rho; std::uint64_t fingerprint; };
    Address encode(const std::string& key) const noexcept;

    std::size_t partition_count_;
    double max_load_;
    std::vector<std::unique_ptr<Partition>> parts_;
    std::atomic<std::size_t> size_{0};
};

} // namespace bdr
