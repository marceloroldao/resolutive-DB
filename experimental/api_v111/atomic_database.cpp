#include "../api_v86/include/bdr/atomic_database.hpp"
#include "../api_v110/durable_database.hpp"

#include <stdexcept>
#include <utility>

namespace bdr {
namespace {

v110::DurabilityMode to_internal(DurabilityMode mode) {
    switch (mode) {
        case DurabilityMode::Async: return v110::DurabilityMode::Async;
        case DurabilityMode::BatchSync: return v110::DurabilityMode::BatchSync;
        case DurabilityMode::PerOperationSync: return v110::DurabilityMode::PerOperationSync;
    }
    throw std::invalid_argument("unknown DurabilityMode");
}

v101::Operation to_internal(Operation op) {
    v101::Operation out;
    out.type = op.type == OperationType::Put ? v101::OpType::Put : v101::OpType::Delete;
    out.key = std::move(op.key);
    out.value = std::move(op.value);
    return out;
}

BatchResult to_public(v110::BatchResult r) {
    return {r.sequence, r.operations, r.durable};
}

} // namespace

class AtomicDatabase::Impl {
public:
    Impl(std::filesystem::path legacy_directory, std::filesystem::path bdw4_path)
        : db(std::move(legacy_directory), std::move(bdw4_path)) {}

    v110::DurableDatabase db;
};

std::unique_ptr<AtomicDatabase> AtomicDatabase::open(
    const std::filesystem::path& legacy_directory,
    const std::filesystem::path& bdw4_path) {
    auto path = bdw4_path;
    if (path.empty()) path = legacy_directory / "atomic.bdw4";
    return std::unique_ptr<AtomicDatabase>(
        new AtomicDatabase(std::make_unique<Impl>(legacy_directory, path)));
}

AtomicDatabase::AtomicDatabase(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
AtomicDatabase::~AtomicDatabase() = default;

BatchResult AtomicDatabase::write_batch(std::vector<Operation> operations,
                                        DurabilityMode durability) {
    std::vector<v101::Operation> internal;
    internal.reserve(operations.size());
    for (auto& op : operations) internal.push_back(to_internal(std::move(op)));
    return to_public(impl_->db.write_batch(std::move(internal), to_internal(durability)));
}

BatchResult AtomicDatabase::put(std::string key,
                                std::string value,
                                DurabilityMode durability) {
    return to_public(impl_->db.put(std::move(key), std::move(value), to_internal(durability)));
}

BatchResult AtomicDatabase::erase(std::string key,
                                  DurabilityMode durability) {
    return to_public(impl_->db.erase(std::move(key), to_internal(durability)));
}

BatchResult AtomicDatabase::put_many(
    std::vector<std::pair<std::string, std::string>> entries,
    DurabilityMode durability) {
    return to_public(impl_->db.put_many(std::move(entries), to_internal(durability)));
}

BatchResult AtomicDatabase::erase_many(std::vector<std::string> keys,
                                       DurabilityMode durability) {
    return to_public(impl_->db.erase_many(std::move(keys), to_internal(durability)));
}

std::optional<std::string> AtomicDatabase::get(const std::string& key) const {
    return impl_->db.get(key);
}

bool AtomicDatabase::contains(const std::string& key) const {
    return static_cast<bool>(get(key));
}

void AtomicDatabase::sync() { impl_->db.sync(); }
std::uint64_t AtomicDatabase::last_sequence() const { return impl_->db.last_sequence(); }
std::uint64_t AtomicDatabase::durable_sequence() const { return impl_->db.durable_sequence(); }
std::size_t AtomicDatabase::size() const { return impl_->db.size(); }

} // namespace bdr
