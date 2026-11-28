/*
 * nco.h - numerically controlled oscillator: a 32-bit phase accumulator (D2).
 *
 * The phase is a uint32_t, with 2^32 = one cycle. Each sample does
 * phase += delta, and uint32_t arithmetic wraps for free (C guarantees modulo
 * 2^32 for unsigned types). The model is sim/lockin/nco.py, and the host test
 * checks this code against every phase.bin in sim/vectors bit for bit.
 *
 * Header-only: these are one or two instructions each and get called per
 * sample in the ISR.
 *
 * Where delta comes from is deliberately not decided here. The Python model
 * computes delta = round(f_ref / f_s * 2^32) in double precision. Bring-up
 * step B4 only uses delta = k * 2^25 (see nco_is_periodic and the D25 note).
 */
#ifndef NCO_H
#define NCO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t phase;   /* phase of the NEXT sample */
    uint32_t delta;   /* increment per sample */
} nco_t;

static inline void nco_init(nco_t *n, uint32_t delta, uint32_t phase0)
{
    n->phase = phase0;
    n->delta = delta;
}

/* Return the current phase, then advance. Sample 0 gets phase0, matching
 * nco.phases(delta, n, phase0)[0] in the model. */
static inline uint32_t nco_next(nco_t *n)
{
    uint32_t p = n->phase;
    n->phase = p + n->delta;
    return p;
}

/* count consecutive phases into out[]: the same as count calls of nco_next(). */
static inline void nco_fill(nco_t *n, uint32_t *out, size_t count)
{
    uint32_t p = n->phase;
    const uint32_t d = n->delta;
    for (size_t i = 0; i < count; i++) {
        out[i] = p;
        p += d;
    }
    n->phase = p;
}

/* Phase of harmonic k: k * phase mod 2^32 (D2). An unsigned multiply wraps by
 * definition, so it's exact. The model is nco.harmonic(). */
static inline uint32_t nco_harmonic(uint32_t phase, uint32_t k)
{
    return phase * k;
}

/* uint32 phase -> CORDIC q1.31 angle (+-1 <-> +-pi). The same bits, read as
 * signed. Converting an out-of-range value to int32_t is implementation-defined
 * in C11. GCC defines it as modulo 2^32 (GCC manual, "Integers implementation"),
 * which is the reinterpretation we want, and it compiles to no instruction. */
static inline int32_t nco_to_cordic_angle(uint32_t phase)
{
    return (int32_t)phase;
}

/* True if len steps of delta come back exactly to the starting phase, i.e.
 * delta * len == 0 mod 2^32. Then a buffer of len samples holds a whole number
 * of cycles and can be replayed by circular DMA with no refill (B4, D25 note).
 * For len = 128 that means delta = k * 2^25. */
static inline bool nco_is_periodic(uint32_t delta, uint32_t len)
{
    return (uint32_t)(delta * len) == 0u;
}

#endif /* NCO_H */
