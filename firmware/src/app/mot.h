/*
 * mot.h - Machine-on-Tether main loop: a cooperative dispatcher (D16, D33).
 *
 * The main loop is a fixed table of "devices". Each has an init() that runs
 * once and a poll() that is called on every pass, forever. A poll() must
 * return quickly and never block: it does a small step of work and keeps its
 * own state between calls (a state machine). Because no device can hold the
 * CPU, none can starve another, and none need locks.
 *
 * Hard real-time work does NOT belong here: it runs in the DMA interrupt,
 * which pre-empts this loop whenever it needs to. The loop is "everything
 * that can wait a millisecond".
 *
 * The idea is from the ENEE440 course's Machine-on-Tether; the course's
 * prebuilt library is not used.
 */
#ifndef MOT_H
#define MOT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    void (*init)(void);     /* may be NULL */
    void (*poll)(void);     /* may be NULL */
} mot_device_t;

typedef struct {
    uint32_t passes;        /* main-loop passes since start (wraps)            */
    uint32_t max_pass_cyc;  /* longest single pass, in CPU cycles (incl. ISRs) */
} mot_stats_t;

/* Init every device in table order, then poll them all forever. */
__attribute__((noreturn))
void mot_run(const mot_device_t *devices, size_t count);

const mot_stats_t *mot_stats(void);
void mot_reset_max(void);

#endif /* MOT_H */
