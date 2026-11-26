/*
 * clock.h - system clock: 170 MHz from the PLL in Range 1 boost mode (D19).
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CLOCK_SRC_HSI = 0,      /* internal 16 MHz RC, +-1 %  (fallback)         */
    CLOCK_SRC_HSE = 1,      /* external 24 MHz crystal X3 (build default)    */
} clock_src_t;

/* Which step of clock_init() failed. CLOCK_OK means SYSCLK = 170 MHz. */
typedef enum {
    CLOCK_OK = 0,
    CLOCK_ERR_HSE_READY,    /* HSE didn't start; fell back to HSI */
    CLOCK_ERR_HSI_READY,
    CLOCK_ERR_PLL_READY,
    CLOCK_ERR_VOS,          /* regulator didn't settle (PWR_SR2.VOSF)  */
    CLOCK_ERR_FLASH_LATENCY,/* FLASH_ACR read-back didn't show 4 WS     */
    CLOCK_ERR_SWITCH,       /* SWS never reported the PLL               */
    CLOCK_HELD_AFTER_FAULT, /* PLL not tried: last reset was a HardFault */
    CLOCK_HELD_BY_BUILD,    /* PLL not tried: built with LOCKIN_NO_PLL   */
} clock_err_t;

typedef struct {
    clock_src_t requested;
    clock_src_t pll_source;  /* what actually feeds the PLL              */
    clock_err_t err;         /* first failure, or CLOCK_OK               */
    uint32_t    sysclk_hz;   /* SystemCoreClock after clock_init()       */
} clock_status_t;

/*
 * Switch SYSCLK to 170 MHz. Every hardware wait is bounded: if one times out,
 * the chip stays (or goes back to) a safe 16 MHz HSI clock, and the error is
 * recorded rather than hanging. The serial console runs from HSI16 either way,
 * so it can always report what went wrong.
 */
void clock_init(clock_src_t src);

/* Stay on the 16 MHz reset clock and record why (one of the CLOCK_HELD_*
 * values). Used for the first bring-up steps, and as a safe mode after a
 * fault so a crash during or just after the 170 MHz switch can't turn into
 * a silent reset loop. */
void clock_hold_hsi(clock_err_t reason);

const clock_status_t *clock_status(void);
const char *clock_err_str(clock_err_t e);

#endif /* CLOCK_H */
