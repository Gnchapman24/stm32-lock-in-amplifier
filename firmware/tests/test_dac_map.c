/*
 * test_dac_map.c - the C DAC mapping matches the model bit for bit (D30).
 */
#include "test.h"
#include "vectors.h"
#include "dac_map.h"

static void check_against_vectors(void)
{
    size_t n_sin, n_dac;
    int32_t *sin_v = vec_read("dac_map", "sin", &n_sin);
    int32_t *dac = vec_read("dac_map", "dac", &n_dac);
    CHECK(n_sin == n_dac);
    CHECK(n_sin == 4096u + 9u);   /* the sweep plus the edge values (export_vectors.py) */

    size_t mismatches = 0;
    for (size_t i = 0; i < n_sin && i < n_dac; i++) {
        uint16_t got = dac_code_from_sin(sin_v[i], DAC_AMP);
        if (got != (uint16_t)dac[i]) {
            if (mismatches < 5u) {
                fprintf(stderr, "dac_map[%lu]: sin=%ld -> %u, expected %ld\n",
                        (unsigned long)i, (long)sin_v[i], (unsigned)got, (long)dac[i]);
            }
            mismatches++;
        }
    }
    CHECK(mismatches == 0u);
    free(sin_v);
    free(dac);
}

static void check_edges(void)
{
    /* Full scale: -1.0 -> 2048 - 1600; (1 - 2^-31) rounds to 2048 + 1600. */
    CHECK_EQ_I32(dac_code_from_sin(INT32_MIN, DAC_AMP), 448);
    CHECK_EQ_I32(dac_code_from_sin(INT32_MAX, DAC_AMP), 3648);
    CHECK_EQ_I32(dac_code_from_sin(0, DAC_AMP), 2048);

    /* +-2^24 * 1600 / 2^31 = +-12.5 exactly. Half up: +13 and -12 (not -13). */
    CHECK_EQ_I32(dac_code_from_sin(1 << 24, DAC_AMP), 2048 + 13);
    CHECK_EQ_I32(dac_code_from_sin(-(1 << 24), DAC_AMP), 2048 - 12);
    /* One LSB either side of those halves rounds the other way. */
    CHECK_EQ_I32(dac_code_from_sin((1 << 24) - 1, DAC_AMP), 2048 + 12);
    CHECK_EQ_I32(dac_code_from_sin(-(1 << 24) - 1, DAC_AMP), 2048 - 13);
}

static void check_amp(void)
{
    CHECK(dac_amp_ok(DAC_AMP));
    CHECK(dac_amp_ok(0));
    CHECK(dac_amp_ok(1799));         /* 2048 + 1799 = 3847, the top of the window */
    CHECK(!dac_amp_ok(1800));
    CHECK(!dac_amp_ok(-1));

    /* With the largest allowed amp, full scale lands inside the window. */
    CHECK(dac_code_from_sin(INT32_MIN, 1799) >= DAC_CODE_MIN);
    CHECK(dac_code_from_sin(INT32_MAX, 1799) <= DAC_CODE_MAX);
}

int main(void)
{
    check_against_vectors();
    check_edges();
    check_amp();
    return TEST_SUMMARY();
}
