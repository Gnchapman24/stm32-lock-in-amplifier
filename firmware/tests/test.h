/*
 * test.h - the whole host-side test "framework" (D37): a check macro that
 * reports file:line and keeps going, and a summary that sets the exit code
 * CTest reads.
 *
 *     int main(void) {
 *         CHECK(1 + 1 == 2);
 *         CHECK_EQ_U32(crc, 0xCBF43926u);
 *         return TEST_SUMMARY();
 *     }
 */
#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdio.h>

static int g_test_checks;
static int g_test_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_test_checks++;                                                     \
        if (!(cond)) {                                                       \
            g_test_failures++;                                               \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                     \
                    __FILE__, __LINE__, #cond);                              \
        }                                                                    \
    } while (0)

#define CHECK_EQ_U32(actual, expected)                                       \
    do {                                                                     \
        uint32_t a_ = (uint32_t)(actual), e_ = (uint32_t)(expected);         \
        g_test_checks++;                                                     \
        if (a_ != e_) {                                                      \
            g_test_failures++;                                               \
            fprintf(stderr, "%s:%d: %s == 0x%08lX, expected 0x%08lX\n",      \
                    __FILE__, __LINE__, #actual,                             \
                    (unsigned long)a_, (unsigned long)e_);                   \
        }                                                                    \
    } while (0)

#define CHECK_EQ_I32(actual, expected)                                       \
    do {                                                                     \
        int32_t a_ = (int32_t)(actual), e_ = (int32_t)(expected);            \
        g_test_checks++;                                                     \
        if (a_ != e_) {                                                      \
            g_test_failures++;                                               \
            fprintf(stderr, "%s:%d: %s == %ld, expected %ld\n",              \
                    __FILE__, __LINE__, #actual, (long)a_, (long)e_);        \
        }                                                                    \
    } while (0)

#define TEST_SUMMARY()                                                       \
    (printf("%d checks, %d failed\n", g_test_checks, g_test_failures),       \
     g_test_failures == 0 ? 0 : 1)

#endif /* TEST_H */
