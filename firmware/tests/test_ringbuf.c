/*
 * test_ringbuf.c - SPSC byte queue: order, full/empty, drop counting and
 * wrap-around of the free-running 32-bit indices.
 *
 * Single-threaded: it checks the queue's logic, not its concurrency. The
 * concurrency argument (each side writes only its own index) is in ringbuf.h.
 */
#include "test.h"
#include "ringbuf.h"

#define SIZE 8u

static void test_fifo_order(void)
{
    uint8_t store[SIZE];
    rb_t r;
    rb_init(&r, store, SIZE);

    uint8_t b = 0;
    CHECK(rb_count(&r) == 0u);
    CHECK(rb_free(&r) == SIZE);
    CHECK(!rb_get(&r, &b));                         /* empty */

    for (uint8_t i = 0; i < 5u; i++) { CHECK(rb_put(&r, (uint8_t)(100u + i))); }
    CHECK(rb_count(&r) == 5u);
    for (uint8_t i = 0; i < 5u; i++) {
        CHECK(rb_get(&r, &b));
        CHECK(b == 100u + i);
    }
    CHECK(rb_count(&r) == 0u);
}

static void test_full_refuses_newest(void)
{
    uint8_t store[SIZE];
    rb_t r;
    rb_init(&r, store, SIZE);

    for (uint8_t i = 0; i < SIZE; i++) { CHECK(rb_put(&r, i)); }
    CHECK(rb_free(&r) == 0u);
    CHECK(!rb_put(&r, 0xAAu));                      /* full: refused ... */
    CHECK(!rb_put(&r, 0xBBu));
    CHECK(r.dropped == 2u);                         /* ... and counted */

    uint8_t b = 0;
    CHECK(rb_get(&r, &b) && b == 0u);               /* oldest data intact */
    CHECK(rb_put(&r, 0xCCu));                       /* one slot free again */
    for (uint8_t i = 1; i < SIZE; i++) { CHECK(rb_get(&r, &b) && b == i); }
    CHECK(rb_get(&r, &b) && b == 0xCCu);
    CHECK(!rb_get(&r, &b));
}

static void test_index_wraparound(void)
{
    uint8_t store[SIZE];
    rb_t r;
    rb_init(&r, store, SIZE);

    /* Start both indices just below 2^32 so they wrap during the test.
     * h - t must stay the true count across the wrap. */
    r.head = r.tail = 0xFFFFFFFCu;
    for (uint32_t i = 0; i < 1000u; i++) {
        CHECK(rb_put(&r, (uint8_t)i));
        CHECK(rb_put(&r, (uint8_t)(i + 1u)));
        CHECK(rb_count(&r) == 2u);
        uint8_t b = 0;
        CHECK(rb_get(&r, &b) && b == (uint8_t)i);
        CHECK(rb_get(&r, &b) && b == (uint8_t)(i + 1u));
    }
    CHECK(r.head < 0xFFFFFFFCu);                    /* it really did wrap */
    CHECK(rb_count(&r) == 0u);

    /* Full detection across the wrap. */
    r.head = r.tail = 0xFFFFFFFEu;
    for (uint8_t i = 0; i < SIZE; i++) { CHECK(rb_put(&r, i)); }
    CHECK(!rb_put(&r, 0xEEu));
    CHECK(rb_count(&r) == SIZE);
}

int main(void)
{
    test_fifo_order();
    test_full_refuses_newest();
    test_index_wraparound();
    return TEST_SUMMARY();
}
