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
    uint8_t input[] = {
        0x00u, 0x11u, 0x22u, 0x33u,
        0x44u, 0x55u, 0x66u, 0x77u,
        0x88u, 0x99u, 0xAAu, 0xBBu,
        0xCCu
    };
    uint8_t key[] = { 0x04u, 0x05u, 0xF0u };
    uint8_t encoded[sizeof(input)] = { 0u };
    uint8_t decoded[sizeof(input)] = { 0u };
    size_t written = 0u;
    int rc = 0;

    rc = bsc_encode(input, sizeof(input), key, sizeof(key), 3u,
                    encoded, sizeof(encoded), &written);
    if (rc != BSC_OK) {
        printf("encode failed: %d\n", rc);
        return 1;
    }

    print_hex("input", input, sizeof(input));
    print_hex("encoded (payload + pad_len)", encoded, written);
    printf("pad_len (suffix) = %u\n", encoded[written - 1u]);

    rc = bsc_decode(encoded, written, key, sizeof(key), 3u,
                    decoded, sizeof(decoded), &written);
    if (rc != BSC_OK) {
        printf("decode failed: %d\n", rc);
        return 1;
    }

    print_hex("decoded", decoded, written);
    return 0;
}
