#include "bdr/bdr_c.h"
#include "bdr/database.hpp"

#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

struct bdr_handle {
    std::unique_ptr<bdr::Database> db;
    bool closed = false;
};

static thread_local std::string g_last_error;

static bdr_status classify_exception(const std::exception& e) {
    g_last_error = e.what();
    const std::string m = e.what();
    if (m.find("already open") != std::string::npos) return BDR_ALREADY_OPEN;
    if (m.find("closed") != std::string::npos) return BDR_CLOSED;
    if (m.find("invalid") != std::string::npos || m.find("must be") != std::string::npos)
        return BDR_INVALID_ARGUMENT;
    if (m.find("write") != std::string::npos || m.find("sync") != std::string::npos ||
        m.find("open failed") != std::string::npos || m.find("I/O") != std::string::npos)
        return BDR_IO_ERROR;
    return BDR_INTERNAL_ERROR;
}

static std::string bytes_to_string(const void* p, size_t n) {
    if (!p && n) throw std::invalid_argument("null byte pointer with non-zero length");
    return std::string(static_cast<const char*>(p), n);
}

static bdr::Options to_cpp_options(const bdr_options* in) {
    bdr::Options o;
    if (!in) return o;
    if (in->abi_version != BDR_C_ABI_VERSION || in->struct_size < sizeof(bdr_options))
        throw std::runtime_error("incompatible C ABI options struct");
    o.reserve_bytes = static_cast<std::size_t>(in->reserve_bytes);
    o.wal_batch = static_cast<std::size_t>(in->wal_batch);
    o.partition_count = static_cast<std::size_t>(in->partition_count);
    o.partition_max_load = in->partition_max_load;
    o.keep_size_preallocation = in->keep_size_preallocation != 0;
    return o;
}

extern "C" {

void bdr_options_init(bdr_options* o) {
    if (!o) return;
    std::memset(o, 0, sizeof(*o));
    o->abi_version = BDR_C_ABI_VERSION;
    o->struct_size = sizeof(*o);
    o->reserve_bytes = 64ull * 1024ull * 1024ull;
    o->wal_batch = 512;
    o->partition_count = 4096;
    o->partition_max_load = 0.78;
    o->keep_size_preallocation = 1;
}

uint32_t bdr_abi_version(void) { return BDR_C_ABI_VERSION; }

bdr_status bdr_open(const char* directory, const bdr_options* options, bdr_handle** out_handle) {
    g_last_error.clear();
    if (!directory || !*directory || !out_handle) return BDR_INVALID_ARGUMENT;
    *out_handle = nullptr;
    try {
        auto h = std::make_unique<bdr_handle>();
        h->db = bdr::Database::open(directory, to_cpp_options(options));
        *out_handle = h.release();
        return BDR_OK;
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()).find("incompatible C ABI") != std::string::npos) {
            g_last_error = e.what();
            return BDR_INCOMPATIBLE_ABI;
        }
        return classify_exception(e);
    } catch (const std::exception& e) {
        return classify_exception(e);
    } catch (...) {
        g_last_error = "unknown exception";
        return BDR_INTERNAL_ERROR;
    }
}

bdr_status bdr_close(bdr_handle* h) {
    g_last_error.clear();
    if (!h) return BDR_INVALID_ARGUMENT;
    if (h->closed) { delete h; return BDR_OK; }
    try {
        h->db->close();
        h->closed = true;
        delete h;
        return BDR_OK;
    } catch (const std::exception& e) {
        delete h;
        return classify_exception(e);
    }
}

bdr_status bdr_put(bdr_handle* h,const void* key,size_t key_len,const void* value,size_t value_len,bdr_ticket* out_ticket) {
    g_last_error.clear();
    if (!h || !out_ticket) return BDR_INVALID_ARGUMENT;
    try {
        auto t = h->db->put(bytes_to_string(key,key_len), bytes_to_string(value,value_len));
        *out_ticket = t.sequence;
        return BDR_OK;
    } catch (const std::exception& e) { return classify_exception(e); }
}

bdr_status bdr_put_sync(bdr_handle* h,const void* key,size_t key_len,const void* value,size_t value_len) {
    g_last_error.clear();
    if (!h) return BDR_INVALID_ARGUMENT;
    try { h->db->put_sync(bytes_to_string(key,key_len), bytes_to_string(value,value_len)); return BDR_OK; }
    catch (const std::exception& e) { return classify_exception(e); }
}

bdr_status bdr_delete(bdr_handle* h,const void* key,size_t key_len,bdr_ticket* out_ticket) {
    g_last_error.clear();
    if (!h || !out_ticket) return BDR_INVALID_ARGUMENT;
    try { auto t=h->db->erase(bytes_to_string(key,key_len));*out_ticket=t.sequence;return BDR_OK; }
    catch (const std::exception& e) { return classify_exception(e); }
}

bdr_status bdr_delete_sync(bdr_handle* h,const void* key,size_t key_len) {
    g_last_error.clear();
    if (!h) return BDR_INVALID_ARGUMENT;
    try { h->db->erase_sync(bytes_to_string(key,key_len));return BDR_OK; }
    catch (const std::exception& e) { return classify_exception(e); }
}

bdr_status bdr_get(bdr_handle* h,const void* key,size_t key_len,void* out_value,size_t* inout_value_len) {
    g_last_error.clear();
    if (!h || !inout_value_len) return BDR_INVALID_ARGUMENT;
    try {
        auto v = h->db->get(bytes_to_string(key,key_len));
        if (!v) return BDR_NOT_FOUND;
        if (!out_value) { *inout_value_len = v->size(); return BDR_OK; }
        if (*inout_value_len < v->size()) { *inout_value_len = v->size(); return BDR_INVALID_ARGUMENT; }
        if (!v->empty()) std::memcpy(out_value, v->data(), v->size());
        *inout_value_len = v->size();
        return BDR_OK;
    } catch (const std::exception& e) { return classify_exception(e); }
}

bdr_status bdr_wait(bdr_handle* h,bdr_ticket ticket) {
    g_last_error.clear();if(!h)return BDR_INVALID_ARGUMENT;
    try { h->db->wait(bdr::Ticket{ticket});return BDR_OK; } catch(const std::exception&e){return classify_exception(e);} }

bdr_status bdr_sync(bdr_handle* h) {
    g_last_error.clear();if(!h)return BDR_INVALID_ARGUMENT;
    try { h->db->sync();return BDR_OK; } catch(const std::exception&e){return classify_exception(e);} }

bdr_status bdr_checkpoint(bdr_handle* h) {
    g_last_error.clear();if(!h)return BDR_INVALID_ARGUMENT;
    try { h->db->checkpoint();return BDR_OK; } catch(const std::exception&e){return classify_exception(e);} }

uint64_t bdr_last_sequence(const bdr_handle* h) { return h && h->db ? h->db->last_sequence() : 0; }
uint64_t bdr_durable_sequence(const bdr_handle* h) { return h && h->db ? h->db->durable_sequence() : 0; }
uint64_t bdr_size(const bdr_handle* h) { return h && h->db ? static_cast<uint64_t>(h->db->size()) : 0; }
const char* bdr_last_error(void) { return g_last_error.c_str(); }

} // extern C
