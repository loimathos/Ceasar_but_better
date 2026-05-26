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

/**
 * @brief Encode data using a byte-shift block scheme.
 *
 * Key format:
 *   key[0] = XX (block size, 1..255)
 *   key[1..] = YY sequence (per-block shift base values)
 *
 * Encoding rules:
 * - Input is padded with zero bytes to a multiple of XX.
 * - One extra byte is appended at the end with pad_len.
 * - Each block uses shift = (YY * rounds) mod 256.
 *
 * @param in Pointer to input data (non-NULL).
 * @param in_len Length of input data in bytes.
 * @param key Encoding key (non-NULL).
 * @param key_len Length of the key in bytes (>= 2).
 * @param rounds Number of rounds (BYTE_SHIFT_DEFAULT_ROUNDS if 0).
 * @param out Output buffer (non-NULL, allocated by caller).
 * @param out_len Maximum size of output buffer in bytes.
 * @param out_written Receives number of bytes written (0 on error).
 * @return int Status code (BSC_OK on success).
 *
 * Required output size:
 *   pad_len = (XX - (in_len % XX)) % XX
 *   out_len >= in_len + pad_len + 1
 */
int bsc_encode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written);

/**
 * @brief Decode data produced by bsc_encode.
 *
 * Decoding rules:
 * - The last byte of input is pad_len.
 * - The payload length (in_len - 1) must be a multiple of XX.
 * - Output length is payload_len - pad_len.
 * - rounds should match the value used for encoding (0 uses default).
 *
 * @param in Pointer to encoded input data (non-NULL).
 * @param in_len Length of encoded input in bytes (>= 1).
 * @param key Decoding key (non-NULL).
 * @param key_len Length of the key in bytes (>= 2).
 * @param rounds Number of rounds (BYTE_SHIFT_DEFAULT_ROUNDS if 0).
 * @param out Output buffer (non-NULL, allocated by caller).
 * @param out_len Maximum size of output buffer in bytes.
 * @param out_written Receives number of bytes written (0 on error).
 * @return int Status code (BSC_OK on success).
 *
 * Required output size:
 *   pad_len = in[in_len - 1]
 *   out_len >= (in_len - 1) - pad_len
 */
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

#endif /* BYTE_SHIFT_CODEC_H */