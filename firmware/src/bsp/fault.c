/*
 * fault.c - see fault.h
 */
#include "fault.h"
#include "stm32g4xx.h"

#include <stddef.h>

#define FAULT_MAGIC 0xFA017EADu

/* Survives a software reset: .noinit is neither copied nor zeroed at start-up.
 * After power-on it holds random bits, which is what the magic number guards. */
static fault_record_t s_saved __attribute__((section(".noinit")));

static fault_record_t s_last;
static bool           s_have_last;
static const char    *s_reset_cause = "?";

/* Called from the naked handler below with a pointer to the exception frame
 * the hardware pushed: r0, r1, r2, r3, r12, lr, pc, xpsr. */
__attribute__((used, noreturn))
void fault_save_and_reset(const uint32_t *frame)
{
    s_saved.pc    = frame[6];
    s_saved.lr    = frame[5];
    s_saved.xpsr  = frame[7];
    s_saved.cfsr  = SCB->CFSR;
    s_saved.hfsr  = SCB->HFSR;
    s_saved.bfar  = SCB->BFAR;
    s_saved.mmfar = SCB->MMFAR;
    s_saved.magic = FAULT_MAGIC;
    __DSB();                         /* the record is in RAM before the reset */
    NVIC_SystemReset();
}

/* Naked: no prologue, so the stack pointer still points at the exception frame.
 * Bit 2 of EXC_RETURN (in LR) says which stack the frame was pushed on. This
 * firmware only uses MSP, but checking costs two instructions. */
__attribute__((naked))
void HardFault_Handler(void)
{
    __asm volatile(
        "tst   lr, #4              \n"
        "ite   eq                  \n"
        "mrseq r0, msp             \n"
        "mrsne r0, psp             \n"
        "b     fault_save_and_reset\n");
}

void fault_boot_check(void)
{
    if (s_saved.magic == FAULT_MAGIC) {
        s_last      = s_saved;
        s_have_last = true;
    }
    s_saved.magic = 0u;

    /* Reset cause: several flags can be set at once; report the most specific. */
    uint32_t csr = RCC->CSR;
    if      (csr & RCC_CSR_LPWRRSTF) { s_reset_cause = "low-power"; }
    else if (csr & RCC_CSR_WWDGRSTF) { s_reset_cause = "window watchdog"; }
    else if (csr & RCC_CSR_IWDGRSTF) { s_reset_cause = "watchdog"; }
    else if (csr & RCC_CSR_SFTRSTF)  { s_reset_cause = s_have_last ? "hard fault" : "software"; }
    else if (csr & RCC_CSR_BORRSTF)  { s_reset_cause = "power-on"; }
    else if (csr & RCC_CSR_OBLRSTF)  { s_reset_cause = "option bytes"; }
    else if (csr & RCC_CSR_PINRSTF)  { s_reset_cause = "pin"; }
    RCC->CSR |= RCC_CSR_RMVF;        /* clear the flags for next time */
}

const fault_record_t *fault_last(void)
{
    return s_have_last ? &s_last : NULL;
}

const char *fault_reset_cause(void)
{
    return s_reset_cause;
}
