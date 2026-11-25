/*
 * serial_port.h - interrupt-driven LPUART1 driver for the Nucleo-G474RE
 *                 ST-LINK virtual COM port (PA2 = TX, PA3 = RX, AF12).
 *
 *  - Bare-metal CMSIS register access, no HAL dependency.
 *  - LPUART1 is clocked from HSI16, so the baud rate does not depend on SYSCLK
 *    (the port works even if the PLL doesn't).
 *  - The 8-byte hardware FIFOs are on (D22):
 *      RX: interrupt on "RX FIFO not empty"; the ISR drains the whole FIFO
 *          into the RX ring. A threshold interrupt isn't used because it
 *          would need a receiver timeout to flush a short command, and
 *          LPUART1 has none (RM0440 Table 370).
 *      TX: interrupt on "TX FIFO not full"; the ISR refills the FIFO from the
 *          TX ring until either is exhausted.
 *  - Nothing here ever blocks, except serial_flush().
 *
 * Reused from the earlier telemetry project (D18); the FIFO handling is new.
 *
 * Threading rule: call the serial_* functions from ONE context (the main loop).
 */
#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Buffer sizes - must be powers of two. */
#ifndef SERIAL_RX_BUF_SIZE
#define SERIAL_RX_BUF_SIZE  256u
#endif
#ifndef SERIAL_TX_BUF_SIZE
#define SERIAL_TX_BUF_SIZE  2048u
#endif

typedef struct {
    uint32_t rx_bytes;         /* good bytes received                          */
    uint32_t tx_bytes;         /* bytes handed to the transmitter              */
    uint32_t rx_ring_dropped;  /* RX ring full: main loop is not keeping up    */
    uint32_t tx_ring_dropped;  /* TX ring full: output produced too fast       */
    uint32_t uart_overrun;     /* hardware overrun (ISR latency too long)      */
    uint32_t framing_err;      /* wrong baud rate / break / noise on the line  */
    uint32_t noise_err;
    uint32_t parity_err;
    uint32_t isr_calls;        /* LPUART1 ISR invocations                      */
    uint32_t isr_max_cyc;      /* longest ISR body, CPU cycles, excl. the
                                  exception entry/exit (~22 cycles, not counted) */
} serial_stats_t;

/* Configure clocks, pins, LPUART1 (8N1), NVIC.  e.g. serial_init(115200). */
void   serial_init(uint32_t baud);

/* Non-blocking.  Returns the byte (0..255) or -1 if nothing is waiting. */
int    serial_read_byte(void);
size_t serial_rx_available(void);

/* Non-blocking.  Queues as many bytes as fit and returns how many were taken. */
size_t serial_write(const uint8_t *data, size_t len);
size_t serial_tx_free(void);

/* True once the TX ring is empty AND the last stop bit has left the shifter. */
bool   serial_tx_idle(void);

/* Blocks until serial_tx_idle().  Use before a reset or in a fault handler;
 * interrupts must be enabled. */
void   serial_flush(void);

void   serial_get_stats(serial_stats_t *out);
void   serial_reset_isr_max(void);

#endif /* SERIAL_PORT_H */
