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

/* Compute the effective shift value (mod 256). */
static uint8_t bsc_effective_shift(uint8_t yy, uint32_t rounds)
{
    /* Only the low 8 bits matter for the mod-256 result. */
    uint8_t rounds_mod = (uint8_t)rounds;

    /* Multiply in 16 bits, then truncate to 8 bits (mod 256). */
    return (uint8_t)((uint16_t)yy * (uint16_t)rounds_mod);
}

/* Compute padding length to reach a multiple of block_size. */
static uint8_t bsc_pad_len(size_t in_len, size_t block_size)
{
    /* Remainder is how many bytes are already in the last block. */
    size_t rem = in_len % block_size;
    if (rem == 0u) {
        /* Already aligned: no padding needed. */
        return 0u;
    }
    /* Pad with the bytes needed to complete the block. */
    return (uint8_t)(block_size - rem);
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
    /* Determine how many zero bytes must be added to align to block_size. */
    uint8_t pad_len = bsc_pad_len(in_len, block_size);

    /* Guard against size_t overflow when adding padding. */
    if (in_len > SIZE_MAX - (size_t)pad_len) {
        return BSC_ERR_BAD_PARAM;
    }

    size_t padded_len = in_len + (size_t)pad_len;

    /* Guard against size_t overflow when adding the pad length byte. */
    if (padded_len > SIZE_MAX - 1u) {
        return BSC_ERR_BAD_PARAM;
    }

    /* Total output = padded payload + 1 byte for pad_len. */
    size_t proc_len = padded_len + 1u;

    /* Output buffer must be large enough. */
    if (out_len < proc_len) {
        return BSC_ERR_OUT_TOO_SMALL;
    }

    /* Special case: empty input yields only the pad length byte. */
    if (padded_len == 0u) {
        out[0] = 0u;
        if (out_written) {
            *out_written = 1u;
        }
        return BSC_OK;
    }

    uint32_t rounds_eff = bsc_rounds_eff(rounds);
    /* Number of full blocks after padding. */
    size_t block_count = padded_len / block_size;

    /* Process each block with its corresponding YY-based shift. */
    for (size_t block = 0u; block < block_count; ++block) {
        /* YY is chosen cyclically from the key. */
        uint8_t yy = key[1u + (block % shifts_len)];
        uint8_t shift = bsc_effective_shift(yy, rounds_eff);
        /* Base index of the current block. */
        size_t base = block * block_size;

        /* Encode each byte in the block, padding missing input with 0. */
        for (size_t j = 0u; j < block_size; ++j) {
            size_t idx = base + j;
            /* Use 0 for padding beyond input length. */
            uint8_t src = (idx < in_len) ? in[idx] : 0u;
            out[idx] = (uint8_t)(src + shift);
        }
    }

    /* Store padding length as the final byte. */
    out[padded_len] = pad_len;

    if (out_written) {
        *out_written = proc_len;
    }

    return BSC_OK;
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

    /* Need at least one byte to read pad_len. */
    if (in_len < 1u) {
        return BSC_ERR_BAD_PARAM;
    }

    size_t block_size = (size_t)block_size_u8;

    /* Last byte stores pad length; payload is everything before it. */
    uint8_t pad_len = in[in_len - 1u];
    size_t payload_len = in_len - 1u;

    /* Encoded payload must be a multiple of block_size. */
    if ((payload_len % block_size) != 0u) {
        return BSC_ERR_LEN_MISMATCH;
    }
    /* pad_len must be less than block size and not exceed payload_len. */
    if (pad_len >= block_size_u8 || (size_t)pad_len > payload_len) {
        return BSC_ERR_BAD_PADDING;
    }

    /* Real length excludes the trailing padding bytes. */
    size_t real_len = payload_len - (size_t)pad_len;

    /* Output buffer must fit the real length. */
    if (out_len < real_len) {
        return BSC_ERR_OUT_TOO_SMALL;
    }

    /* Special case: only pad_len byte, no payload to decode. */
    if (payload_len == 0u) {
        if (out_written) {
            *out_written = 0u;
        }
        return BSC_OK;
    }

    uint32_t rounds_eff = bsc_rounds_eff(rounds);
    /* Number of full blocks in the payload. */
    size_t block_count = payload_len / block_size;

    /* Process each block with the same YY-based shift as encoding. */
    for (size_t block = 0u; block < block_count; ++block) {
        /* YY is chosen cyclically from the key. */
        uint8_t yy = key[1u + (block % shifts_len)];
        uint8_t shift = bsc_effective_shift(yy, rounds_eff);
        /* Base index of the current block. */
        size_t base = block * block_size;

        /* Decode each byte; skip bytes that are purely padding. */
        for (size_t j = 0u; j < block_size; ++j) {
            size_t idx = base + j;
            uint8_t value = (uint8_t)(in[idx] - shift);
            if (idx < real_len) {
                out[idx] = value;
            }
        }
    }

    if (out_written) {
        *out_written = real_len;
    }

    return BSC_OK;
}