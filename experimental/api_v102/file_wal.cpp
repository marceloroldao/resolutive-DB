#include "file_wal.hpp"

#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace bdr::v102 {
namespace fs = std::filesystem;

namespace {
using Clock = std::chrono::steady_clock;

std::uint64_t elapsed_us(Clock::time_point start, Clock::time_point end) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
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
    if (!fs::exists(path)) return {initial_sequence, 0, 0, false, 0, 0, 0, 0};

    const auto read_start = Clock::now();
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("V102 WAL read open failed");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    const auto read_end = Clock::now();

    const auto replay_start = Clock::now();
    auto replay = v101::replay(bytes, state, initial_sequence);
    const auto replay_end = Clock::now();

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
            replay.replayed_operations,
            elapsed_us(read_start, read_end),
            elapsed_us(replay_start, replay_end)};
}

} // namespace bdr::v102
