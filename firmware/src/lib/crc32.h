/*
 * crc32.h - CRC-32 with the zlib/Ethernet parameters, in portable C.
 *
 *   polynomial 0x04C11DB7 (reflected form 0xEDB88320), init 0xFFFFFFFF,
 *   input and output reflected, final XOR 0xFFFFFFFF.
 *   Check value: crc32("123456789") = 0xCBF43926.
 *
 * The capture frames (decision D35) carry this CRC. On the MCU it is computed by
 * the hardware CRC unit; this software version is the reference the hardware
 * result is checked against, and what the host-side unit tests use. The host
 * script checks frames with Python's zlib.crc32(), which uses the same parameters.
 *
 * Usage matches zlib: start with crc = 0, feed chunks, the result needs no
 * further processing:
 *     uint32_t c = crc32_update(0, a, na);
 *     c = crc32_update(c, b, nb);          // == crc32 of a followed by b
 */
#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

#define CRC32_CHECK_VALUE 0xCBF43926u   /* crc32 of the ASCII bytes "123456789" */

uint32_t crc32_update(uint32_t crc, const void *data, size_t len);

#endif /* CRC32_H */
