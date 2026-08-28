#include "bdr/c_api.h"
#include "bdr/atomic_database.hpp"

#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <new>
#include <string>

struct bdr_c_database {
    std::unique_ptr<bdr::AtomicDatabase> impl;
};

namespace {
thread_local std::string g_last_error;

void set_error(const char* message) {
    g_last_error = message ? message : "unknown error";
}

void set_error(const std::exception& e) {
    g_last_error = e.what();
}

bdr::DurabilityMode to_mode(bdr_c_durability mode) {
    switch (mode) {
        case BDR_C_ASYNC: return bdr::DurabilityMode::Async;
        case BDR_C_BATCH_SYNC: return bdr::DurabilityMode::BatchSync;
        case BDR_C_PER_OPERATION_SYNC: return bdr::DurabilityMode::PerOperationSync;
    }
    throw std::invalid_argument("invalid durability mode");
}

bool valid_bytes(const void* data, size_t size) {
    return size == 0 || data != nullptr;
}

std::string bytes_to_string(const void* data, size_t size) {
    if (size == 0) return {};
    return std::string(static_cast<const char*>(data), size);
}
}

extern "C" {

uint32_t bdr_c_abi_version(void) {
    return BDR_C_ABI_VERSION;
}

const char* bdr_c_last_error(void) {
    return g_last_error.c_str();
}

bdr_c_status bdr_c_open(const char* directory, const char* bdw4_path, bdr_c_database** out_db) {
    if (!directory || !out_db) {
        set_error("directory and out_db are required");
        return BDR_C_INVALID_ARGUMENT;
    }
    *out_db = nullptr;
    try {
        auto holder = std::make_unique<bdr_c_database>();
        const std::filesystem::path dir(directory);
        const std::filesystem::path wal = (bdw4_path && *bdw4_path) ? std::filesystem::path(bdw4_path) : std::filesystem::path{};
        holder->impl = bdr::AtomicDatabase::open(dir, wal);
        *out_db = holder.release();
        g_last_error.clear();
        return BDR_C_OK;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_C_IO_ERROR;
    } catch (...) {
        set_error("unknown error while opening BDR");
        return BDR_C_INTERNAL_ERROR;
    }
}

void bdr_c_close(bdr_c_database* db) {
    delete db;
}

bdr_c_status bdr_c_put(bdr_c_database* db,
                       const void* key,
                       size_t key_size,
                       const void* value,
                       size_t value_size,
                       bdr_c_durability durability) {
    if (!db || !db->impl || !valid_bytes(key, key_size) || !valid_bytes(value, value_size)) {
        set_error("invalid put arguments");
        return BDR_C_INVALID_ARGUMENT;
    }
    try {
        db->impl->put(bytes_to_string(key, key_size), bytes_to_string(value, value_size), to_mode(durability));
        g_last_error.clear();
        return BDR_C_OK;
    } catch (const std::invalid_argument& e) {
        set_error(e);
        return BDR_C_INVALID_ARGUMENT;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_C_IO_ERROR;
    } catch (...) {
        set_error("unknown error while writing BDR");
        return BDR_C_INTERNAL_ERROR;
    }
}

bdr_c_status bdr_c_get(bdr_c_database* db,
                       const void* key,
                       size_t key_size,
                       void* out_value,
                       size_t out_capacity,
                       size_t* out_size) {
    if (!db || !db->impl || !valid_bytes(key, key_size) || !out_size) {
        set_error("invalid get arguments");
        return BDR_C_INVALID_ARGUMENT;
    }
    try {
        const auto value = db->impl->get(bytes_to_string(key, key_size));
        if (!value) {
            *out_size = 0;
            set_error("key not found");
            return BDR_C_NOT_FOUND;
        }
        *out_size = value->size();
        if (value->size() > out_capacity || (value->size() > 0 && out_value == nullptr)) {
            set_error("output buffer too small");
            return BDR_C_BUFFER_TOO_SMALL;
        }
        if (!value->empty()) std::memcpy(out_value, value->data(), value->size());
        g_last_error.clear();
        return BDR_C_OK;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_C_IO_ERROR;
    } catch (...) {
        set_error("unknown error while reading BDR");
        return BDR_C_INTERNAL_ERROR;
    }
}

bdr_c_status bdr_c_contains(bdr_c_database* db,
                            const void* key,
                            size_t key_size,
                            int* out_contains) {
    if (!db || !db->impl || !valid_bytes(key, key_size) || !out_contains) {
        set_error("invalid contains arguments");
        return BDR_C_INVALID_ARGUMENT;
    }
    try {
        *out_contains = db->impl->contains(bytes_to_string(key, key_size)) ? 1 : 0;
        g_last_error.clear();
        return BDR_C_OK;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_C_IO_ERROR;
    } catch (...) {
        set_error("unknown error while checking BDR");
        return BDR_C_INTERNAL_ERROR;
    }
}

bdr_c_status bdr_c_sync(bdr_c_database* db) {
    if (!db || !db->impl) {
        set_error("invalid database handle");
        return BDR_C_INVALID_ARGUMENT;
    }
    try {
        db->impl->sync();
        g_last_error.clear();
        return BDR_C_OK;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_C_IO_ERROR;
    } catch (...) {
        set_error("unknown error while syncing BDR");
        return BDR_C_INTERNAL_ERROR;
    }
}

uint64_t bdr_c_last_sequence(const bdr_c_database* db) {
    return (db && db->impl) ? db->impl->last_sequence() : 0;
}

uint64_t bdr_c_durable_sequence(const bdr_c_database* db) {
    return (db && db->impl) ? db->impl->durable_sequence() : 0;
}

size_t bdr_c_size(const bdr_c_database* db) {
    return (db && db->impl) ? db->impl->size() : 0;
}

} // extern "C"
