#ifndef BYTE_SHIFT_CODEC_H
#define BYTE_SHIFT_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTE_SHIFT_DEFAULT_ROUNDS 10u

typedef enum {
    BSC_OK = 0,
    BSC_ERR_BAD_KEY = -1,
    BSC_ERR_BAD_PARAM = -2,
    BSC_ERR_OUT_TOO_SMALL = -3,
    BSC_ERR_LEN_MISMATCH = -4,
    BSC_ERR_BAD_PADDING = -5
} bsc_status_t;

int bsc_encode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written);

int bsc_decode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written);

#ifdef __cplusplus
}
#endif

#endif
