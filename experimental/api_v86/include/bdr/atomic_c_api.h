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

typedef enum bdr_atomic_c_durability {
    BDR_ATOMIC_C_ASYNC = 0,
    BDR_ATOMIC_C_BATCH_SYNC = 1,
    BDR_ATOMIC_C_PER_OPERATION_SYNC = 2
} bdr_atomic_c_durability;

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

/* Experimental additive diagnostics. Not a persistence-format contract. */
typedef struct bdr_atomic_c_diagnostics {
    uint64_t wal_bytes;
    size_t replayed_batches;
    size_t replayed_operations;
    size_t legacy_records;
    size_t resident_records;
    uint64_t legacy_sequence;
    uint64_t last_sequence;
    uint64_t durable_sequence;
    int repaired_torn_tail;
    uint64_t legacy_load_us;
    uint64_t wal_read_us;
    uint64_t wal_decode_apply_us;
    uint64_t wal_replay_us;
} bdr_atomic_c_diagnostics;

uint32_t bdr_atomic_c_abi_version(void);
bdr_atomic_c_status bdr_atomic_c_open(const char *directory, bdr_atomic_c_handle **out_handle);
/* Backward-compatible v1 entry point: BatchSync remains the default. */
bdr_atomic_c_status bdr_atomic_c_write_batch(
    bdr_atomic_c_handle *handle,
    const bdr_atomic_c_operation *operations,
    size_t operation_count,
    bdr_atomic_c_batch_result *out_result);
/* Additive selectable-durability entry point; atomicity is unchanged. */
bdr_atomic_c_status bdr_atomic_c_write_batch_with_durability(
    bdr_atomic_c_handle *handle,
    const bdr_atomic_c_operation *operations,
    size_t operation_count,
    bdr_atomic_c_durability durability,
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
bdr_atomic_c_status bdr_atomic_c_diagnostics_get(
    bdr_atomic_c_handle *handle,
    bdr_atomic_c_diagnostics *out_diagnostics);
bdr_atomic_c_status bdr_atomic_c_integrity_check(bdr_atomic_c_handle *handle);
void bdr_atomic_c_free_buffer(bdr_atomic_c_buffer buffer);
void bdr_atomic_c_close(bdr_atomic_c_handle *handle);

#ifdef __cplusplus
}
#endif

#endif
