#include "commit_boundary.hpp"

#include <algorithm>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace bdr::v103 {
namespace fs = std::filesystem;

static void write_all(int fd, const std::uint8_t* data, std::size_t size) {
    while (size) {
        const auto written = ::write(fd, data, size);
        if (written <= 0) throw std::runtime_error("V103 WAL write failed");
        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

CommitOutcome commit_batch(const fs::path& path,
                           std::uint64_t sequence,
                           const std::vector<v101::Operation>& operations,
                           FailPoint failpoint,
                           std::size_t mid_append_bytes) {
    if (failpoint == FailPoint::BeforeAppend) throw SimulatedCrash{};

    const auto bytes = v101::encode_batch(sequence, operations);
    const int fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) throw std::runtime_error("V103 WAL open failed");

    try {
        if (failpoint == FailPoint::MidAppend) {
            const std::size_t n = std::min(mid_append_bytes ? mid_append_bytes : bytes.size() / 2,
                                           bytes.size() - 1);
            write_all(fd, bytes.data(), n);
            ::close(fd);
            throw SimulatedCrash{};
        }

        write_all(fd, bytes.data(), bytes.size());
        if (failpoint == FailPoint::AfterAppendBeforeSync) {
            ::close(fd);
            throw SimulatedCrash{};
        }

        if (::fdatasync(fd) != 0) throw std::runtime_error("V103 WAL fdatasync failed");
        ::close(fd);

        if (failpoint == FailPoint::AfterSyncBeforeAck) throw SimulatedCrash{};

        CommitOutcome outcome{true, true, sequence};
        if (failpoint == FailPoint::AfterAck) throw SimulatedCrash{};
        return outcome;
    } catch (...) {
        // close() after the explicit branches is harmless only when fd is still open;
        // those branches close before throwing. Keep this catch intentionally empty.
        throw;
    }
}

} // namespace bdr::v103
