/*
 * clock.c - see clock.h
 *
 * The order of operations is the point of this file. Going above 150 MHz
 * (RM0440 section 6.1.5, "Range 1 normal mode to Range 1 boost mode"):
 *   1. divide the AHB clock by 2
 *   2. clear PWR_CR5.R1MODE (select boost)
 *   3. set the flash wait states for the new frequency
 *   4. switch SYSCLK to the PLL
 *   5. wait at least 1 us, then set the AHB prescaler back to /1
 * If the wait states were set after the switch, the CPU would briefly fetch from
 * flash too fast and read garbage: the classic "works at 16 MHz, crashes at 170".
 */
#include "clock.h"
#include "board.h"

/* Spin budget for each hardware ready flag. At 16 MHz one loop pass is a few
 * cycles, so this is several ms: far longer than any of these flags need
 * (HSE crystal start-up is the slowest, ~2 ms typical). */
#define READY_SPINS  200000u

static clock_status_t s_status;

/* Wait until (*reg & mask) == want, at most READY_SPINS reads. */
static bool wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t want)
{
    for (uint32_t n = READY_SPINS; n != 0u; n--) {
        if ((*reg & mask) == want) { return true; }
    }
    return false;
}

static void set_ahb_prescaler(uint32_t hpre)
{
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE_Msk) | hpre;
}

/* Back to the reset clock: HSI16, AHB /1. Used when a step fails. */
static void fall_back_to_hsi(void)
{
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_HSI;
    (void)wait_bits(&RCC->CFGR, RCC_CFGR_SWS_Msk, RCC_CFGR_SWS_HSI);
    set_ahb_prescaler(RCC_CFGR_HPRE_DIV1);
    /* The wait states and boost mode are left as they are: more wait states
     * and a higher core voltage are always safe at a lower frequency. */
}

static clock_err_t start_oscillator(clock_src_t src)
{
    /* HSI16 is on after reset, but make sure: LPUART1 also depends on it. */
    RCC->CR |= RCC_CR_HSION;
    if (!wait_bits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) {
        return CLOCK_ERR_HSI_READY;
    }
    if (src == CLOCK_SRC_HSE) {
        RCC->CR &= ~RCC_CR_HSEBYP;          /* crystal, not an external clock signal */
        RCC->CR |= RCC_CR_HSEON;
        if (!wait_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY)) {
            RCC->CR &= ~RCC_CR_HSEON;
            return CLOCK_ERR_HSE_READY;
        }
    }
    return CLOCK_OK;
}

static clock_err_t start_pll(clock_src_t src)
{
    /* PLLCFGR can only be written while the PLL is off. */
    RCC->CR &= ~RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY, 0u)) {
        return CLOCK_ERR_PLL_READY;
    }

    const uint32_t m = (src == CLOCK_SRC_HSE) ? PLL_M_HSE : PLL_M_HSI;
    RCC->PLLCFGR = ((src == CLOCK_SRC_HSE) ? RCC_PLLCFGR_PLLSRC_HSE : RCC_PLLCFGR_PLLSRC_HSI)
                 | ((m - 1u)             << RCC_PLLCFGR_PLLM_Pos)   /* field holds M - 1 */
                 | (PLL_N                << RCC_PLLCFGR_PLLN_Pos)
                 | (((PLL_R / 2u) - 1u)  << RCC_PLLCFGR_PLLR_Pos)   /* 00 = /2, 01 = /4 ... */
                 | RCC_PLLCFGR_PLLREN;                              /* enable the R output (SYSCLK) */

    RCC->CR |= RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) {
        return CLOCK_ERR_PLL_READY;
    }
    return CLOCK_OK;
}

static clock_err_t switch_to_170mhz(void)
{
    /* PWR registers need the PWR interface clock. */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;                    /* read back so the enable has taken effect */

    /* Range 1 is the reset value of VOS; boost is an option within it. */
    if ((PWR->CR1 & PWR_CR1_VOS_Msk) != PWR_CR1_VOS_0) {
        PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | PWR_CR1_VOS_0;
    }

    /* 1. AHB /2, so HCLK only steps 16 -> 85 MHz at the switch, not 16 -> 170. */
    set_ahb_prescaler(RCC_CFGR_HPRE_DIV2);

    /* 2. Range 1 boost (R1MODE = 0 selects boost; reset value is 1 = normal). */
    PWR->CR5 &= ~PWR_CR5_R1MODE;
    if (!wait_bits(&PWR->SR2, PWR_SR2_VOSF, 0u)) {
        return CLOCK_ERR_VOS;
    }

    /* 3. Flash: 4 wait states, plus prefetch and the instruction/data caches.
     *    Read back: the new latency must be in effect before the switch. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk)
               | FLASH_LATENCY_170MHZ | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    if (!wait_bits(&FLASH->ACR, FLASH_ACR_LATENCY_Msk, FLASH_LATENCY_170MHZ)) {
        return CLOCK_ERR_FLASH_LATENCY;
    }

    /* 4. SYSCLK = PLL. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_PLL;
    if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS_Msk, RCC_CFGR_SWS_PLL)) {
        return CLOCK_ERR_SWITCH;
    }

    /* 5. Wait >= 1 us at HCLK = 85 MHz (85 cycles). Each pass of this loop is
     *    at least 2 cycles, so 200 passes is >= 4.7 us. Volatile keeps the
     *    compiler from deleting it. */
    for (volatile uint32_t n = 200u; n != 0u; n--) { }
    set_ahb_prescaler(RCC_CFGR_HPRE_DIV1);

    /* APB1 and APB2 at /1, so the timers also run at 170 MHz (D27). */
    RCC->CFGR &= ~(RCC_CFGR_PPRE1_Msk | RCC_CFGR_PPRE2_Msk);
    return CLOCK_OK;
}

void clock_init(clock_src_t src)
{
    s_status.requested  = src;
    s_status.pll_source = src;
    s_status.err        = CLOCK_OK;

    clock_err_t e = start_oscillator(src);
    if (e == CLOCK_ERR_HSE_READY) {
        /* No crystal (or a solder-bridge problem): carry on from HSI and say so. */
        s_status.err        = e;
        s_status.pll_source = CLOCK_SRC_HSI;
        e = start_oscillator(CLOCK_SRC_HSI);
    }
    if (e == CLOCK_OK) { e = start_pll(s_status.pll_source); }
    if (e == CLOCK_OK) { e = switch_to_170mhz(); }

    if (e != CLOCK_OK) {
        fall_back_to_hsi();
        s_status.err = e;
    }

    SystemCoreClockUpdate();                /* recompute from the registers, don't assume */
    s_status.sysclk_hz = SystemCoreClock;
}

void clock_hold_hsi(clock_err_t reason)
{
    s_status.requested  = CLOCK_SRC_HSI;
    s_status.pll_source = CLOCK_SRC_HSI;
    s_status.err        = reason;
    SystemCoreClockUpdate();                /* 16 MHz: the reset configuration */
    s_status.sysclk_hz  = SystemCoreClock;
}

const clock_status_t *clock_status(void)
{
    return &s_status;
}

const char *clock_err_str(clock_err_t e)
{
    switch (e) {
    case CLOCK_OK:                return "ok";
    case CLOCK_ERR_HSE_READY:     return "HSE not ready (using HSI)";
    case CLOCK_ERR_HSI_READY:     return "HSI not ready";
    case CLOCK_ERR_PLL_READY:     return "PLL not ready";
    case CLOCK_ERR_VOS:           return "regulator (VOSF) not settled";
    case CLOCK_ERR_FLASH_LATENCY: return "flash latency not applied";
    case CLOCK_ERR_SWITCH:        return "SYSCLK switch to PLL failed";
    case CLOCK_HELD_AFTER_FAULT:  return "held at HSI16 after a fault (reset to retry)";
    case CLOCK_HELD_BY_BUILD:     return "held at HSI16 (LOCKIN_NO_PLL build)";
    }
    return "?";
}
