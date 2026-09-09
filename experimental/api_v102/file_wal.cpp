#include "file_wal.hpp"

#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace bdr::v102 {
namespace fs = std::filesystem;

namespace {

std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}

std::size_t count_replayed_operations(const std::vector<std::uint8_t>& bytes,
                                      std::size_t last_good) {
    std::size_t operations = 0;
    std::size_t pos = 0;
    while (pos < last_good) {
        if (last_good - pos < 24) throw std::runtime_error("V102 telemetry frame truncated");
        const auto total = static_cast<std::size_t>(be32(bytes.data() + pos));
        if (total == 0 || pos + total > last_good)
            throw std::runtime_error("V102 telemetry frame bounds invalid");
        operations += static_cast<std::size_t>(be32(bytes.data() + pos + 20));
        pos += total;
    }
    if (pos != last_good) throw std::runtime_error("V102 telemetry replay boundary mismatch");
    return operations;
}

static void write_all(int fd, const std::uint8_t* data, std::size_t size) {
    while (size) {
        const auto written = ::write(fd, data, size);
        if (written <= 0) throw std::runtime_error("V102 WAL write failed");
        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

} // namespace

void append_batch(const fs::path& path,
                  std::uint64_t sequence,
                  const std::vector<v101::Operation>& operations,
                  bool durable) {
    const auto bytes = v101::encode_batch(sequence, operations);
    const int fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) throw std::runtime_error("V102 WAL open failed");
    try {
        write_all(fd, bytes.data(), bytes.size());
        if (durable && ::fdatasync(fd) != 0) throw std::runtime_error("V102 WAL fdatasync failed");
        ::close(fd);
    } catch (...) {
        ::close(fd);
        throw;
    }
}

FileReplayResult recover_file(const fs::path& path,
                              std::map<std::string, std::string>& state,
                              std::uint64_t initial_sequence,
                              bool repair_torn_tail) {
    if (!fs::exists(path)) return {initial_sequence, 0, 0, false, 0, 0};

    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("V102 WAL read open failed");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());

    auto replay = v101::replay(bytes, state, initial_sequence);
    const auto replayed_operations = count_replayed_operations(bytes, replay.last_good);
    bool repaired = false;
    if (replay.torn_tail && repair_torn_tail) {
        std::error_code ec;
        fs::resize_file(path, replay.last_good, ec);
        if (ec) throw std::system_error(ec, "V102 WAL tail repair failed");
        repaired = true;
    }

    return {replay.last_sequence,
            replay.committed_batches,
            static_cast<std::uintmax_t>(replay.last_good),
            repaired,
            static_cast<std::uintmax_t>(bytes.size()),
            replayed_operations};
}

} // namespace bdr::v102
