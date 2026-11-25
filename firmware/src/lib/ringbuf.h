/*
 * ringbuf.h - lock-free single-producer / single-consumer byte queue
 *
 * Reused unchanged from the earlier telemetry project (decision D18). It has no
 * hardware dependency, so it is built and unit-tested on the PC (tests/test_ringbuf.c).
 *
 * Design notes
 *  - Size must be a power of two; indices are free-running 32-bit counters, so
 *    "full" and "empty" are unambiguous and wrap-around needs only a mask.
 *  - Exactly ONE context may call rb_put() and exactly ONE (other) context may
 *    call rb_get().  Under that rule no interrupt masking is required: the
 *    producer only writes `head`, the consumer only writes `tail`.
 *  - When full, the NEWEST byte is refused and `dropped` is incremented.
 *    (Overwrite-oldest would make the producer modify consumer state, which
 *    is what forces a critical section.)
 *  - Release/acquire ordering guarantees the data byte is visible before the
 *    index that publishes it.
 *
 *  Which side owns what:
 *      RX queue : producer = LPUART1 ISR,  consumer = main loop
 *      TX queue : producer = main loop,    consumer = LPUART1 ISR
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  *buf;
    uint32_t  mask;      /* size - 1                                   */
    uint32_t  head;      /* next write position   (producer writes)    */
    uint32_t  tail;      /* next read position    (consumer writes)    */
    uint32_t  dropped;   /* bytes refused because full (producer only) */
} rb_t;

static inline void rb_init(rb_t *r, uint8_t *storage, uint32_t size_pow2)
{
    r->buf     = storage;
    r->mask    = size_pow2 - 1u;
    r->head    = 0;
    r->tail    = 0;
    r->dropped = 0;
}

/* Number of bytes currently queued.  Safe from either side; the result can
 * only be stale in the "conservative" direction (producer sees too few free
 * slots, consumer sees too few bytes). */
static inline uint32_t rb_count(const rb_t *r)
{
    uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);
    uint32_t h = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);
    return h - t;
}

/* Free slots (call from the producer side). */
static inline uint32_t rb_free(const rb_t *r)
{
    return (r->mask + 1u) - rb_count(r);
}

/* Producer side. Returns false (and counts a drop) if the queue is full. */
static inline bool rb_put(rb_t *r, uint8_t b)
{
    uint32_t h = __atomic_load_n(&r->head, __ATOMIC_RELAXED);
    uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);

    if ((h - t) > r->mask) {            /* full */
        r->dropped++;
        return false;
    }
    r->buf[h & r->mask] = b;
    __atomic_store_n(&r->head, h + 1u, __ATOMIC_RELEASE);
    return true;
}

/* Consumer side. Returns false if the queue is empty. */
static inline bool rb_get(rb_t *r, uint8_t *out)
{
    uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);
    uint32_t h = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);

    if (h == t) {                       /* empty */
        return false;
    }
    *out = r->buf[t & r->mask];
    __atomic_store_n(&r->tail, t + 1u, __ATOMIC_RELEASE);
    return true;
}

#endif /* RINGBUF_H */
