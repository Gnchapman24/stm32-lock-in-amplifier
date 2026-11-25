/*
 * main.c - boot sequence and the Machine-on-Tether device table.
 *
 * Phase 1, stage A: clock, tick, serial console and heartbeat. The sampling
 * chain (TIM2, DAC, ADC, DMA) and burst capture are added as further devices
 * in later stages.
 */
#include "board.h"
#include "clock.h"
#include "fault.h"
#include "gpio.h"
#include "tick.h"
#include "serial_port.h"
#include "mot.h"
#include "telemetry.h"

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

/* Clock source, chosen at build time (CMake option LOCKIN_USE_HSE, D19). */
#if defined(LOCKIN_USE_HSE) && LOCKIN_USE_HSE
#define CLOCK_SOURCE CLOCK_SRC_HSE
#else
#define CLOCK_SOURCE CLOCK_SRC_HSI
#endif

#define CONSOLE_BAUD        921600u     /* D18, D34 */
#define STATUS_PERIOD_MS    1000u

/* ------------------------------------------------------------------------ *
 *  Heartbeat: LD2 blinks slowly when all is well, fast if something at boot
 *  went wrong (clock not at 170 MHz, or a fault before the last reset).
 * ------------------------------------------------------------------------ */
#define BLINK_OK_MS     500u
#define BLINK_ERR_MS    100u

static uint32_t s_blink_ms;
static uint32_t s_blink_next;

static void heartbeat_init(void)
{
    clock_err_t ce = clock_status()->err;
    bool trouble = (ce != CLOCK_OK && ce != CLOCK_HELD_BY_BUILD) || (fault_last() != NULL);
    s_blink_ms   = trouble ? BLINK_ERR_MS : BLINK_OK_MS;
    s_blink_next = millis();
}

static void heartbeat_poll(void)
{
    uint32_t now = millis();
    if ((int32_t)(now - s_blink_next) >= 0) {
        s_blink_next += s_blink_ms;
        led_set(!led_get());
    }
}

/* ------------------------------------------------------------------------ *
 *  Console commands
 * ------------------------------------------------------------------------ */

/* SCPI-style identification: manufacturer, model, serial number, firmware. */
static void cmd_idn(int argc, char **argv)
{
    (void)argc; (void)argv;
    const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;   /* 96-bit unique ID */
    tlm_printf("stm32-lock-in-amplifier,NUCLEO-G474RE,%08lX%08lX%08lX,%s\n",
               (unsigned long)uid[2], (unsigned long)uid[1], (unsigned long)uid[0],
               FW_VERSION);
}

static void cmd_clock(int argc, char **argv)
{
    (void)argc; (void)argv;
    const clock_status_t *c = clock_status();
    tlm_printf("sysclk=%lu Hz  requested=%s  pll_source=%s  status=%s\n",
               (unsigned long)SystemCoreClock,
               c->requested  == CLOCK_SRC_HSE ? "HSE" : "HSI",
               c->pll_source == CLOCK_SRC_HSE ? "HSE" : "HSI",
               clock_err_str(c->err));
    /* Raw read-back of the settings the 170 MHz switch depends on. */
    tlm_printf("RCC_CFGR=0x%08lX (SWS=%lu)  RCC_PLLCFGR=0x%08lX\n",
               (unsigned long)RCC->CFGR,
               (unsigned long)((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos),
               (unsigned long)RCC->PLLCFGR);
    tlm_printf("FLASH_ACR=0x%08lX (LATENCY=%lu WS)  PWR_CR5.R1MODE=%lu (0 = boost)\n",
               (unsigned long)FLASH->ACR,
               (unsigned long)(FLASH->ACR & FLASH_ACR_LATENCY_Msk),
               (unsigned long)((PWR->CR5 & PWR_CR5_R1MODE) ? 1u : 0u));
}

static void cmd_fault(int argc, char **argv)
{
    if (argc >= 2 && argv[1][0] == 't') {
        /* "fault test": deliberately crash, to check the whole path on real
         * hardware: handler, .noinit record surviving the reset, boot report,
         * fast blink and the HSI16 safe mode. UDF -> UsageFault, which is
         * disabled, so it escalates to HardFault (HFSR.FORCED set). */
        tlm_printf("faulting now...\n");
        serial_flush();
        __builtin_trap();
    }
    tlm_printf("reset cause: %s\n", fault_reset_cause());
    const fault_record_t *f = fault_last();
    if (f == NULL) {
        tlm_printf("no fault recorded before the last reset\n");
        return;
    }
    tlm_printf("hard fault: pc=0x%08lX lr=0x%08lX xpsr=0x%08lX\n",
               (unsigned long)f->pc, (unsigned long)f->lr, (unsigned long)f->xpsr);
    tlm_printf("  cfsr=0x%08lX hfsr=0x%08lX bfar=0x%08lX mmfar=0x%08lX\n",
               (unsigned long)f->cfsr, (unsigned long)f->hfsr,
               (unsigned long)f->bfar, (unsigned long)f->mmfar);
}

static void cmd_loop(int argc, char **argv)
{
    (void)argv;
    const mot_stats_t *m = mot_stats();
    uint32_t cyc_per_us = SystemCoreClock / 1000000u;
    tlm_printf("main loop: passes=%lu  longest pass=%lu cycles (%lu us)\n",
               (unsigned long)m->passes, (unsigned long)m->max_pass_cyc,
               (unsigned long)(m->max_pass_cyc / cyc_per_us));
    if (argc >= 2) {
        mot_reset_max();
        tlm_printf("longest pass reset\n");
    }
}

static void cmd_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    tlm_printf("resetting...\n");
    serial_flush();                    /* let the message leave before we go */
    NVIC_SystemReset();
}

static const tlm_cmd_t k_app_cmds[] = {
    { "*idn?", "identify: maker, model, serial, firmware", cmd_idn   },
    { "clock", "clock tree status and register read-back",  cmd_clock },
    { "fault", "fault [test]  reset cause, last hard fault", cmd_fault },
    { "loop",  "loop [reset]  main-loop pass statistics",   cmd_loop  },
    { "reset", "reboot the MCU",                            cmd_reset },
};

/* Periodic status line (console mode stream, or the "status" command). */
static void app_status(uint32_t now_ms)
{
    serial_stats_t st;
    serial_get_stats(&st);
    tlm_printf("t=%lu.%03lu sysclk=%luMHz clock=%s uart_ovr=%lu rx_drop=%lu\n",
               (unsigned long)(now_ms / 1000u), (unsigned long)(now_ms % 1000u),
               (unsigned long)(SystemCoreClock / 1000000u),
               clock_err_str(clock_status()->err),
               (unsigned long)st.uart_overrun, (unsigned long)st.rx_ring_dropped);
}

static void console_init(void)
{
    tlm_init(CONSOLE_BAUD, millis, "stm32-lock-in-amplifier " FW_VERSION);
    tlm_set_commands(k_app_cmds, sizeof k_app_cmds / sizeof k_app_cmds[0]);
    tlm_set_status_cb(app_status, STATUS_PERIOD_MS);

    tlm_event("boot: reset cause %s, sysclk %lu Hz",
              fault_reset_cause(), (unsigned long)SystemCoreClock);
    if (clock_status()->err != CLOCK_OK) {
        tlm_event("WARNING clock: %s", clock_err_str(clock_status()->err));
    }
    if (fault_last() != NULL) {
        tlm_event("WARNING hard fault before reset at pc=0x%08lX (type 'fault')",
                  (unsigned long)fault_last()->pc);
    }
}

/* ------------------------------------------------------------------------ *
 *  Device table and boot
 * ------------------------------------------------------------------------ */

static const mot_device_t k_devices[] = {
    { "console",   console_init,   tlm_poll       },
    { "heartbeat", heartbeat_init, heartbeat_poll },
};

int main(void)
{
    fault_boot_check();                         /* before anything can overwrite .noinit */
    NVIC_SetPriorityGrouping(NVIC_GROUPING_4_0);
    gpio_init();

#if defined(LOCKIN_NO_PLL) && LOCKIN_NO_PLL
    clock_hold_hsi(CLOCK_HELD_BY_BUILD);        /* bring-up: everything at 16 MHz */
#else
    if (fault_last() != NULL) {
        /* Safe mode: if the crash was caused by the clock switch (or by
         * anything soon after it), trying again would just reset again, and
         * each reset would overwrite the record. Stay at 16 MHz for this boot
         * so the console can report the fault; the next reset retries. */
        clock_hold_hsi(CLOCK_HELD_AFTER_FAULT);
    } else {
        clock_init(CLOCK_SOURCE);
    }
#endif
    tick_init();                                /* after the clock: SysTick reload uses it */

    mot_run(k_devices, sizeof k_devices / sizeof k_devices[0]);
}
