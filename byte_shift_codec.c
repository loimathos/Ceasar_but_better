#include "byte_shift_codec.h"

static int bsc_validate_key(const uint8_t *key,
                            size_t key_len,
                            uint8_t *xx,
                            size_t *yy_len)
{
    if (!key || key_len < 2u || !xx || !yy_len) {
        return BSC_ERR_BAD_KEY;
    }

    *xx = key[0];
    if (*xx == 0u) {
        return BSC_ERR_BAD_KEY;
    }

    *yy_len = key_len - 1u;
    if (*yy_len == 0u) {
        return BSC_ERR_BAD_KEY;
    }

    return BSC_OK;
}

static uint8_t bsc_effective_shift(uint8_t yy, uint32_t rounds)
{
    return (uint8_t)((uint32_t)yy * rounds);
}

static uint8_t bsc_pad_len(size_t in_len, uint8_t xx)
{
    size_t rem = in_len % (size_t)xx;
    if (rem == 0u) {
        return 0u;
    }
    return (uint8_t)((size_t)xx - rem);
}

int bsc_encode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written)
{
    uint8_t xx = 0u;
    size_t yy_len = 0u;
    size_t proc_len = 0u;
    size_t padded_len = 0u;
    uint8_t pad_len = 0u;
    uint32_t rounds_eff = rounds == 0u ? BYTE_SHIFT_DEFAULT_ROUNDS : rounds;
    size_t block_count = 0u;
    size_t block = 0u;

    if (!in || !out) {
        return BSC_ERR_BAD_PARAM;
    }

    if (bsc_validate_key(key, key_len, &xx, &yy_len) != BSC_OK) {
        return BSC_ERR_BAD_KEY;
    }

    if ((in_len % (size_t)xx) != 0u) {
        return BSC_ERR_LEN_MISMATCH;
    }

    pad_len = bsc_pad_len(in_len, xx);
    padded_len = in_len + (size_t)pad_len;
    proc_len = padded_len + 1u;
    if (out_len < proc_len) {
        return BSC_ERR_OUT_TOO_SMALL;
    }

    if (padded_len == 0u) {
        out[0] = 0u;
        if (out_written) {
            *out_written = 1u;
        }
        return BSC_OK;
    }

    block_count = padded_len / (size_t)xx;

    for (block = 0u; block < block_count; ++block) {
        uint8_t yy = key[1u + (block % yy_len)];
        uint8_t shift = bsc_effective_shift(yy, rounds_eff);
        size_t base = block * (size_t)xx;
        size_t j = 0u;

        for (j = 0u; j < (size_t)xx; ++j) {
            size_t idx = base + j;
            uint8_t src = 0u;
            if (idx < in_len) {
                src = in[idx];
            }
            out[idx] = (uint8_t)(src + shift);
        }
    }

    out[padded_len] = pad_len;

    if (out_written) {
        *out_written = padded_len + 1u;
    }

    return BSC_OK;
}

int bsc_decode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written)
{
    uint8_t xx = 0u;
    size_t yy_len = 0u;
    size_t proc_len = 0u;
    size_t payload_len = 0u;
    size_t real_len = 0u;
    uint8_t pad_len = 0u;
    uint32_t rounds_eff = rounds == 0u ? BYTE_SHIFT_DEFAULT_ROUNDS : rounds;
    size_t block_count = 0u;
    size_t block = 0u;

    if (!in || !out) {
        return BSC_ERR_BAD_PARAM;
    }

    if (bsc_validate_key(key, key_len, &xx, &yy_len) != BSC_OK) {
        return BSC_ERR_BAD_KEY;
    }

    if ((in_len % (size_t)xx) != 0u) {
        return BSC_ERR_LEN_MISMATCH;
    }

    if (in_len < 1u) {
        return BSC_ERR_BAD_PARAM;
    }

    pad_len = in[in_len - 1u];
    payload_len = in_len - 1u;
    if ((payload_len % (size_t)xx) != 0u) {
        return BSC_ERR_LEN_MISMATCH;
    }
    if (pad_len >= xx) {
        return BSC_ERR_BAD_PADDING;
    }
    if ((size_t)pad_len > payload_len) {
        return BSC_ERR_BAD_PADDING;
    }

    real_len = payload_len - (size_t)pad_len;
    proc_len = payload_len;
    if (out_len < real_len) {
        return BSC_ERR_OUT_TOO_SMALL;
    }

    if (proc_len == 0u) {
        if (out_written) {
            *out_written = 0u;
        }
        return BSC_OK;
    }

    block_count = proc_len / (size_t)xx;

    for (block = 0u; block < block_count; ++block) {
        uint8_t yy = key[1u + (block % yy_len)];
        uint8_t shift = bsc_effective_shift(yy, rounds_eff);
        size_t base = block * (size_t)xx;
        size_t j = 0u;

        for (j = 0u; j < (size_t)xx; ++j) {
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
