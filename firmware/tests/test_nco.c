/*
 * test_nco.c - the C NCO reproduces the model's phase sequence exactly (D2, D13).
 */
#include "test.h"
#include "vectors.h"
#include "nco.h"

/* delta and phase0 copied from each case's config.json. A typo here makes the
 * phase comparison fail, so it can't pass by accident. */
static const struct {
    const char *name;
    uint32_t delta;
    uint32_t phase0;
} k_cases[] = {
    { "basic_1k",     42949673u, 0u          },
    { "sync_1030",    44238163u, 0u          },
    { "harm2_order4", 21474836u, 0u          },
    { "clip_edge",    85899346u, 2654435769u },
};

static void check_against_vectors(void)
{
    for (size_t c = 0; c < sizeof k_cases / sizeof k_cases[0]; c++) {
        size_t n;
        int32_t *expected = vec_read(k_cases[c].name, "phase", &n);
        CHECK(n == 4096u);

        /* One sample at a time, the way the ISR will run it. */
        nco_t a;
        nco_init(&a, k_cases[c].delta, k_cases[c].phase0);
        size_t first_bad = n;
        for (size_t i = 0; i < n; i++) {
            if (nco_next(&a) != (uint32_t)expected[i] && first_bad == n) { first_bad = i; }
        }
        if (first_bad != n) {
            fprintf(stderr, "%s: nco_next differs from phase.bin at sample %lu\n",
                    k_cases[c].name, (unsigned long)first_bad);
        }
        CHECK(first_bad == n);

        /* In blocks of 64 (D21), checking that nco_fill carries the phase across calls. */
        nco_t b;
        nco_init(&b, k_cases[c].delta, k_cases[c].phase0);
        uint32_t block[64];
        size_t mismatches = 0;
        for (size_t i = 0; i < n; i += 64u) {
            nco_fill(&b, block, 64u);
            for (size_t j = 0; j < 64u; j++) {
                if (block[j] != (uint32_t)expected[i + j]) { mismatches++; }
            }
        }
        CHECK(mismatches == 0u);
        CHECK_EQ_U32(b.phase, a.phase);   /* both end at sample n */
        free(expected);
    }
}

static void check_harmonic_and_angle(void)
{
    /* Same values as sim/tests/test_nco.py::TestHarmonic::test_values. */
    CHECK_EQ_U32(nco_harmonic(0u, 2u), 0u);
    CHECK_EQ_U32(nco_harmonic(1u << 30, 2u), 1u << 31);
    CHECK_EQ_U32(nco_harmonic(3u << 30, 2u), 1u << 31);   /* 1.5 cycles wraps to 0.5 */
    CHECK_EQ_U32(nco_harmonic(0x9E3779B9u, 3u), 0xDAA66D2Bu); /* 3 * 0x9E3779B9 mod 2^32 */

    /* Same values as TestCordicAngle: 0, half cycle = -pi, +pi/2, -pi/2. */
    CHECK_EQ_I32(nco_to_cordic_angle(0u), 0);
    CHECK_EQ_I32(nco_to_cordic_angle(1u << 31), INT32_MIN);
    CHECK_EQ_I32(nco_to_cordic_angle(1u << 30), 1 << 30);
    CHECK_EQ_I32(nco_to_cordic_angle(3u << 30), -(1 << 30));
}

static void check_periodic(void)
{
    /* B4: a 128-sample buffer (2B, D21) replays cleanly only for delta = k * 2^25. */
    for (uint32_t k = 1u; k < 64u; k++) {
        uint32_t delta = k << 25;
        CHECK(nco_is_periodic(delta, 128u));
        nco_t n;
        nco_init(&n, delta, 0x12345678u);
        uint32_t buf[128];
        nco_fill(&n, buf, 128u);
        CHECK_EQ_U32(n.phase, 0x12345678u);   /* back to the start after 128 steps */
    }
    CHECK(!nco_is_periodic((1u << 25) + 1u, 128u));
    CHECK(!nco_is_periodic(42949673u, 128u));        /* 1 kHz: not periodic in 128 */
    CHECK(!nco_is_periodic(1u << 24, 128u));         /* half a cycle per buffer */
    CHECK(nco_is_periodic(1u << 24, 256u));
}

int main(void)
{
    check_against_vectors();
    check_harmonic_and_angle();
    check_periodic();
    return TEST_SUMMARY();
}
