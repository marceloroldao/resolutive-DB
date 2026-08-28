#ifndef BDR_ATOMIC_C_API_H
#define BDR_ATOMIC_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BDR_ATOMIC_C_ABI_VERSION 1u

typedef struct bdr_atomic_c_handle bdr_atomic_c_handle;

typedef enum bdr_atomic_c_status {
    BDR_ATOMIC_C_OK = 0,
    BDR_ATOMIC_C_INVALID_ARGUMENT = 1,
    BDR_ATOMIC_C_NOT_FOUND = 2,
    BDR_ATOMIC_C_IO_ERROR = 3,
    BDR_ATOMIC_C_INTERNAL_ERROR = 4
} bdr_atomic_c_status;

typedef enum bdr_atomic_c_operation_type {
    BDR_ATOMIC_C_PUT = 1,
    BDR_ATOMIC_C_DELETE = 2
} bdr_atomic_c_operation_type;

typedef struct bdr_atomic_c_operation {
    bdr_atomic_c_operation_type type;
    const void *key;
    size_t key_size;
    const void *value;
    size_t value_size;
} bdr_atomic_c_operation;

typedef struct bdr_atomic_c_buffer {
    uint8_t *data;
    size_t size;
} bdr_atomic_c_buffer;

typedef struct bdr_atomic_c_batch_result {
    uint64_t sequence;
    size_t operations;
    int durable;
} bdr_atomic_c_batch_result;

uint32_t bdr_atomic_c_abi_version(void);
bdr_atomic_c_status bdr_atomic_c_open(const char *directory, bdr_atomic_c_handle **out_handle);
bdr_atomic_c_status bdr_atomic_c_write_batch(
    bdr_atomic_c_handle *handle,
    const bdr_atomic_c_operation *operations,
    size_t operation_count,
    bdr_atomic_c_batch_result *out_result);
bdr_atomic_c_status bdr_atomic_c_get(
    bdr_atomic_c_handle *handle,
    const void *key,
    size_t key_size,
    bdr_atomic_c_buffer *out_value);
bdr_atomic_c_status bdr_atomic_c_exists(
    bdr_atomic_c_handle *handle,
    const void *key,
    size_t key_size,
    int *out_exists);
bdr_atomic_c_status bdr_atomic_c_sync(bdr_atomic_c_handle *handle);
bdr_atomic_c_status bdr_atomic_c_last_sequence(bdr_atomic_c_handle *handle, uint64_t *out_sequence);
bdr_atomic_c_status bdr_atomic_c_durable_sequence(bdr_atomic_c_handle *handle, uint64_t *out_sequence);
bdr_atomic_c_status bdr_atomic_c_integrity_check(bdr_atomic_c_handle *handle);
void bdr_atomic_c_free_buffer(bdr_atomic_c_buffer buffer);
void bdr_atomic_c_close(bdr_atomic_c_handle *handle);

#ifdef __cplusplus
}
#endif

#endif
