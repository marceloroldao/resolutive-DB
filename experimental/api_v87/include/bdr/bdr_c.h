#ifndef BDR_C_H
#define BDR_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BDR_C_ABI_VERSION 1u

typedef struct bdr_handle bdr_handle;
typedef uint64_t bdr_ticket;

typedef enum bdr_status {
    BDR_OK = 0,
    BDR_NOT_FOUND = 1,
    BDR_INVALID_ARGUMENT = 2,
    BDR_ALREADY_OPEN = 3,
    BDR_IO_ERROR = 4,
    BDR_CLOSED = 5,
    BDR_INCOMPATIBLE_ABI = 6,
    BDR_INTERNAL_ERROR = 255
} bdr_status;

typedef struct bdr_options {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t reserve_bytes;
    uint64_t wal_batch;
    uint64_t partition_count;
    double partition_max_load;
    uint8_t keep_size_preallocation;
    uint8_t reserved[31];
} bdr_options;

void bdr_options_init(bdr_options* options);
uint32_t bdr_abi_version(void);

bdr_status bdr_open(const char* directory, const bdr_options* options, bdr_handle** out_handle);
bdr_status bdr_close(bdr_handle* handle);

bdr_status bdr_put(bdr_handle* handle,
                   const void* key, size_t key_len,
                   const void* value, size_t value_len,
                   bdr_ticket* out_ticket);

bdr_status bdr_put_sync(bdr_handle* handle,
                        const void* key, size_t key_len,
                        const void* value, size_t value_len);

bdr_status bdr_delete(bdr_handle* handle,
                      const void* key, size_t key_len,
                      bdr_ticket* out_ticket);

bdr_status bdr_delete_sync(bdr_handle* handle,
                           const void* key, size_t key_len);

/* Two-call GET: pass out_value=NULL to query required size in *inout_value_len. */
bdr_status bdr_get(bdr_handle* handle,
                   const void* key, size_t key_len,
                   void* out_value, size_t* inout_value_len);

bdr_status bdr_wait(bdr_handle* handle, bdr_ticket ticket);
bdr_status bdr_sync(bdr_handle* handle);
bdr_status bdr_checkpoint(bdr_handle* handle);

uint64_t bdr_last_sequence(const bdr_handle* handle);
uint64_t bdr_durable_sequence(const bdr_handle* handle);
uint64_t bdr_size(const bdr_handle* handle);

/* Thread-local diagnostic for the most recent failing C ABI call. */
const char* bdr_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
