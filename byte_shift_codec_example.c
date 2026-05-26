#include <stdio.h>
#include <stdint.h>

#include "byte_shift_codec.h"

static void print_hex(const char *label, const uint8_t *buf, size_t len)
{
    size_t i = 0u;

    printf("%s (%zu bytes): ", label, len);
    for (i = 0u; i < len; ++i) {
        printf("%02X", buf[i]);
        if (i + 1u < len) {
            printf(" ");
        }
    }
    printf("\n");
}

int main(void)
{
    /* Example input payload (13 bytes, not a multiple of block size). */
    uint8_t input[] = {
        0x00u, 0x11u, 0x22u, 0x33u,
        0x44u, 0x55u, 0x66u, 0x77u,
        0x88u, 0x99u, 0xAAu, 0xBBu,
        0xCCu
    };
    /* Key format: key[0] = block size (XX), key[1..] = YY sequence. */
    uint8_t key[] = { 0x04u, 0x05u, 0xF0u };
    /* Output buffers must be large enough for padding + pad_len byte. */
    uint8_t encoded[64] = { 0u };
    uint8_t decoded[64] = { 0u };
    size_t written = 0u;
    int rc = 0;

    /* Encode with rounds = 3 (0 would use BYTE_SHIFT_DEFAULT_ROUNDS). */
    rc = bsc_encode(input, sizeof(input), key, sizeof(key), 3u,
                    encoded, sizeof(encoded), &written);
    if (rc < 0) {
        printf("encode failed: %d\n", rc);
        return 1;
    }
    if (rc == BSC_WARN_ROUNDS_CLAMPED) {
        printf("encode warning: rounds clamped to avoid zero shift\n");
    }
    /* A valid encoding always writes at least the pad_len suffix byte. */
    if (written == 0u) {
        printf("encode produced no output\n");
        return 1;
    }

    print_hex("input", input, sizeof(input));
    /* Encoded output = padded payload + 1 byte pad_len suffix. */
    print_hex("encoded (payload + pad_len)", encoded, written);
    printf("pad_len (suffix) = %u\n", encoded[written - 1u]);

    /* Decode with the same key and rounds used for encoding. */
    rc = bsc_decode(encoded, written, key, sizeof(key), 3u,
                    decoded, sizeof(decoded), &written);
    if (rc < 0) {
        printf("decode failed: %d\n", rc);
        return 1;
    }
    if (rc == BSC_WARN_ROUNDS_CLAMPED) {
        printf("decode warning: rounds clamped to avoid zero shift\n");
    }

    /* The decoded output should match the original input bytes. */
    print_hex("decoded", decoded, written);
    return 0;
}
