/*
 * tick.c - see tick.h
 */
#include "tick.h"
#include "board.h"

/* One writer (SysTick_Handler), any number of readers. A 32-bit aligned load is
 * a single instruction on the M4, so readers never see a torn value. */
static volatile uint32_t s_ms;

void tick_init(void)
{
    /* SysTick from the core clock, 1 kHz. SysTick_Config also sets the
     * priority to the lowest level; set it explicitly so it's visible here. */
    (void)SysTick_Config(SystemCoreClock / 1000u);
    NVIC_SetPriority(SysTick_IRQn, IRQ_PRIO_SYSTICK);

    /* DWT cycle counter: enable trace (TRCENA) first, or DWT writes are ignored. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t millis(void)
{
    return s_ms;
}

void SysTick_Handler(void)
{
    s_ms++;
}
