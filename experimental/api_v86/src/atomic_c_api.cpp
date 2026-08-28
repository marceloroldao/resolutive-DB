#include "bdr/atomic_c_api.h"
#include "bdr/atomic_database.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct bdr_atomic_c_handle {
    std::unique_ptr<bdr::AtomicDatabase> db;
};

static std::string bytes_to_string(const void *data, size_t size) {
    return std::string(static_cast<const char *>(data), size);
}

extern "C" uint32_t bdr_atomic_c_abi_version(void) {
    return BDR_ATOMIC_C_ABI_VERSION;
}

extern "C" bdr_atomic_c_status bdr_atomic_c_open(const char *directory, bdr_atomic_c_handle **out_handle) {
    if (!directory || !*directory || !out_handle) return BDR_ATOMIC_C_INVALID_ARGUMENT;
    *out_handle = nullptr;
    try {
        const std::filesystem::path dir(directory);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) return BDR_ATOMIC_C_IO_ERROR;
        std::unique_ptr<bdr_atomic_c_handle> h(new bdr_atomic_c_handle{});
        h->db = bdr::AtomicDatabase::open(dir);
        if (!h->db) return BDR_ATOMIC_C_IO_ERROR;
        *out_handle = h.release();
        return BDR_ATOMIC_C_OK;
    } catch (...) {
        return BDR_ATOMIC_C_IO_ERROR;
    }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_write_batch(
    bdr_atomic_c_handle *handle,
    const bdr_atomic_c_operation *operations,
    size_t operation_count,
    bdr_atomic_c_batch_result *out_result) {
    if (!handle || !handle->db || !operations || operation_count == 0 || !out_result)
        return BDR_ATOMIC_C_INVALID_ARGUMENT;
    try {
        std::vector<bdr::Operation> batch;
        batch.reserve(operation_count);
        for (size_t i = 0; i < operation_count; ++i) {
            const auto &op = operations[i];
            if (!op.key || op.key_size == 0) return BDR_ATOMIC_C_INVALID_ARGUMENT;
            if (op.type != BDR_ATOMIC_C_PUT && op.type != BDR_ATOMIC_C_DELETE)
                return BDR_ATOMIC_C_INVALID_ARGUMENT;
            bdr::Operation item;
            item.type = op.type == BDR_ATOMIC_C_DELETE ? bdr::OperationType::Delete : bdr::OperationType::Put;
            item.key = bytes_to_string(op.key, op.key_size);
            if (item.type == bdr::OperationType::Put) {
                if (!op.value && op.value_size != 0) return BDR_ATOMIC_C_INVALID_ARGUMENT;
                item.value = bytes_to_string(op.value, op.value_size);
            }
            batch.push_back(std::move(item));
        }
        auto result = handle->db->write_batch(std::move(batch), bdr::DurabilityMode::BatchSync);
        out_result->sequence = result.sequence;
        out_result->operations = result.operations;
        out_result->durable = result.durable ? 1 : 0;
        return BDR_ATOMIC_C_OK;
    } catch (...) {
        return BDR_ATOMIC_C_IO_ERROR;
    }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_get(
    bdr_atomic_c_handle *handle,
    const void *key,
    size_t key_size,
    bdr_atomic_c_buffer *out_value) {
    if (!handle || !handle->db || !key || key_size == 0 || !out_value)
        return BDR_ATOMIC_C_INVALID_ARGUMENT;
    out_value->data = nullptr;
    out_value->size = 0;
    try {
        auto value = handle->db->get(bytes_to_string(key, key_size));
        if (!value) return BDR_ATOMIC_C_NOT_FOUND;
        if (!value->empty()) {
            out_value->data = static_cast<uint8_t *>(std::malloc(value->size()));
            if (!out_value->data) return BDR_ATOMIC_C_INTERNAL_ERROR;
            std::memcpy(out_value->data, value->data(), value->size());
        }
        out_value->size = value->size();
        return BDR_ATOMIC_C_OK;
    } catch (...) {
        return BDR_ATOMIC_C_IO_ERROR;
    }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_exists(
    bdr_atomic_c_handle *handle,
    const void *key,
    size_t key_size,
    int *out_exists) {
    if (!handle || !handle->db || !key || key_size == 0 || !out_exists)
        return BDR_ATOMIC_C_INVALID_ARGUMENT;
    try {
        *out_exists = handle->db->contains(bytes_to_string(key, key_size)) ? 1 : 0;
        return BDR_ATOMIC_C_OK;
    } catch (...) {
        return BDR_ATOMIC_C_IO_ERROR;
    }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_sync(bdr_atomic_c_handle *handle) {
    if (!handle || !handle->db) return BDR_ATOMIC_C_INVALID_ARGUMENT;
    try { handle->db->sync(); return BDR_ATOMIC_C_OK; } catch (...) { return BDR_ATOMIC_C_IO_ERROR; }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_last_sequence(bdr_atomic_c_handle *handle, uint64_t *out_sequence) {
    if (!handle || !handle->db || !out_sequence) return BDR_ATOMIC_C_INVALID_ARGUMENT;
    try { *out_sequence = handle->db->last_sequence(); return BDR_ATOMIC_C_OK; } catch (...) { return BDR_ATOMIC_C_IO_ERROR; }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_durable_sequence(bdr_atomic_c_handle *handle, uint64_t *out_sequence) {
    if (!handle || !handle->db || !out_sequence) return BDR_ATOMIC_C_INVALID_ARGUMENT;
    try { *out_sequence = handle->db->durable_sequence(); return BDR_ATOMIC_C_OK; } catch (...) { return BDR_ATOMIC_C_IO_ERROR; }
}

extern "C" bdr_atomic_c_status bdr_atomic_c_integrity_check(bdr_atomic_c_handle *handle) {
    if (!handle || !handle->db) return BDR_ATOMIC_C_INVALID_ARGUMENT;
    try {
        const auto last = handle->db->last_sequence();
        const auto durable = handle->db->durable_sequence();
        if (durable > last) return BDR_ATOMIC_C_INTERNAL_ERROR;
        (void)handle->db->size();
        return BDR_ATOMIC_C_OK;
    } catch (...) {
        return BDR_ATOMIC_C_IO_ERROR;
    }
}

extern "C" void bdr_atomic_c_free_buffer(bdr_atomic_c_buffer buffer) {
    std::free(buffer.data);
}

extern "C" void bdr_atomic_c_close(bdr_atomic_c_handle *handle) {
    delete handle;
}
