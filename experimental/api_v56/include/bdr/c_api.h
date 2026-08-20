#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bdr_db bdr_db;

typedef enum bdr_status {
    BDR_OK = 0,
    BDR_INVALID_ARGUMENT = 1,
    BDR_NOT_FOUND = 2,
    BDR_BUFFER_TOO_SMALL = 3,
    BDR_ERROR = 4
} bdr_status;

typedef struct bdr_options {
    size_t reserve_bytes;
    size_t wal_batch;
    size_t partition_count;
    double partition_max_load;
    int keep_size_preallocation;
} bdr_options;

bdr_options bdr_default_options(void);

bdr_status bdr_open(const char* directory, const bdr_options* options, bdr_db** out_db);
bdr_status bdr_close(bdr_db* db);

bdr_status bdr_put(bdr_db* db,
                   const void* key, size_t key_size,
                   const void* value, size_t value_size,
                   uint64_t* out_ticket);
bdr_status bdr_put_sync(bdr_db* db,
                        const void* key, size_t key_size,
                        const void* value, size_t value_size);
bdr_status bdr_delete(bdr_db* db,
                      const void* key, size_t key_size,
                      uint64_t* out_ticket);
bdr_status bdr_delete_sync(bdr_db* db,
                           const void* key, size_t key_size);

bdr_status bdr_get(bdr_db* db,
                   const void* key, size_t key_size,
                   void* out_value, size_t out_capacity,
                   size_t* out_value_size);

bdr_status bdr_wait(bdr_db* db, uint64_t ticket);
bdr_status bdr_sync(bdr_db* db);
bdr_status bdr_checkpoint(bdr_db* db);

bdr_status bdr_size(bdr_db* db, size_t* out_size);
bdr_status bdr_last_sequence(bdr_db* db, uint64_t* out_sequence);
bdr_status bdr_durable_sequence(bdr_db* db, uint64_t* out_sequence);

const char* bdr_last_error(void);

#ifdef __cplusplus
}
#endif
