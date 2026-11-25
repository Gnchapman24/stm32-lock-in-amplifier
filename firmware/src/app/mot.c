/*
 * mot.c - see mot.h
 */
#include "mot.h"
#include "tick.h"

static mot_stats_t s_stats;

void mot_run(const mot_device_t *devices, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (devices[i].init != NULL) { devices[i].init(); }
    }

    for (;;) {
        uint32_t t0 = cycles();
        for (size_t i = 0; i < count; i++) {
            if (devices[i].poll != NULL) { devices[i].poll(); }
        }
        /* The longest pass is the worst-case delay before any device gets to
         * react: a quick check that no poll() is secretly blocking. */
        uint32_t dt = cycles() - t0;
        if (dt > s_stats.max_pass_cyc) { s_stats.max_pass_cyc = dt; }
        s_stats.passes++;
    }
}

const mot_stats_t *mot_stats(void)
{
    return &s_stats;
}

void mot_reset_max(void)
{
    s_stats.max_pass_cyc = 0u;
}
