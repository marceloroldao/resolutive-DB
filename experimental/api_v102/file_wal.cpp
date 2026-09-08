#include "file_wal.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace bdr::v102 {
namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
int open_append(const fs::path& path) {
    return ::_open(path.string().c_str(), _O_CREAT | _O_WRONLY | _O_APPEND | _O_BINARY, _S_IREAD | _S_IWRITE);
}
int write_fd(int fd, const void* data, unsigned int size) { return ::_write(fd, data, size); }
int sync_fd(int fd) { return ::_commit(fd); }
int close_fd(int fd) { return ::_close(fd); }
#else
int open_append(const fs::path& path) { return ::open(path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644); }
ssize_t write_fd(int fd, const void* data, std::size_t size) { return ::write(fd, data, size); }
int sync_fd(int fd) { return ::fdatasync(fd); }
int close_fd(int fd) { return ::close(fd); }
#endif

} // namespace

static void write_all(int fd, const std::uint8_t* data, std::size_t size) {
    while (size) {
#ifdef _WIN32
        const auto chunk = static_cast<unsigned int>(std::min<std::size_t>(size, 0x7fffffffU));
        const auto written = write_fd(fd, data, chunk);
#else
        const auto written = write_fd(fd, data, size);
#endif
        if (written <= 0) throw std::runtime_error("V102 WAL write failed");
        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

void append_batch(const fs::path& path,
                  std::uint64_t sequence,
                  const std::vector<v101::Operation>& operations,
                  bool durable) {
    const auto bytes = v101::encode_batch(sequence, operations);
    const int fd = open_append(path);
    if (fd < 0) throw std::runtime_error("V102 WAL open failed");
    try {
        write_all(fd, bytes.data(), bytes.size());
        if (durable && sync_fd(fd) != 0) throw std::runtime_error("V102 WAL sync failed");
        close_fd(fd);
    } catch (...) {
        close_fd(fd);
        throw;
    }
}

FileReplayResult recover_file(const fs::path& path,
                              std::map<std::string, std::string>& state,
                              std::uint64_t initial_sequence,
                              bool repair_torn_tail) {
    if (!fs::exists(path)) return {initial_sequence, 0, 0, false};

    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("V102 WAL read open failed");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());

    auto replay = v101::replay(bytes, state, initial_sequence);
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
            repaired};
}

} // namespace bdr::v102
