/*
 * dac_map.h - Q31 sine -> 12-bit DAC code (D30).
 *
 *     code = DAC_MID + (((int64_t)sin_q31 * amp + 2^30) >> 31)
 *
 * The multiply rounds half up, the same rule as every Q31 multiply (D6,
 * sim/Q31-RULES.md section 1). It relies on >> of a negative int64_t being an
 * arithmetic shift (floor), which GCC guarantees. No saturation is needed:
 * |sin| <= 1, so the offset is within +-amp.
 *
 * The model is sim/lockin/dac.py. The host test checks this against
 * sim/vectors/dac_map bit for bit. On hardware the sine comes from the CORDIC,
 * which isn't bit-exact with the model (D12), so the hardware check in B3 is
 * "within 1 code".
 */
#ifndef DAC_MAP_H
#define DAC_MAP_H

#include <stdbool.h>
#include <stdint.h>

#define DAC_MID       2048            /* mid-scale: 0 V of the excitation */
#define DAC_AMP       1600            /* default peak amplitude in codes (~1.29 V peak) */

/* Linear window of the buffered output: 0.2 V .. VREF+ - 0.2 V (DS12288),
 * which is codes 248 .. 3847 at 3.3 V. */
#define DAC_CODE_MIN  248
#define DAC_CODE_MAX  3847

_Static_assert(DAC_MID - DAC_AMP >= DAC_CODE_MIN && DAC_MID + DAC_AMP <= DAC_CODE_MAX,
               "DAC_AMP leaves the DAC output's linear window (D30)");

/* Run-time version of the check above, for an amplitude set at run time. */
static inline bool dac_amp_ok(int32_t amp)
{
    return amp >= 0 && DAC_MID - amp >= DAC_CODE_MIN && DAC_MID + amp <= DAC_CODE_MAX;
}

/* One sample. amp must pass dac_amp_ok(): that's what keeps the code in range. */
static inline uint16_t dac_code_from_sin(int32_t sin_q31, int32_t amp)
{
    int64_t offset = ((int64_t)sin_q31 * amp + ((int64_t)1 << 30)) >> 31;
    return (uint16_t)(DAC_MID + (int32_t)offset);
}

#endif /* DAC_MAP_H */
