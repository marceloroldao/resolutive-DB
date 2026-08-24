#include "bdr/c_api.h"
#include "bdr/database.hpp"

#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

struct bdr_db {
    std::unique_ptr<bdr::Database> impl;
};

namespace {
thread_local std::string g_last_error;

static void set_error(const char* msg) {
    g_last_error = msg ? msg : "unknown BDR error";
}

static void set_error(const std::exception& e) {
    g_last_error = e.what();
}

static std::string bytes_to_string(const void* p, size_t n) {
    if (n == 0) return {};
    if (!p) throw std::invalid_argument("null buffer with non-zero size");
    return std::string(static_cast<const char*>(p), n);
}

static bdr::Options convert_options(const bdr_options* in) {
    bdr::Options out;
    if (!in) return out;
    if (in->abi_version != BDR_C_ABI_VERSION)
        throw std::invalid_argument("unsupported BDR C ABI version");
    if (in->struct_size != sizeof(bdr_options))
        throw std::invalid_argument("bdr_options struct_size mismatch");
    out.reserve_bytes = in->reserve_bytes;
    out.wal_batch = in->wal_batch;
    out.partition_count = in->partition_count;
    out.partition_max_load = in->partition_max_load;
    out.keep_size_preallocation = in->keep_size_preallocation != 0;
    return out;
}

template <class F>
static bdr_status guard(F&& fn) {
    try {
        g_last_error.clear();
        fn();
        return BDR_OK;
    } catch (const std::invalid_argument& e) {
        set_error(e);
        return BDR_INVALID_ARGUMENT;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_ERROR;
    } catch (...) {
        set_error("unknown non-standard exception");
        return BDR_ERROR;
    }
}
}

extern "C" {

uint32_t bdr_abi_version(void) {
    return BDR_C_ABI_VERSION;
}

size_t bdr_options_size(void) {
    return sizeof(bdr_options);
}

bdr_options bdr_default_options(void) {
    bdr::Options o;
    bdr_options c{};
    c.abi_version = BDR_C_ABI_VERSION;
    c.struct_size = sizeof(bdr_options);
    c.reserve_bytes = o.reserve_bytes;
    c.wal_batch = o.wal_batch;
    c.partition_count = o.partition_count;
    c.partition_max_load = o.partition_max_load;
    c.keep_size_preallocation = o.keep_size_preallocation ? 1 : 0;
    return c;
}

bdr_status bdr_open(const char* directory, const bdr_options* options, bdr_db** out_db) {
    if (!directory || !out_db) {
        set_error("directory and out_db are required");
        return BDR_INVALID_ARGUMENT;
    }
    *out_db = nullptr;
    return guard([&] {
        auto h = std::make_unique<bdr_db>();
        h->impl = bdr::Database::open(directory, convert_options(options));
        *out_db = h.release();
    });
}

bdr_status bdr_close(bdr_db* db) {
    if (!db) return BDR_OK;
    bdr_status status = BDR_OK;
    try {
        g_last_error.clear();
        db->impl->close();
    } catch (const std::exception& e) {
        set_error(e);
        status = BDR_ERROR;
    } catch (...) {
        set_error("unknown non-standard exception during close");
        status = BDR_ERROR;
    }
    delete db;
    return status;
}

bdr_status bdr_put(bdr_db* db,
                   const void* key, size_t key_size,
                   const void* value, size_t value_size,
                   uint64_t* out_ticket) {
    if (!db || !out_ticket) {
        set_error("db and out_ticket are required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] {
        auto t = db->impl->put(bytes_to_string(key, key_size), bytes_to_string(value, value_size));
        *out_ticket = t.sequence;
    });
}

bdr_status bdr_put_sync(bdr_db* db,
                        const void* key, size_t key_size,
                        const void* value, size_t value_size) {
    if (!db) {
        set_error("db is required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] {
        db->impl->put_sync(bytes_to_string(key, key_size), bytes_to_string(value, value_size));
    });
}

bdr_status bdr_delete(bdr_db* db,
                      const void* key, size_t key_size,
                      uint64_t* out_ticket) {
    if (!db || !out_ticket) {
        set_error("db and out_ticket are required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] {
        auto t = db->impl->erase(bytes_to_string(key, key_size));
        *out_ticket = t.sequence;
    });
}

bdr_status bdr_delete_sync(bdr_db* db,
                           const void* key, size_t key_size) {
    if (!db) {
        set_error("db is required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] {
        db->impl->erase_sync(bytes_to_string(key, key_size));
    });
}

bdr_status bdr_get(bdr_db* db,
                   const void* key, size_t key_size,
                   void* out_value, size_t out_capacity,
                   size_t* out_value_size) {
    if (!db || !out_value_size) {
        set_error("db and out_value_size are required");
        return BDR_INVALID_ARGUMENT;
    }
    try {
        g_last_error.clear();
        auto value = db->impl->get(bytes_to_string(key, key_size));
        if (!value) {
            *out_value_size = 0;
            return BDR_NOT_FOUND;
        }
        *out_value_size = value->size();
        if (!out_value || out_capacity < value->size()) return BDR_BUFFER_TOO_SMALL;
        if (!value->empty()) std::memcpy(out_value, value->data(), value->size());
        return BDR_OK;
    } catch (const std::invalid_argument& e) {
        set_error(e);
        return BDR_INVALID_ARGUMENT;
    } catch (const std::exception& e) {
        set_error(e);
        return BDR_ERROR;
    } catch (...) {
        set_error("unknown non-standard exception");
        return BDR_ERROR;
    }
}

bdr_status bdr_wait(bdr_db* db, uint64_t ticket) {
    if (!db) {
        set_error("db is required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] { db->impl->wait(bdr::Ticket{ticket}); });
}

bdr_status bdr_sync(bdr_db* db) {
    if (!db) {
        set_error("db is required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] { db->impl->sync(); });
}

bdr_status bdr_checkpoint(bdr_db* db) {
    if (!db) {
        set_error("db is required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] { db->impl->checkpoint(); });
}

bdr_status bdr_size(bdr_db* db, size_t* out_size) {
    if (!db || !out_size) {
        set_error("db and out_size are required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] { *out_size = db->impl->size(); });
}

bdr_status bdr_last_sequence(bdr_db* db, uint64_t* out_sequence) {
    if (!db || !out_sequence) {
        set_error("db and out_sequence are required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] { *out_sequence = db->impl->last_sequence(); });
}

bdr_status bdr_durable_sequence(bdr_db* db, uint64_t* out_sequence) {
    if (!db || !out_sequence) {
        set_error("db and out_sequence are required");
        return BDR_INVALID_ARGUMENT;
    }
    return guard([&] { *out_sequence = db->impl->durable_sequence(); });
}

const char* bdr_last_error(void) {
    return g_last_error.c_str();
}

} // extern "C"
