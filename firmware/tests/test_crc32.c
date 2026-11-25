/*
 * test_crc32.c - the software CRC-32 matches zlib (D35).
 */
#include "test.h"
#include "crc32.h"

#include <string.h>

int main(void)
{
    /* The standard check value: catches a wrong polynomial, init, bit order
     * or final XOR immediately. */
    const char *check = "123456789";
    CHECK_EQ_U32(crc32_update(0, check, strlen(check)), CRC32_CHECK_VALUE);

    /* Empty input leaves the CRC unchanged (zlib: crc32(0, "", 0) == 0). */
    CHECK_EQ_U32(crc32_update(0, "", 0), 0u);

    /* Known zlib values. */
    CHECK_EQ_U32(crc32_update(0, "a", 1), 0xE8B7BE43u);
    CHECK_EQ_U32(crc32_update(0, "The quick brown fox jumps over the lazy dog", 43), 0x414FA339u);

    /* Feeding in chunks gives the same result as all at once, for every split.
     * The capture frame relies on this: header and payload are fed separately. */
    uint8_t buf[256];
    for (unsigned i = 0; i < sizeof buf; i++) { buf[i] = (uint8_t)(i * 37u + 11u); }
    uint32_t whole = crc32_update(0, buf, sizeof buf);
    for (size_t split = 0; split <= sizeof buf; split++) {
        uint32_t c = crc32_update(0, buf, split);
        c = crc32_update(c, buf + split, sizeof buf - split);
        CHECK_EQ_U32(c, whole);
    }

    return TEST_SUMMARY();
}
