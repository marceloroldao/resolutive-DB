#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BDR_C_ABI_VERSION 1u

typedef struct bdr_c_database bdr_c_database;

typedef enum bdr_c_status {
    BDR_C_OK = 0,
    BDR_C_INVALID_ARGUMENT = 1,
    BDR_C_NOT_FOUND = 2,
    BDR_C_BUFFER_TOO_SMALL = 3,
    BDR_C_IO_ERROR = 4,
    BDR_C_INTERNAL_ERROR = 5
} bdr_c_status;

typedef enum bdr_c_durability {
    BDR_C_ASYNC = 0,
    BDR_C_BATCH_SYNC = 1,
    BDR_C_PER_OPERATION_SYNC = 2
} bdr_c_durability;

typedef struct bdr_c_pair {
    const void* key;
    size_t key_size;
    const void* value;
    size_t value_size;
} bdr_c_pair;

uint32_t bdr_c_abi_version(void);
const char* bdr_c_last_error(void);

bdr_c_status bdr_c_open(
    const char* directory,
    const char* bdw4_path,
    bdr_c_database** out_db);

void bdr_c_close(bdr_c_database* db);

bdr_c_status bdr_c_put(
    bdr_c_database* db,
    const void* key,
    size_t key_size,
    const void* value,
    size_t value_size,
    bdr_c_durability durability);

bdr_c_status bdr_c_put_many(
    bdr_c_database* db,
    const bdr_c_pair* entries,
    size_t entry_count,
    bdr_c_durability durability);

bdr_c_status bdr_c_get(
    bdr_c_database* db,
    const void* key,
    size_t key_size,
    void* out_value,
    size_t out_capacity,
    size_t* out_size);

bdr_c_status bdr_c_contains(
    bdr_c_database* db,
    const void* key,
    size_t key_size,
    int* out_contains);

bdr_c_status bdr_c_sync(bdr_c_database* db);

uint64_t bdr_c_last_sequence(const bdr_c_database* db);
uint64_t bdr_c_durable_sequence(const bdr_c_database* db);
size_t bdr_c_size(const bdr_c_database* db);

#ifdef __cplusplus
}
#endif
