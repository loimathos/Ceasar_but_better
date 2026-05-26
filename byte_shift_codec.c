#include "byte_shift_codec.h"

#include <stdint.h>

/* Validate key layout and extract XX (block size) and YY sequence length. */
static int bsc_validate_key(const uint8_t *key, size_t key_len, uint8_t *xx, size_t *yy_len)
{
    /* Require non-null pointers and at least two bytes: XX + one YY. */
    if (!key || key_len < 2u || !xx || !yy_len) {
        return BSC_ERR_BAD_KEY;
    }

    /* First byte is XX (block size). It must be non-zero. */
    *xx = key[0];
    if (*xx == 0u) {
        return BSC_ERR_BAD_KEY;
    }

    /* Remaining bytes are the YY sequence. Must be non-empty. */
    *yy_len = key_len - 1u;
    if (*yy_len == 0u) {
        return BSC_ERR_BAD_KEY;
    }

    return BSC_OK;
}

/* Determine the effective rounds value (0 means default). */
static uint32_t bsc_rounds_eff(uint32_t rounds)
{
    return (rounds == 0u) ? BYTE_SHIFT_DEFAULT_ROUNDS : rounds;
}

/* Clamp rounds mod 256 to avoid a zero shift when rounds is a multiple of 256. */
static uint8_t bsc_rounds_mod(uint32_t rounds_eff, int *clamped)
{
    if (clamped) {
        *clamped = 0;
    }
    uint8_t rounds_mod = (uint8_t)rounds_eff;
    if (rounds_mod == 0u) {
        rounds_mod = 1u;
        if (clamped) {
            *clamped = 1;
        }
    }
    return rounds_mod;
}

/* Compute the effective shift value (mod 256). */
static uint8_t bsc_effective_shift(uint8_t yy, uint8_t rounds_mod)
{
    /* Multiply in 16 bits, then truncate to 8 bits (mod 256). */
    return (uint8_t)((uint16_t)yy * (uint16_t)rounds_mod);
}

int bsc_encode(const uint8_t *in, size_t in_len, const uint8_t *key, size_t key_len,
               uint32_t rounds, uint8_t *out, size_t out_len, size_t *out_written)
{
    uint8_t block_size_u8 = 0u;
    size_t shifts_len = 0u;

    if (out_written) {
        /* Default to 0 on error paths. */
        *out_written = 0u;
    }

    /* Basic null pointer validation. */
    if (!in || !out) {
        return BSC_ERR_BAD_PARAM;
    }

    /* Validate key and extract block size and YY sequence length. */
    if (bsc_validate_key(key, key_len, &block_size_u8, &shifts_len) != BSC_OK) {
        return BSC_ERR_BAD_KEY;
    }

    /* Defensive re-check after extraction. */
    if (block_size_u8 == 0u || shifts_len == 0u) {
        return BSC_ERR_BAD_KEY;
    }

    size_t block_size = (size_t)block_size_u8;

    /* Output buffer must be large enough. */
    if (out_len < in_len) {
        return BSC_ERR_OUT_TOO_SMALL;
    }

    uint32_t rounds_eff = bsc_rounds_eff(rounds);
    int rounds_clamped = 0;
    uint8_t rounds_mod = bsc_rounds_mod(rounds_eff, &rounds_clamped);

    /* Number of blocks, last block may be partial. */
    size_t block_count = in_len / block_size;
    if ((in_len % block_size) != 0u) {
        ++block_count;
    }

    /* Process each block with its corresponding YY-based shift. */
    for (size_t block = 0u; block < block_count; ++block) {
        /* YY is chosen cyclically from the key. */
        uint8_t yy = key[1u + (block % shifts_len)];
        uint8_t shift = bsc_effective_shift(yy, rounds_mod);
        /* Base index of the current block. */
        size_t base = block * block_size;

        size_t remaining = (in_len > base) ? (in_len - base) : 0u;
        size_t bytes_in_block = (remaining < block_size) ? remaining : block_size;

        /* Encode each byte in the block; last block may be partial. */
        for (size_t j = 0u; j < bytes_in_block; ++j) {
            size_t idx = base + j;
            out[idx] = (uint8_t)(in[idx] + shift);
        }
    }

    if (out_written) {
        *out_written = in_len;
    }

    return rounds_clamped ? BSC_WARN_ROUNDS_CLAMPED : BSC_OK;
}

int bsc_decode(const uint8_t *in, size_t in_len, const uint8_t *key, size_t key_len,
               uint32_t rounds, uint8_t *out, size_t out_len, size_t *out_written)
{
    uint8_t block_size_u8 = 0u;
    size_t shifts_len = 0u;

    if (out_written) {
        /* Default to 0 on error paths. */
        *out_written = 0u;
    }

    /* Basic null pointer validation. */
    if (!in || !out) {
        return BSC_ERR_BAD_PARAM;
    }

    /* Validate key and extract block size and YY sequence length. */
    if (bsc_validate_key(key, key_len, &block_size_u8, &shifts_len) != BSC_OK) {
        return BSC_ERR_BAD_KEY;
    }

    /* Defensive re-check after extraction. */
    if (block_size_u8 == 0u || shifts_len == 0u) {
        return BSC_ERR_BAD_KEY;
    }

    size_t block_size = (size_t)block_size_u8;

    /* Output buffer must fit the input length. */
    if (out_len < in_len) {
        return BSC_ERR_OUT_TOO_SMALL;
    }

    uint32_t rounds_eff = bsc_rounds_eff(rounds);
    int rounds_clamped = 0;
    uint8_t rounds_mod = bsc_rounds_mod(rounds_eff, &rounds_clamped);

    /* Number of blocks, last block may be partial. */
    size_t block_count = in_len / block_size;
    if ((in_len % block_size) != 0u) {
        ++block_count;
    }

    /* Process each block with the same YY-based shift as encoding. */
    for (size_t block = 0u; block < block_count; ++block) {
        /* YY is chosen cyclically from the key. */
        uint8_t yy = key[1u + (block % shifts_len)];
        uint8_t shift = bsc_effective_shift(yy, rounds_mod);
        /* Base index of the current block. */
        size_t base = block * block_size;

        size_t remaining = in_len - base;
        size_t bytes_in_block = (remaining < block_size) ? remaining : block_size;

        /* Decode each byte in the block; last block may be partial. */
        for (size_t j = 0u; j < bytes_in_block; ++j) {
            size_t idx = base + j;
            out[idx] = (uint8_t)(in[idx] - shift);
        }
    }

    if (out_written) {
        *out_written = in_len;
    }

    return rounds_clamped ? BSC_WARN_ROUNDS_CLAMPED : BSC_OK;
}