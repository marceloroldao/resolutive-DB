#include "file_wal.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using bdr::v101::OpType;
using bdr::v101::Operation;

static void write_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

static std::vector<Operation> memory_batch(const std::string& id) {
    return {
        {OpType::Put, id + "/payload", "payload"},
        {OpType::Put, id + "/node/0", "alpha"},
        {OpType::Put, id + "/occ/0", "1"},
        {OpType::Put, id + "/meta", "source=test"},
    };
}

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr-v102-test";
    fs::remove_all(root);
    fs::create_directories(root);

    const auto first_ops = memory_batch("m1");
    const auto second_ops = memory_batch("m2");
    const auto first = bdr::v101::encode_batch(1, first_ops);
    const auto second = bdr::v101::encode_batch(2, second_ops);

    // Every possible torn tail in the first batch must recover as none.
    for (std::size_t cut = 0; cut < first.size(); ++cut) {
        const auto path = root / "first-torn.bdw4";
        write_bytes(path, {first.begin(), first.begin() + static_cast<std::ptrdiff_t>(cut)});
        std::map<std::string, std::string> state;
        const auto result = bdr::v102::recover_file(path, state);
        assert(state.empty());
        assert(result.last_sequence == 0);
        assert(result.committed_batches == 0);
        if (cut != 0) assert(result.repaired_torn_tail);
        assert(fs::file_size(path) == 0);
    }

    // One complete batch followed by every possible torn prefix of batch two.
    for (std::size_t cut = 0; cut < second.size(); ++cut) {
        const auto path = root / "second-torn.bdw4";
        std::vector<std::uint8_t> bytes = first;
        bytes.insert(bytes.end(), second.begin(), second.begin() + static_cast<std::ptrdiff_t>(cut));
        write_bytes(path, bytes);
        std::map<std::string, std::string> state;
        const auto result = bdr::v102::recover_file(path, state);
        assert(result.last_sequence == 1);
        assert(result.committed_batches == 1);
        assert(state.size() == first_ops.size());
        assert(state.count("m1/payload") == 1);
        assert(state.count("m2/payload") == 0);
        assert(fs::file_size(path) == first.size());
    }

    // Durable append path: acknowledged batches survive reopen in full.
    {
        const auto path = root / "durable.bdw4";
        fs::remove(path);
        bdr::v102::append_batch(path, 1, first_ops, true);
        bdr::v102::append_batch(path, 2, second_ops, true);
        std::map<std::string, std::string> state;
        const auto result = bdr::v102::recover_file(path, state);
        assert(result.last_sequence == 2);
        assert(result.committed_batches == 2);
        assert(state.size() == first_ops.size() + second_ops.size());
        assert(state.at("m1/payload") == "payload");
        assert(state.at("m2/meta") == "source=test");
    }

    // Repair must make a second reopen deterministic.
    {
        const auto path = root / "repair-reopen.bdw4";
        std::vector<std::uint8_t> bytes = first;
        bytes.insert(bytes.end(), second.begin(), second.begin() + static_cast<std::ptrdiff_t>(second.size() / 2));
        write_bytes(path, bytes);
        std::map<std::string, std::string> state1;
        auto r1 = bdr::v102::recover_file(path, state1);
        assert(r1.repaired_torn_tail);
        std::map<std::string, std::string> state2;
        auto r2 = bdr::v102::recover_file(path, state2);
        assert(!r2.repaired_torn_tail);
        assert(state1 == state2);
        assert(r2.last_sequence == 1);
    }

    fs::remove_all(root);
    std::cout << "V102 file WAL atomic recovery PASS\n";
    return 0;
}
