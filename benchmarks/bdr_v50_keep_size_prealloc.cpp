#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <linux/falloc.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <zlib.h>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static uint32_t crc32b(const void* p, size_t n, uint32_t seed = 0) {
    return uint32_t(crc32(seed, static_cast<const Bytef*>(p), uInt(n)));
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
static void pwrite_all(int fd, const uint8_t* p, size_t n, off_t off) {
    while (n) {
        ssize_t w = ::pwrite(fd, p, n, off);
        if (w <= 0) throw std::runtime_error("pwrite");
        p += w; n -= size_t(w); off += w;
    }
}

#pragma pack(push,1)
struct W3H {
    char magic[4];
    uint16_t version;
    uint16_t hsize;
    uint32_t flags;
    uint64_t seg;
    uint64_t first_seq;
    uint32_t crc;
};
#pragma pack(pop)

static uint32_t hcrc(const W3H& h) { return crc32b(&h, sizeof(h) - 4); }

static std::vector<uint8_t> header3() {
    W3H h{};
    std::memcpy(h.magic, "BDW3", 4);
    h.version = 3;
    h.hsize = sizeof(h);
    h.seg = 1;
    h.first_seq = 1;
    h.crc = hcrc(h);
    std::vector<uint8_t> b(sizeof(h));
    std::memcpy(b.data(), &h, sizeof(h));
    return b;
}

static std::vector<uint8_t> rec3(uint64_t seq, const std::string& k, const std::string& v) {
    std::vector<uint8_t> h;
    put64(h, seq);
    h.push_back(1);
    put32(h, uint32_t(k.size()));
    put32(h, uint32_t(v.size()));
    uint32_t hc = crc32b(h.data(), h.size());
    uint32_t total = 4 + uint32_t(h.size()) + 4 + uint32_t(k.size() + v.size()) + 4;
    std::vector<uint8_t> b;
    put32(b, total);
    b.insert(b.end(), h.begin(), h.end());
    put32(b, hc);
    b.insert(b.end(), k.begin(), k.end());
    b.insert(b.end(), v.begin(), v.end());
    put32(b, crc32b(b.data(), b.size()));
    return b;
}

enum class Mode { APPEND, POSIX_EXTEND, KEEP_SIZE };
struct Result {
    std::string mode;
    int batch;
    double ops_s;
    uint64_t logical_bytes;
    uint64_t expected_bytes;
    uint64_t allocated_bytes;
    size_t recovered;
    bool exact_eof;
    bool pass;
};

static size_t scan(const fs::path& p) {
    int fd = ::open(p.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("open scan");
    struct stat st{};
    if (::fstat(fd, &st)) throw std::runtime_error("stat");
    if (st.st_size < (off_t)sizeof(W3H)) throw std::runtime_error("short header");
    W3H wh{};
    if (::pread(fd, &wh, sizeof(wh), 0) != (ssize_t)sizeof(wh)) throw std::runtime_error("header read");
    if (std::memcmp(wh.magic, "BDW3", 4) || wh.version != 3 || wh.hsize != sizeof(wh) || hcrc(wh) != wh.crc)
        throw std::runtime_error("header invalid");
    off_t pos = sizeof(W3H);
    uint64_t expected_seq = 1;
    size_t good = 0;
    while (pos < st.st_size) {
        uint8_t lb[4];
        ssize_t r = ::pread(fd, lb, 4, pos);
        if (r != 4) throw std::runtime_error("torn len");
        uint32_t total = be32(lb);
        constexpr uint32_t MIN = 4 + 8 + 1 + 4 + 4 + 4 + 4;
        if (total < MIN || total > (1u << 24)) throw std::runtime_error("invalid total_len");
        if (pos + total > st.st_size) throw std::runtime_error("torn frame");
        std::vector<uint8_t> b(total - 4);
        if (::pread(fd, b.data(), b.size(), pos + 4) != (ssize_t)b.size()) throw std::runtime_error("frame read");
        const uint8_t* q = b.data();
        uint64_t seq = be64(q); q += 8;
        uint8_t op = *q++;
        uint32_t kl = be32(q); q += 4;
        uint32_t vl = be32(q); q += 4;
        uint32_t hc = be32(q); q += 4;
        std::vector<uint8_t> hh;
        put64(hh, seq); hh.push_back(op); put32(hh, kl); put32(hh, vl);
        if (crc32b(hh.data(), hh.size()) != hc || op != 1 || seq != expected_seq)
            throw std::runtime_error("header crc/op/seq");
        if (uint64_t(kl) + vl + 4 != uint64_t(b.data() + b.size() - q)) throw std::runtime_error("bounds");
        uint32_t got = be32(b.data() + b.size() - 4);
        std::vector<uint8_t> whole;
        whole.insert(whole.end(), lb, lb + 4);
        whole.insert(whole.end(), b.begin(), b.end() - 4);
        if (crc32b(whole.data(), whole.size()) != got) throw std::runtime_error("record crc");
        ++good; ++expected_seq; pos += total;
    }
    ::close(fd);
    return good;
}

static Result run(Mode mode, int batch, int N) {
    std::string name = mode == Mode::APPEND ? "append" : mode == Mode::POSIX_EXTEND ? "posix_fallocate" : "fallocate_keep_size";
    fs::path path = "v50_" + name + "_" + std::to_string(batch) + ".bdw3";
    fs::remove(path);
    int flags = O_CREAT | O_TRUNC | O_RDWR;
    int fd = ::open(path.c_str(), flags, 0644);
    if (fd < 0) throw std::runtime_error("open");

    const size_t reserve = size_t(N) * 96 + 4096;
    if (mode == Mode::POSIX_EXTEND) {
        int rc = ::posix_fallocate(fd, 0, reserve);
        if (rc != 0) throw std::runtime_error("posix_fallocate");
    } else if (mode == Mode::KEEP_SIZE) {
        if (::fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, reserve) != 0) throw std::runtime_error("fallocate KEEP_SIZE");
    }

    off_t off = 0;
    auto wh = header3();
    pwrite_all(fd, wh.data(), wh.size(), off);
    off += wh.size();

    std::vector<uint8_t> buf;
    buf.reserve(size_t(batch) * 96);
    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        auto r = rec3(i + 1, "K" + std::to_string(i), "VALUE_" + std::to_string(i));
        buf.insert(buf.end(), r.begin(), r.end());
        if ((i + 1) % batch == 0) {
            pwrite_all(fd, buf.data(), buf.size(), off);
            off += buf.size();
            buf.clear();
            if (::fdatasync(fd)) throw std::runtime_error("fdatasync");
        }
    }
    if (!buf.empty()) {
        pwrite_all(fd, buf.data(), buf.size(), off);
        off += buf.size();
        buf.clear();
        if (::fdatasync(fd)) throw std::runtime_error("fdatasync final");
    }
    auto t1 = Clock::now();

    // POSIX_EXTEND intentionally demonstrates the ambiguity: trim it before scanning so the
    // benchmark can still report throughput while exact_eof remains false.
    struct stat st_before{};
    if (::fstat(fd, &st_before)) throw std::runtime_error("fstat");
    uint64_t logical_before = uint64_t(st_before.st_size);
    uint64_t allocated = uint64_t(st_before.st_blocks) * 512ull;
    bool exact_eof = logical_before == uint64_t(off);
    if (!exact_eof && mode == Mode::POSIX_EXTEND) {
        if (::ftruncate(fd, off)) throw std::runtime_error("truncate");
        if (::fdatasync(fd)) throw std::runtime_error("sync truncate");
    }
    ::close(fd);

    size_t recovered = scan(path);
    double ops = N / std::chrono::duration<double>(t1 - t0).count();
    bool pass = recovered == size_t(N) && (mode != Mode::KEEP_SIZE || exact_eof);
    return {name, batch, ops, logical_before, uint64_t(off), allocated, recovered, exact_eof, pass};
}

int main(int argc, char** argv) {
    int N = argc > 1 ? std::stoi(argv[1]) : 100000;
    std::cout << "mode,batch,ops_s,logical_bytes,expected_bytes,allocated_bytes,recovered,exact_eof,pass\n";
    int fail = 0;
    for (int batch : {1, 32, 128, 512}) {
        for (Mode mode : {Mode::APPEND, Mode::POSIX_EXTEND, Mode::KEEP_SIZE}) {
            auto r = run(mode, batch, N);
            std::cout << r.mode << ',' << r.batch << ',' << r.ops_s << ',' << r.logical_bytes << ','
                      << r.expected_bytes << ',' << r.allocated_bytes << ',' << r.recovered << ','
                      << r.exact_eof << ',' << r.pass << '\n';
            if (!r.pass) ++fail;
        }
    }
    return fail ? 2 : 0;
}
