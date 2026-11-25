/*
 * crc32.c - see crc32.h
 *
 * Bit-at-a-time, no table: 8 shift/XOR steps per byte. Slow (a 32 KB frame takes
 * a few ms on the MCU) but obviously correct, which is the point of a reference.
 */
#include "crc32.h"

#define CRC32_POLY_REFLECTED 0xEDB88320u

uint32_t crc32_update(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    crc = ~crc;                          /* undo the previous final XOR (or apply init) */
    while (len-- != 0u) {
        crc ^= *p++;
        for (int bit = 0; bit < 8; bit++) {
            /* -(crc & 1) is all ones when the low bit is set, so this XORs the
             * polynomial in exactly when a 1 is shifted out. */
            crc = (crc >> 1) ^ (CRC32_POLY_REFLECTED & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}
