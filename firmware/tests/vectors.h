/*
 * vectors.h - read the Python model's test vectors (sim/vectors, D13).
 *
 * Each stage is a file of little-endian int32 values. VECTOR_DIR is set by
 * tests/CMakeLists.txt to the absolute path of sim/vectors.
 */
#ifndef VECTORS_H
#define VECTORS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef VECTOR_DIR
#error "VECTOR_DIR must be defined (tests/CMakeLists.txt)"
#endif

/* Read <VECTOR_DIR>/<vcase>/<stage>.bin into a malloc'd array. Stores the
 * count in *n_out. Exits the test on any error: a missing vector is a
 * failure, never a silent skip. */
static int32_t *vec_read(const char *vcase, const char *stage, size_t *n_out)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s/%s.bin", VECTOR_DIR, vcase, stage);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (bytes <= 0 || bytes % 4 != 0) {
        fprintf(stderr, "%s: bad size %ld\n", path, bytes);
        exit(2);
    }
    size_t n = (size_t)bytes / 4u;
    uint8_t *raw = malloc((size_t)bytes);
    int32_t *v = malloc(n * sizeof *v);
    if (raw == NULL || v == NULL || fread(raw, 1, (size_t)bytes, f) != (size_t)bytes) {
        fprintf(stderr, "%s: read failed\n", path);
        exit(2);
    }
    fclose(f);
    /* Assemble from bytes so the test is right on any host byte order. */
    for (size_t i = 0; i < n; i++) {
        uint32_t u = (uint32_t)raw[4 * i] | ((uint32_t)raw[4 * i + 1] << 8)
                   | ((uint32_t)raw[4 * i + 2] << 16) | ((uint32_t)raw[4 * i + 3] << 24);
        v[i] = (int32_t)u;
    }
    free(raw);
    *n_out = n;
    return v;
}

#endif /* VECTORS_H */
