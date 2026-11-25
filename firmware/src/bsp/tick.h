/*
 * tick.h - 1 ms system tick (SysTick) and the DWT cycle counter (D33, D38).
 */
#ifndef TICK_H
#define TICK_H

#include <stdint.h>
#include "stm32g4xx.h"

/* Start SysTick at 1 kHz from SystemCoreClock, and the DWT cycle counter.
 * Call after clock_init(), so the reload value uses the final clock. */
void tick_init(void);

/* Milliseconds since tick_init(). Wraps after ~49.7 days; compare with
 * (int32_t)(a - b) so the wrap doesn't matter. Replaces HAL_GetTick. */
uint32_t millis(void);

/* CPU cycles, free-running at SYSCLK (wraps every 25 s at 170 MHz).
 * The unsigned difference of two reads is correct across one wrap. */
static inline uint32_t cycles(void)
{
    return DWT->CYCCNT;
}

#endif /* TICK_H */
