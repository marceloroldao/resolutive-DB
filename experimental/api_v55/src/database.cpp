#include "bdr/database.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <linux/falloc.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>
#include <zlib.h>

namespace bdr {
namespace fs = std::filesystem;

namespace {

static uint32_t crc32b(const void* p, std::size_t n, uint32_t seed = 0) {
    return uint32_t(::crc32(seed, static_cast<const Bytef*>(p), uInt(n)));
}

static void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 3; i >= 0; --i) b.push_back(uint8_t(v >> (8 * i)));
}
static void put64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 7; i >= 0; --i) b.push_back(uint8_t(v >> (8 * i)));
}
static uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
static uint64_t be64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

static void write_all(int fd, const void* p, std::size_t n) {
    const char* q = static_cast<const char*>(p);
    while (n) {
        ssize_t w = ::write(fd, q, n);
        if (w <= 0) throw std::runtime_error("BDR write failed");
        q += w;
        n -= std::size_t(w);
    }
}

static void pwrite_all(int fd, const uint8_t* p, std::size_t n, off_t off) {
    while (n) {
        ssize_t w = ::pwrite(fd, p, n, off);
        if (w <= 0) throw std::runtime_error("BDR pwrite failed");
        p += w;
        n -= std::size_t(w);
        off += w;
    }
}

static void fsync_dir(const fs::path& p) {
    int fd = ::open(p.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) throw std::runtime_error("BDR open directory failed");
    int rc = ::fsync(fd);
    ::close(fd);
    if (rc != 0) throw std::runtime_error("BDR directory fsync failed");
}

#pragma pack(push, 1)
struct Wal3Header {
    char magic[4];
    uint16_t version;
    uint16_t header_size;
    uint32_t flags;
    uint64_t segment_id;
    uint64_t first_sequence;
    uint32_t crc;
};
#pragma pack(pop)

static uint32_t wal_header_crc(const Wal3Header& h) {
    return crc32b(&h, sizeof(h) - sizeof(h.crc));
}

static std::vector<uint8_t> make_record(uint64_t seq, OperationType type,
                                        const std::string& key, const std::string& value) {
    std::vector<uint8_t> head;
    put64(head, seq);
    head.push_back(uint8_t(type));
    put32(head, uint32_t(key.size()));
    put32(head, uint32_t(value.size()));
    const uint32_t hc = crc32b(head.data(), head.size());

    const uint32_t total = 4 + uint32_t(head.size()) + 4 +
                           uint32_t(key.size() + value.size()) + 4;
    std::vector<uint8_t> out;
    out.reserve(total);
    put32(out, total);
    out.insert(out.end(), head.begin(), head.end());
    put32(out, hc);
    out.insert(out.end(), key.begin(), key.end());
    out.insert(out.end(), value.begin(), value.end());
    put32(out, crc32b(out.data(), out.size()));
    return out;
}

static std::vector<uint8_t> encode_snapshot(
    uint64_t seq, const std::unordered_map<std::string, std::string>& kv) {
    // Sort keys to make snapshots deterministic and reproducible.
    std::map<std::string, std::string> ordered(kv.begin(), kv.end());
    std::vector<uint8_t> b;
    b.insert(b.end(), {'B', 'D', 'R', '3'});
    put32(b, 3);
    put64(b, seq);
    put64(b, uint64_t(ordered.size()));
    for (const auto& [k, v] : ordered) {
        put32(b, uint32_t(k.size()));
        put32(b, uint32_t(v.size()));
        b.insert(b.end(), k.begin(), k.end());
        b.insert(b.end(), v.begin(), v.end());
    }
    put32(b, crc32b(b.data(), b.size()));
    return b;
}

struct RecoveredState {
    uint64_t seq = 0;
    std::unordered_map<std::string, std::string> kv;
};

static RecoveredState decode_snapshot(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 28) throw std::runtime_error("BDR3 snapshot too short");

    const uint32_t got = be32(b.data() + b.size() - 4);
    if (crc32b(b.data(), b.size() - 4) != got) throw std::runtime_error("BDR3 snapshot CRC mismatch");

    const uint8_t* q = b.data();
    const uint8_t* end = b.data() + b.size() - 4;
    if (std::memcmp(q, "BDR3", 4) != 0) throw std::runtime_error("BDR3 snapshot magic mismatch");
    q += 4;
    if (end - q < 20) throw std::runtime_error("BDR3 snapshot header truncated");
    if (be32(q) != 3) throw std::runtime_error("BDR3 unsupported snapshot version");
    q += 4;

    RecoveredState out;
    out.seq = be64(q); q += 8;
    const uint64_t count = be64(q); q += 8;
    out.kv.reserve(std::size_t(std::min<uint64_t>(count * 2 + 1, 8'000'001)));

    for (uint64_t i = 0; i < count; ++i) {
        if (end - q < 8) throw std::runtime_error("BDR3 snapshot record header truncated");
        const uint32_t kl = be32(q); q += 4;
        const uint32_t vl = be32(q); q += 4;
        if (kl == 0 || kl > (1u << 20) || vl > (1u << 24)) throw std::runtime_error("BDR3 snapshot record bounds invalid");
        if (uint64_t(end - q) < uint64_t(kl) + vl) throw std::runtime_error("BDR3 snapshot record truncated");
        std::string k(reinterpret_cast<const char*>(q), kl); q += kl;
        std::string v(reinterpret_cast<const char*>(q), vl); q += vl;
        out.kv[std::move(k)] = std::move(v);
    }
    if (q != end) throw std::runtime_error("BDR3 snapshot trailing bytes");
    return out;
}

struct ReplayResult {
    off_t last_good_offset = 0;
    uint64_t segment_id = 0;
    bool torn_tail = false;
};

static ReplayResult replay_wal(const fs::path& p, RecoveredState& state) {
    int fd = ::open(p.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("BDW3 WAL open failed");

    Wal3Header wh{};
    if (::pread(fd, &wh, sizeof(wh), 0) != ssize_t(sizeof(wh))) {
        ::close(fd);
        throw std::runtime_error("BDW3 WAL header truncated");
    }
    if (std::memcmp(wh.magic, "BDW3", 4) != 0 || wh.version != 3 ||
        wh.header_size != sizeof(wh) || wal_header_crc(wh) != wh.crc) {
        ::close(fd);
        throw std::runtime_error("BDW3 WAL header invalid");
    }

    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        throw std::runtime_error("BDW3 WAL stat failed");
    }

    off_t pos = sizeof(Wal3Header);
    uint64_t expected = std::max<uint64_t>(state.seq + 1, wh.first_sequence);
    ReplayResult rr{pos, wh.segment_id, false};

    while (pos < st.st_size) {
        uint8_t lb[4];
        ssize_t lr = ::pread(fd, lb, 4, pos);
        if (lr != 4) { rr.torn_tail = true; break; }
        const uint32_t total = be32(lb);
        constexpr uint32_t MIN = 4 + 8 + 1 + 4 + 4 + 4 + 4;
        if (total < MIN || total > (1u << 24)) {
            ::close(fd);
            throw std::runtime_error("BDW3 invalid total_len");
        }
        if (pos + total > st.st_size) { rr.torn_tail = true; break; }

        std::vector<uint8_t> b(total - 4);
        if (::pread(fd, b.data(), b.size(), pos + 4) != ssize_t(b.size())) {
            rr.torn_tail = true;
            break;
        }

        const uint8_t* q = b.data();
        const uint64_t seq = be64(q); q += 8;
        const uint8_t op = *q++;
        const uint32_t kl = be32(q); q += 4;
        const uint32_t vl = be32(q); q += 4;
        const uint32_t hc = be32(q); q += 4;

        std::vector<uint8_t> hh;
        put64(hh, seq); hh.push_back(op); put32(hh, kl); put32(hh, vl);
        if (crc32b(hh.data(), hh.size()) != hc) {
            ::close(fd);
            throw std::runtime_error("BDW3 frame header CRC mismatch");
        }
        if (op != uint8_t(OperationType::Put) && op != uint8_t(OperationType::Delete)) {
            ::close(fd);
            throw std::runtime_error("BDW3 operation invalid");
        }
        if (kl == 0 || kl > (1u << 20) || vl > (1u << 24)) {
            ::close(fd);
            throw std::runtime_error("BDW3 frame bounds invalid");
        }
        if (uint64_t(kl) + vl + 4 != uint64_t(b.data() + b.size() - q)) {
            ::close(fd);
            throw std::runtime_error("BDW3 frame length mismatch");
        }

        const uint32_t record_crc = be32(b.data() + b.size() - 4);
        std::vector<uint8_t> whole;
        whole.reserve(total - 4);
        whole.insert(whole.end(), lb, lb + 4);
        whole.insert(whole.end(), b.begin(), b.end() - 4);
        if (crc32b(whole.data(), whole.size()) != record_crc) {
            ::close(fd);
            throw std::runtime_error("BDW3 record CRC mismatch");
        }

        std::string key(reinterpret_cast<const char*>(q), kl); q += kl;
        std::string value(reinterpret_cast<const char*>(q), vl);

        if (seq > state.seq) {
            if (seq != expected) {
                ::close(fd);
                throw std::runtime_error("BDW3 non-monotonic sequence");
            }
            if (op == uint8_t(OperationType::Put)) state.kv[std::move(key)] = std::move(value);
            else state.kv.erase(key);
            state.seq = seq;
            ++expected;
        }

        pos += total;
        rr.last_good_offset = pos;
    }

    ::close(fd);
    return rr;
}

static std::string wal_name(uint64_t segment_id) {
    char b[48];
    std::snprintf(b, sizeof(b), "wal-%012llu.bdw3", static_cast<unsigned long long>(segment_id));
    return b;
}

} // namespace

class Database::Impl {
public:
    struct Pending {
        uint64_t seq;
        OperationType type;
        std::string key;
        std::string value;
    };

    fs::path dir;
    Options options;

    mutable std::shared_mutex state_mu;
    std::unordered_map<std::string, std::string> kv;

    std::mutex submit_mu;
    std::mutex queue_mu;
    std::condition_variable queue_cv;
    std::condition_variable durable_cv;
    std::deque<Pending> queue;

    std::mutex wal_mu;
    int wal_fd = -1;
    off_t wal_off = 0;
    fs::path active_wal;
    uint64_t segment_id = 0;

    std::atomic<uint64_t> next_seq{0};
    std::atomic<uint64_t> durable_seq{0};
    bool stop = false;
    bool closed = false;
    std::thread writer;

    Impl(fs::path d, Options o) : dir(std::move(d)), options(o) {
        if (options.wal_batch == 0) throw std::invalid_argument("wal_batch must be > 0");
        fs::create_directories(dir);
        recover();
        writer = std::thread([this] { writer_loop(); });
    }

    ~Impl() {
        try { close_impl(); } catch (...) {}
    }

    void create_new_wal(uint64_t seg, uint64_t first_seq) {
        active_wal = dir / wal_name(seg);
        int fd = ::open(active_wal.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
        if (fd < 0) throw std::runtime_error("BDW3 WAL creation failed");

        Wal3Header h{};
        std::memcpy(h.magic, "BDW3", 4);
        h.version = 3;
        h.header_size = sizeof(h);
        h.segment_id = seg;
        h.first_sequence = first_seq;
        h.crc = wal_header_crc(h);
        write_all(fd, &h, sizeof(h));
        if (::fdatasync(fd) != 0) { ::close(fd); throw std::runtime_error("BDW3 header sync failed"); }
        if (options.keep_size_preallocation && options.reserve_bytes > 0) {
            if (::fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, off_t(options.reserve_bytes)) != 0) {
                ::close(fd);
                throw std::runtime_error("BDW3 KEEP_SIZE preallocation failed");
            }
        }
        wal_fd = fd;
        wal_off = sizeof(Wal3Header);
        segment_id = seg;
    }

    void recover() {
        RecoveredState state;
        const fs::path snapshot = dir / "snapshot.bdr3";
        if (fs::exists(snapshot)) state = decode_snapshot(snapshot);

        std::vector<fs::path> wals;
        for (const auto& e : fs::directory_iterator(dir)) {
            if (e.is_regular_file() && e.path().extension() == ".bdw3") wals.push_back(e.path());
        }
        std::sort(wals.begin(), wals.end());

        uint64_t highest_seg = 0;
        ReplayResult last_rr{};
        for (std::size_t i = 0; i < wals.size(); ++i) {
            ReplayResult rr = replay_wal(wals[i], state);
            highest_seg = std::max(highest_seg, rr.segment_id);
            if (rr.torn_tail && i + 1 != wals.size()) throw std::runtime_error("BDW3 torn tail in non-final segment");
            if (i + 1 == wals.size()) last_rr = rr;
        }

        kv = std::move(state.kv);
        next_seq.store(state.seq, std::memory_order_release);
        durable_seq.store(state.seq, std::memory_order_release);

        if (wals.empty()) {
            create_new_wal(1, state.seq + 1);
            fsync_dir(dir);
        } else {
            active_wal = wals.back();
            if (last_rr.torn_tail) {
                int tfd = ::open(active_wal.c_str(), O_RDWR);
                if (tfd < 0) throw std::runtime_error("BDW3 tail repair open failed");
                if (::ftruncate(tfd, last_rr.last_good_offset) != 0 || ::fdatasync(tfd) != 0) {
                    ::close(tfd);
                    throw std::runtime_error("BDW3 tail repair failed");
                }
                ::close(tfd);
            }
            wal_fd = ::open(active_wal.c_str(), O_RDWR);
            if (wal_fd < 0) throw std::runtime_error("BDW3 active WAL open failed");
            wal_off = ::lseek(wal_fd, 0, SEEK_END);
            if (wal_off < off_t(sizeof(Wal3Header))) throw std::runtime_error("BDW3 active WAL invalid size");
            segment_id = highest_seg;
            if (options.keep_size_preallocation && options.reserve_bytes > std::size_t(wal_off)) {
                if (::fallocate(wal_fd, FALLOC_FL_KEEP_SIZE, 0, off_t(options.reserve_bytes)) != 0)
                    throw std::runtime_error("BDW3 KEEP_SIZE re-preallocation failed");
            }
        }
    }

    Ticket submit_impl(Operation operation) {
        if (operation.key.empty()) throw std::invalid_argument("BDR key must not be empty");
        if (operation.key.size() > (1u << 20)) throw std::invalid_argument("BDR key too large");
        if (operation.value.size() > (1u << 24)) throw std::invalid_argument("BDR value too large");
        if (closed) throw std::runtime_error("BDR database is closed");

        std::lock_guard<std::mutex> order_guard(submit_mu);
        const uint64_t seq = next_seq.fetch_add(1, std::memory_order_acq_rel) + 1;

        {
            std::unique_lock<std::shared_mutex> lk(state_mu);
            if (operation.type == OperationType::Put) kv[operation.key] = operation.value;
            else kv.erase(operation.key);
        }
        {
            std::lock_guard<std::mutex> qg(queue_mu);
            queue.push_back(Pending{seq, operation.type, std::move(operation.key), std::move(operation.value)});
        }
        queue_cv.notify_one();
        return Ticket{seq};
    }

    void writer_loop() {
        std::vector<Pending> local;
        std::vector<uint8_t> buffer;
        buffer.reserve(1 << 20);

        for (;;) {
            local.clear();
            {
                std::unique_lock<std::mutex> lk(queue_mu);
                queue_cv.wait(lk, [&] { return stop || !queue.empty(); });
                if (stop && queue.empty()) break;
                while (!queue.empty() && local.size() < options.wal_batch) {
                    local.push_back(std::move(queue.front()));
                    queue.pop_front();
                }
            }

            buffer.clear();
            for (const auto& p : local) {
                auto r = make_record(p.seq, p.type, p.key, p.value);
                buffer.insert(buffer.end(), r.begin(), r.end());
            }

            {
                std::lock_guard<std::mutex> wg(wal_mu);
                pwrite_all(wal_fd, buffer.data(), buffer.size(), wal_off);
                wal_off += off_t(buffer.size());
                if (::fdatasync(wal_fd) != 0) std::abort();
            }
            durable_seq.store(local.back().seq, std::memory_order_release);
            durable_cv.notify_all();
        }
    }

    void wait_impl(Ticket t) {
        if (!t) return;
        if (t.sequence > next_seq.load(std::memory_order_acquire)) throw std::invalid_argument("BDR ticket is from the future");
        std::unique_lock<std::mutex> lk(queue_mu);
        durable_cv.wait(lk, [&] { return durable_seq.load(std::memory_order_acquire) >= t.sequence; });
    }

    void checkpoint_impl() {
        if (closed) throw std::runtime_error("BDR database is closed");
        std::lock_guard<std::mutex> submit_guard(submit_mu);
        const uint64_t seq = next_seq.load(std::memory_order_acquire);
        wait_impl(Ticket{seq});

        std::unordered_map<std::string, std::string> snapshot_state;
        {
            std::shared_lock<std::shared_mutex> lk(state_mu);
            snapshot_state = kv;
        }
        const auto bytes = encode_snapshot(seq, snapshot_state);
        const fs::path tmp = dir / "snapshot.tmp";
        const fs::path dst = dir / "snapshot.bdr3";

        int sfd = ::open(tmp.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (sfd < 0) throw std::runtime_error("BDR3 snapshot temp open failed");
        write_all(sfd, bytes.data(), bytes.size());
        if (::fsync(sfd) != 0) { ::close(sfd); throw std::runtime_error("BDR3 snapshot fsync failed"); }
        ::close(sfd);
        fs::rename(tmp, dst);
        fsync_dir(dir);

        std::lock_guard<std::mutex> wg(wal_mu);
        const fs::path old_active = active_wal;
        if (wal_fd >= 0) {
            if (::fsync(wal_fd) != 0) throw std::runtime_error("BDW3 pre-rotation fsync failed");
            ::close(wal_fd);
            wal_fd = -1;
        }
        create_new_wal(segment_id + 1, seq + 1);
        fsync_dir(dir);

        for (const auto& e : fs::directory_iterator(dir)) {
            if (e.is_regular_file() && e.path().extension() == ".bdw3" && e.path() != active_wal)
                fs::remove(e.path());
        }
        fsync_dir(dir);
    }

    void close_impl() {
        if (closed) return;
        sync_impl();
        {
            std::lock_guard<std::mutex> qg(queue_mu);
            stop = true;
        }
        queue_cv.notify_one();
        if (writer.joinable()) writer.join();
        {
            std::lock_guard<std::mutex> wg(wal_mu);
            if (wal_fd >= 0) {
                ::fdatasync(wal_fd);
                ::close(wal_fd);
                wal_fd = -1;
            }
        }
        closed = true;
    }

    void sync_impl() {
        wait_impl(Ticket{next_seq.load(std::memory_order_acquire)});
    }
};

Database::Database(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Database::~Database() = default;

std::unique_ptr<Database> Database::open(const fs::path& directory, Options options) {
    return std::unique_ptr<Database>(new Database(std::make_unique<Impl>(directory, options)));
}

Ticket Database::submit(Operation operation) { return impl_->submit_impl(std::move(operation)); }
Ticket Database::put(std::string key, std::string value) {
    return submit(Operation{OperationType::Put, std::move(key), std::move(value)});
}
Ticket Database::erase(std::string key) {
    return submit(Operation{OperationType::Delete, std::move(key), {}});
}
void Database::put_sync(std::string key, std::string value) {
    wait(put(std::move(key), std::move(value)));
}
void Database::erase_sync(std::string key) { wait(erase(std::move(key))); }

std::optional<std::string> Database::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lk(impl_->state_mu);
    auto it = impl_->kv.find(key);
    if (it == impl_->kv.end()) return std::nullopt;
    return it->second;
}

void Database::wait(Ticket ticket) { impl_->wait_impl(ticket); }
void Database::sync() { impl_->sync_impl(); }
void Database::checkpoint() { impl_->checkpoint_impl(); }
std::uint64_t Database::last_sequence() const noexcept { return impl_->next_seq.load(std::memory_order_acquire); }
std::uint64_t Database::durable_sequence() const noexcept { return impl_->durable_seq.load(std::memory_order_acquire); }
std::size_t Database::size() const {
    std::shared_lock<std::shared_mutex> lk(impl_->state_mu);
    return impl_->kv.size();
}
void Database::close() { impl_->close_impl(); }

} // namespace bdr
