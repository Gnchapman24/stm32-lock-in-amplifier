/*
 * fault.h - make a HardFault visible after the reboot it causes.
 *
 * The HardFault handler saves the faulting PC and the fault status registers
 * into RAM that the startup code doesn't clear (.noinit), then resets. On the
 * next boot fault_boot_check() picks the record up, so the console can print
 * where the crash happened. Look the PC up with
 *     arm-none-eabi-addr2line -e lockin.elf <pc>
 */
#ifndef FAULT_H
#define FAULT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;     /* FAULT_MAGIC when the rest is valid */
    uint32_t pc;        /* stacked PC: the instruction that faulted     */
    uint32_t lr;        /* stacked LR: usually the caller               */
    uint32_t xpsr;
    uint32_t cfsr;      /* SCB->CFSR: which fault (UFSR/BFSR/MMFSR)     */
    uint32_t hfsr;      /* SCB->HFSR: FORCED = escalated from another fault */
    uint32_t bfar;      /* bus fault address, valid if CFSR.BFARVALID   */
    uint32_t mmfar;     /* memory-manage fault address, if MMARVALID    */
} fault_record_t;

/* Call first thing in main. Reads and clears the saved record, and the
 * reset-cause flags in RCC_CSR. */
void fault_boot_check(void);

/* The record from before the last reset, or NULL if there was no fault. */
const fault_record_t *fault_last(void);

/* Short name of the reset cause, e.g. "pin", "power-on", "software". */
const char *fault_reset_cause(void);

#endif /* FAULT_H */
