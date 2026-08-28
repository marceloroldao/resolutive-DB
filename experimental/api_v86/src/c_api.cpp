#include "bdr/c_api.h"
#include "bdr/database.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>

struct bdr_c_handle {
    std::unique_ptr<bdr::Database> db;
};

static std::string bytes_to_string(const void *data, size_t size) {
    return std::string(static_cast<const char *>(data), size);
}

extern "C" bdr_c_status bdr_c_open(const char *directory, bdr_c_handle **out_handle) {
    if (!directory || !*directory || !out_handle) return BDR_C_INVALID_ARGUMENT;
    try {
        std::unique_ptr<bdr_c_handle> h(new bdr_c_handle{});
        h->db = bdr::Database::open(directory);
        if (!h->db) return BDR_C_IO_ERROR;
        *out_handle = h.release();
        return BDR_C_OK;
    } catch (...) {
        return BDR_C_IO_ERROR;
    }
}

extern "C" bdr_c_status bdr_c_put(bdr_c_handle *handle, const void *key, size_t key_size, const void *value, size_t value_size) {
    if (!handle || !handle->db || !key || key_size == 0 || (!value && value_size != 0)) return BDR_C_INVALID_ARGUMENT;
    try {
        handle->db->put_sync(bytes_to_string(key, key_size), bytes_to_string(value, value_size));
        return BDR_C_OK;
    } catch (...) {
        return BDR_C_IO_ERROR;
    }
}

extern "C" bdr_c_status bdr_c_get(bdr_c_handle *handle, const void *key, size_t key_size, bdr_c_buffer *out_value) {
    if (!handle || !handle->db || !key || key_size == 0 || !out_value) return BDR_C_INVALID_ARGUMENT;
    out_value->data = nullptr; out_value->size = 0;
    try {
        auto value = handle->db->get(bytes_to_string(key, key_size));
        if (!value) return BDR_C_NOT_FOUND;
        if (!value->empty()) {
            out_value->data = static_cast<uint8_t *>(std::malloc(value->size()));
            if (!out_value->data) return BDR_C_INTERNAL_ERROR;
            std::memcpy(out_value->data, value->data(), value->size());
        }
        out_value->size = value->size();
        return BDR_C_OK;
    } catch (...) {
        return BDR_C_IO_ERROR;
    }
}

extern "C" bdr_c_status bdr_c_delete(bdr_c_handle *handle, const void *key, size_t key_size) {
    if (!handle || !handle->db || !key || key_size == 0) return BDR_C_INVALID_ARGUMENT;
    try {
        handle->db->erase_sync(bytes_to_string(key, key_size));
        return BDR_C_OK;
    } catch (...) {
        return BDR_C_IO_ERROR;
    }
}

extern "C" bdr_c_status bdr_c_sync(bdr_c_handle *handle) {
    if (!handle || !handle->db) return BDR_C_INVALID_ARGUMENT;
    try { handle->db->sync(); return BDR_C_OK; } catch (...) { return BDR_C_IO_ERROR; }
}

extern "C" bdr_c_status bdr_c_checkpoint(bdr_c_handle *handle) {
    if (!handle || !handle->db) return BDR_C_INVALID_ARGUMENT;
    try { handle->db->checkpoint(); return BDR_C_OK; } catch (...) { return BDR_C_IO_ERROR; }
}

extern "C" void bdr_c_free_buffer(bdr_c_buffer buffer) { std::free(buffer.data); }

extern "C" void bdr_c_close(bdr_c_handle *handle) {
    if (!handle) return;
    try { if (handle->db) handle->db->close(); } catch (...) {}
    delete handle;
}
