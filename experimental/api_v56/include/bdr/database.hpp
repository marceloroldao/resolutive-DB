#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace bdr {

struct Options {
    std::size_t reserve_bytes = 64ull * 1024ull * 1024ull;
    std::size_t wal_batch = 512;
    bool keep_size_preallocation = true;
    std::size_t partition_count = 4096;
    double partition_max_load = 0.78;
};

struct Ticket {
    std::uint64_t sequence = 0;
    explicit operator bool() const noexcept { return sequence != 0; }
};

enum class OperationType : std::uint8_t { Put = 1, Delete = 2 };

struct Operation {
    OperationType type = OperationType::Put;
    std::string key;
    std::string value;
};

struct IndexStats {
    std::size_t partitions = 0;
    std::size_t records = 0;
    std::size_t slots = 0;
    std::size_t max_partition_records = 0;
    double mean_load = 0.0;
};

class Database {
public:
    static std::unique_ptr<Database> open(const std::filesystem::path& directory,
                                          Options options = {});
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Visibility and durability are intentionally separate.
    Ticket submit(Operation operation);
    Ticket put(std::string key, std::string value);
    Ticket erase(std::string key);
    void put_sync(std::string key, std::string value);
    void erase_sync(std::string key);

    std::optional<std::string> get(const std::string& key) const;
    bool contains(const std::string& key) const;

    void wait(Ticket ticket);
    void sync();
    void checkpoint();
    void close();

    std::uint64_t last_sequence() const noexcept;
    std::uint64_t durable_sequence() const noexcept;
    std::size_t size() const;
    IndexStats index_stats() const;

private:
    class Impl;
    explicit Database(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace bdr
