/*
 * board.h - NUCLEO-G474RE: clock targets, pin map and interrupt priorities.
 *
 * Every constant that comes from a hardware or design decision lives here, with
 * the decision number from docs/decisions.md, so nothing elsewhere is a bare
 * hand-computed literal.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32g4xx.h"

/* ------------------------------------------------------------------------ *
 *  Clocks (D19)
 * ------------------------------------------------------------------------ */
#define HSI_HZ          16000000u
#define HSE_HZ          24000000u       /* X3 crystal, UM2505 Table 12 (SB25/SB26 ON) */
#define SYSCLK_HZ       170000000u

/* PLL: f_in / M must be 2.66..16 MHz, f_in / M * N must be 96..344 MHz (DS12288). */
#define PLL_N           85u
#define PLL_R           2u              /* SYSCLK = VCO / R */
#define PLL_M_HSI       4u              /* 16 MHz / 4 = 4 MHz; x85 = 340 MHz; /2 = 170 MHz */
#define PLL_M_HSE       6u              /* 24 MHz / 6 = 4 MHz; same VCO and output */

_Static_assert(HSI_HZ / PLL_M_HSI * PLL_N / PLL_R == SYSCLK_HZ, "HSI PLL settings");
_Static_assert(HSE_HZ / PLL_M_HSE * PLL_N / PLL_R == SYSCLK_HZ, "HSE PLL settings");
_Static_assert(HSI_HZ / PLL_M_HSI * PLL_N <= 344000000u, "PLL VCO above 344 MHz");

/* Flash wait states for 170 MHz in Range 1 boost: RM0440 Table 9 (4 WS up to 170 MHz). */
#define FLASH_LATENCY_170MHZ  FLASH_ACR_LATENCY_4WS

/* Sample rate (D4). The timer that produces it is configured in a later stage. */
#define FS_HZ           100000u
_Static_assert(SYSCLK_HZ % FS_HZ == 0u, "f_s must divide SYSCLK exactly");

/* ------------------------------------------------------------------------ *
 *  Pins (D26, D27, D28). Reserved, never reuse: PA2/PA3 LPUART1, PA13/PA14
 *  SWD, PC13 USER button, PF0/PF1 HSE crystal.
 * ------------------------------------------------------------------------ */
#define LED_PORT        GPIOA           /* LD2, green user LED (SB6 ON) */
#define LED_PIN         5u

#define DBG_ISR_PORT    GPIOA           /* PA9 = Arduino D8: high while the DMA ISR runs */
#define DBG_ISR_PIN     9u
#define DBG_AUX_PORT    GPIOA           /* PA10 = Arduino D2: spare marker */
#define DBG_AUX_PIN     10u

/* Sample clock (D27): TIM2 generates f_s and its CH3 PWM output marks every
 * sample instant on PB10 (Arduino D6), AF1. */
#define SAMPLE_TIM          TIM2
#define SAMPLE_PWM_PORT     GPIOB
#define SAMPLE_PWM_PIN      10u
#define SAMPLE_PWM_AF       1u
#define SAMPLE_ARR          (SYSCLK_HZ / FS_HZ - 1u)          /* 1699 */
#define SAMPLE_CCR3         ((SAMPLE_ARR + 1u) / 2u)          /* 50 % duty */
_Static_assert(SAMPLE_ARR == 1699u, "TIM2 ARR for 170 MHz / 100 kHz");

/* Set / clear one pin with a single atomic store to BSRR. */
#define PIN_HIGH(port, pin)  ((port)->BSRR = (1u << (pin)))
#define PIN_LOW(port, pin)   ((port)->BSRR = (1u << ((pin) + 16u)))

/* ------------------------------------------------------------------------ *
 *  Interrupt priorities (D22, D31). Lower number = more urgent. Priority
 *  grouping is set so all 4 bits are pre-emption bits: a lower number always
 *  interrupts a higher one.
 *
 *  Worst-case reasoning (cost per call is an estimate until measured):
 *  - LPUART1 (1) pre-empts everything else. RX interrupts on "FIFO not
 *    empty" and TX on "FIFO not full", so in steady traffic the ISR runs
 *    about once per byte per direction (10.85 us per byte at 921600 baud),
 *    moving one byte each call (at most 8 per FIFO).
 *  - The DMA ISR (2) must finish within B / f_s = 640 us (D21). LPUART1 can
 *    pre-empt it before and during its run, so the cost is the total LPUART1
 *    time in one block. The line rate caps that at ~59 bytes per direction,
 *    so <= ~120 calls: ~120 x (cycles per call), est. 10-20 % of the block.
 *    The DMA ISR's own work must fit in the rest. The D23 checks and the DWT
 *    entry-to-exit timing of the DMA ISR (which includes pre-emption) test it.
 *  - Not the other order: with the FIFO on, RX holds 8 bytes in the FIFO plus
 *    1 in RDR (RM0440 LPUART_ISR, RXFF note), so it tolerates ~9 byte times (~98 us)
 *    of delay before an overrun, less than one DSP block.
 *  - SysTick (15) only increments the ms tick, so it can always wait.
 * ------------------------------------------------------------------------ */
#define IRQ_PRIO_LPUART1    1u
#define IRQ_PRIO_DMA_ADC    2u
#define IRQ_PRIO_SYSTICK    15u

/* NVIC_SetPriorityGrouping argument for "4 bits pre-emption, 0 bits sub-priority". */
#define NVIC_GROUPING_4_0   3u

#endif /* BOARD_H */
