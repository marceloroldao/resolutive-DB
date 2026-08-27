#pragma once

#include "bdr/database.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bdr {

enum class DurabilityMode : std::uint8_t {
    Async = 0,
    BatchSync = 1,
    PerOperationSync = 2,
};

struct BatchResult {
    std::uint64_t sequence = 0;
    std::size_t operations = 0;
    bool durable = false;
};

class AtomicDatabase {
public:
    static std::unique_ptr<AtomicDatabase> open(
        const std::filesystem::path& legacy_directory,
        const std::filesystem::path& bdw4_path = {});

    ~AtomicDatabase();
    AtomicDatabase(const AtomicDatabase&) = delete;
    AtomicDatabase& operator=(const AtomicDatabase&) = delete;

    BatchResult write_batch(std::vector<Operation> operations,
                            DurabilityMode durability = DurabilityMode::BatchSync);

    BatchResult put(std::string key,
                    std::string value,
                    DurabilityMode durability = DurabilityMode::Async);
    BatchResult erase(std::string key,
                      DurabilityMode durability = DurabilityMode::Async);

    BatchResult put_many(std::vector<std::pair<std::string, std::string>> entries,
                         DurabilityMode durability = DurabilityMode::BatchSync);
    BatchResult erase_many(std::vector<std::string> keys,
                           DurabilityMode durability = DurabilityMode::BatchSync);

    std::optional<std::string> get(const std::string& key) const;
    bool contains(const std::string& key) const;

    void sync();

    std::uint64_t last_sequence() const;
    std::uint64_t durable_sequence() const;
    std::size_t size() const;

private:
    class Impl;
    explicit AtomicDatabase(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace bdr
