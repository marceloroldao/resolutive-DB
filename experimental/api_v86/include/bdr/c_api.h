#ifndef BDR_C_API_H
#define BDR_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bdr_c_handle bdr_c_handle;

typedef enum bdr_c_status {
    BDR_C_OK = 0,
    BDR_C_INVALID_ARGUMENT = 1,
    BDR_C_NOT_FOUND = 2,
    BDR_C_IO_ERROR = 3,
    BDR_C_INTERNAL_ERROR = 4
} bdr_c_status;

typedef struct bdr_c_buffer {
    uint8_t *data;
    size_t size;
} bdr_c_buffer;

bdr_c_status bdr_c_open(const char *directory, bdr_c_handle **out_handle);
bdr_c_status bdr_c_put(bdr_c_handle *handle, const void *key, size_t key_size, const void *value, size_t value_size);
bdr_c_status bdr_c_get(bdr_c_handle *handle, const void *key, size_t key_size, bdr_c_buffer *out_value);
bdr_c_status bdr_c_delete(bdr_c_handle *handle, const void *key, size_t key_size);
bdr_c_status bdr_c_sync(bdr_c_handle *handle);
bdr_c_status bdr_c_checkpoint(bdr_c_handle *handle);
void bdr_c_free_buffer(bdr_c_buffer buffer);
void bdr_c_close(bdr_c_handle *handle);

#ifdef __cplusplus
}
#endif

#endif
