/*
 * sample_clock.h - TIM2 sample clock (D27, P1-9).
 *
 * TIM2 counts at SYSCLK with PSC = 0 and ARR = SYSCLK / f_s - 1, so its update
 * event (TRGO, MMS = 010) is the sample trigger for the ADC pair and DAC1 in
 * the later stages. CH3 runs PWM mode 1 at 50 % duty on PB10, so its rising
 * edge marks each sample instant on the logic analyzer, and a frequency
 * counter on PB10 checks f_s independently of the firmware.
 *
 * sample_clock_init() configures everything but starts nothing. The counter
 * is started by sample_clock_start(), which the boot sequence never calls by
 * itself: later stages arm the ADC and DMA first, then start the clock last
 * (review checklist item 6).
 */
#ifndef SAMPLE_CLOCK_H
#define SAMPLE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

void sample_clock_init(void);   /* clocks, PB10 alternate function, TIM2 config; stopped */
void sample_clock_start(void);  /* enable the counter; trigger output starts */
void sample_clock_stop(void);
bool sample_clock_running(void);

/* Sample rate this clock produces at the current SystemCoreClock. Equals
 * FS_HZ at 170 MHz; about 9.4 kHz if the clock is held at HSI16 (safe mode). */
uint32_t sample_clock_expected_hz(void);

#endif /* SAMPLE_CLOCK_H */
