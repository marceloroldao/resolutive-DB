#include "integrated_database.hpp"
#include "bdr/database.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static std::vector<unsigned char> bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + p.string());
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static std::map<std::string, std::vector<unsigned char>> legacy_image(const fs::path& dir) {
    std::map<std::string, std::vector<unsigned char>> out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() == ".bdw3" || e.path().filename() == "snapshot.bdr3")
            out[e.path().filename().string()] = bytes(e.path());
    }
    return out;
}

static void require(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

int main() {
    const fs::path root = fs::temp_directory_path() / "bdr_v107_integrated";
    const fs::path legacy = root / "legacy";
    const fs::path wal4 = root / "v11.bdw4";
    fs::remove_all(root);
    fs::create_directories(legacy);

    {
        auto db = bdr::Database::open(legacy);
        db->put_sync("alpha", "one");
        db->put_sync("beta", "two");
        db->checkpoint();
        db->put_sync("gamma", "three");
        db->close();
    }

    const auto before = legacy_image(legacy);
    require(!before.empty(), "legacy image missing");
    const auto legacy_state = bdr::v106::read_v1_state(legacy);
    require(legacy_state.values.at("alpha") == "one", "legacy alpha mismatch");
    require(legacy_state.values.at("gamma") == "three", "legacy WAL mismatch");
    const std::uint64_t base = legacy_state.sequence;

    constexpr int threads = 6;
    constexpr int batches_per_thread = 50;
    std::vector<std::pair<std::uint64_t, std::string>> observed;
    std::mutex observed_mu;

    {
        bdr::v107::IntegratedDatabase db(legacy, wal4);
        require(db.last_sequence() == base, "candidate did not inherit legacy sequence");
        require(db.get("beta") == std::optional<std::string>("two"), "candidate did not inherit legacy state");

        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&, t] {
                for (int i = 0; i < batches_per_thread; ++i) {
                    const std::string value = "t" + std::to_string(t) + "-" + std::to_string(i);
                    std::vector<bdr::v101::Operation> ops{
                        {bdr::v101::OpType::Put, "shared", value},
                        {bdr::v101::OpType::Put, "k" + std::to_string(t) + "-" + std::to_string(i), value},
                    };
                    const auto r = db.write_batch(std::move(ops));
                    require(r.operations == 2 && r.durable, "batch result contract mismatch");
                    std::lock_guard<std::mutex> lock(observed_mu);
                    observed.emplace_back(r.sequence, value);
                }
            });
        }
        for (auto& w : workers) w.join();

        require(observed.size() == std::size_t(threads * batches_per_thread), "missing concurrent batches");
        std::sort(observed.begin(), observed.end());
        for (std::size_t i = 0; i < observed.size(); ++i)
            require(observed[i].first == base + i + 1, "sequence is not total/continuous");

        const auto p = db.put("tail", "ok");
        require(p.sequence == base + observed.size() + 1, "simple put ordering mismatch");
        const auto e = db.erase("beta");
        require(e.sequence == p.sequence + 1, "simple erase ordering mismatch");
        require(db.durable_sequence() == e.sequence, "durability watermark mismatch");
        require(db.get("shared") == std::optional<std::string>(observed.back().second), "largest sequence does not own final value");
    }

    require(legacy_image(legacy) == before, "v1.0 BDR3/BDW3 files were modified");

    {
        bdr::v107::IntegratedDatabase reopened(legacy, wal4);
        const auto expected = base + threads * batches_per_thread + 2;
        require(reopened.last_sequence() == expected, "reopen sequence mismatch");
        require(reopened.durable_sequence() == expected, "reopen durable sequence mismatch");
        require(reopened.get("alpha") == std::optional<std::string>("one"), "legacy state lost after reopen");
        require(!reopened.get("beta"), "delete lost after reopen");
        require(reopened.get("tail") == std::optional<std::string>("ok"), "BDW4 tail lost after reopen");
        require(reopened.get("shared") == std::optional<std::string>(observed.back().second), "concurrent final value lost after reopen");
    }

    require(legacy_image(legacy) == before, "reopen modified legacy files");
    fs::remove_all(root);
    std::cout << "V107 integrated candidate PASS: migration + atomic batch + total ordering + durable reopen\n";
    return 0;
}
