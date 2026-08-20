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
};

struct Ticket {
    std::uint64_t sequence = 0;
    explicit operator bool() const noexcept { return sequence != 0; }
};

enum class OperationType : std::uint8_t {
    Put = 1,
    Delete = 2,
};

struct Operation {
    OperationType type = OperationType::Put;
    std::string key;
    std::string value;
};

class Database {
public:
    static std::unique_ptr<Database> open(const std::filesystem::path& directory,
                                          Options options = {});

    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Asynchronous durability contract: the mutation becomes visible to get()
    // before this call returns, while durability is represented by the Ticket.
    Ticket submit(Operation operation);
    Ticket put(std::string key, std::string value);
    Ticket erase(std::string key);

    // Explicit durable convenience operations.
    void put_sync(std::string key, std::string value);
    void erase_sync(std::string key);

    std::optional<std::string> get(const std::string& key) const;

    // wait(t) returns only after every sequence <= t.sequence is durable.
    void wait(Ticket ticket);
    void sync();

    // Produces an atomic BDR3 snapshot, rotates to a fresh BDW3 WAL and
    // retires WAL segments covered by the snapshot.
    void checkpoint();

    std::uint64_t last_sequence() const noexcept;
    std::uint64_t durable_sequence() const noexcept;
    std::size_t size() const;

    void close();

private:
    class Impl;
    explicit Database(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace bdr
