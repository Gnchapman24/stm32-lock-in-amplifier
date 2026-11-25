/*
 * serial_port.c - see serial_port.h
 */
#include "serial_port.h"
#include "ringbuf.h"
#include "board.h"
#include "tick.h"

/* LPUART kernel clock = HSI16 (selected below) */
#define SERIAL_KERNEL_HZ    HSI_HZ

/* With FIFOEN = 1 the same bits change meaning (RM0440 section 37.7, LPUART
 * registers): RXNEIE becomes RXFNEIE, TXEIE becomes TXFNFIE, RXNE becomes RXFNE
 * ("RX FIFO not empty") and TXE becomes TXFNF ("TX FIFO not full"). */
#define CR1_UE       USART_CR1_UE
#define CR1_RE       USART_CR1_RE
#define CR1_TE       USART_CR1_TE
#define CR1_RXFNEIE  USART_CR1_RXNEIE_RXFNEIE
#define CR1_TXFNFIE  USART_CR1_TXEIE_TXFNFIE
#define CR1_FIFOEN   USART_CR1_FIFOEN

#define ISR_PE       USART_ISR_PE
#define ISR_FE       USART_ISR_FE
#define ISR_NE       USART_ISR_NE
#define ISR_ORE      USART_ISR_ORE
#define ISR_RXFNE    USART_ISR_RXNE_RXFNE
#define ISR_TC       USART_ISR_TC
#define ISR_TXFNF    USART_ISR_TXE_TXFNF
#define ISR_ERRORS   (ISR_PE | ISR_FE | ISR_NE | ISR_ORE)

/* The ISR moves at most this many bytes per direction per call: one FIFO's
 * worth. It bounds the ISR's length, which the DMA ISR's deadline relies on
 * (see the priority comment in board.h). */
#define FIFO_DEPTH   8u

_Static_assert((SERIAL_RX_BUF_SIZE & (SERIAL_RX_BUF_SIZE - 1u)) == 0u,
               "SERIAL_RX_BUF_SIZE must be a power of two");
_Static_assert((SERIAL_TX_BUF_SIZE & (SERIAL_TX_BUF_SIZE - 1u)) == 0u,
               "SERIAL_TX_BUF_SIZE must be a power of two");

static uint8_t s_rx_storage[SERIAL_RX_BUF_SIZE];
static uint8_t s_tx_storage[SERIAL_TX_BUF_SIZE];
static rb_t    s_rx;
static rb_t    s_tx;

/* Each counter has a single writer (the ISR), so plain increments are safe. */
static volatile uint32_t c_rx_bytes, c_tx_bytes;
static volatile uint32_t c_ore, c_fe, c_ne, c_pe;
static volatile uint32_t c_isr_calls, c_isr_max_cyc;   /* written by the ISR only */

/* ------------------------------------------------------------------------ */

void serial_init(uint32_t baud)
{
    rb_init(&s_rx, s_rx_storage, SERIAL_RX_BUF_SIZE);
    rb_init(&s_tx, s_tx_storage, SERIAL_TX_BUF_SIZE);

    /* Kernel clock: keep HSI16 running (also in Stop mode) and select it for LPUART1 */
    RCC->CR |= RCC_CR_HSION | RCC_CR_HSIKERON;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_LPUART1SEL_Msk) | RCC_CCIPR_LPUART1SEL_1;

    /* Peripheral clocks */
    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->APB1ENR2;                 /* read-back: let the clock enables settle */
    (void)RCC->AHB2ENR;

    /* PA2 = LPUART1_TX, PA3 = LPUART1_RX, both AF12; pull-up on RX */
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((0xFu << 8) | (0xFu << 12)))
                  | (12u << 8) | (12u << 12);
    GPIOA->MODER  = (GPIOA->MODER  & ~((3u << 4) | (3u << 6)))
                  | (2u << 4) | (2u << 6);
    GPIOA->PUPDR  = (GPIOA->PUPDR  & ~(3u << 6)) | (1u << 6);

    /* LPUART1: 8N1, no prescaler. FIFOEN may only be written while UE = 0. */
    LPUART1->CR1   = 0u;
    LPUART1->CR2   = 0u;
    LPUART1->CR3   = 0u;                 /* FIFO thresholds unused: we interrupt on not-empty / not-full */
    LPUART1->PRESC = 0u;
    /* LPUART baud: BRR = 256 * fck / baud, valid range 0x300 .. 0xFFFFF.
     * 921600 baud from 16 MHz: BRR = 4444, actual 921692 baud, error 0.01 %. */
    LPUART1->BRR   = (uint32_t)(((uint64_t)256u * SERIAL_KERNEL_HZ + baud / 2u) / baud);
    LPUART1->ICR   = ISR_ERRORS;         /* clear any stale error flags */

    LPUART1->CR1 = CR1_FIFOEN;           /* FIFO mode first, still disabled */
    LPUART1->CR1 = CR1_FIFOEN | CR1_UE | CR1_RE | CR1_TE | CR1_RXFNEIE;

    while ((LPUART1->ISR & ISR_RXFNE) != 0u) {
        (void)LPUART1->RDR;              /* drop anything stale */
    }

    NVIC_SetPriority(LPUART1_IRQn, IRQ_PRIO_LPUART1);
    NVIC_ClearPendingIRQ(LPUART1_IRQn);
    NVIC_EnableIRQ(LPUART1_IRQn);
}

/* ------------------------------------------------------------------------ */

int serial_read_byte(void)
{
    uint8_t b;
    return rb_get(&s_rx, &b) ? (int)b : -1;
}

size_t serial_rx_available(void)
{
    return rb_count(&s_rx);
}

static inline void tx_irq_enable(void)
{
    /* The ISR also read-modify-writes CR1 (to clear TXFNFIE), so protect the RMW.
     * Save/restore PRIMASK rather than unconditionally re-enabling interrupts. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    LPUART1->CR1 |= CR1_TXFNFIE;
    __set_PRIMASK(primask);
}

size_t serial_write(const uint8_t *data, size_t len)
{
    size_t n = 0;
    while (n < len && rb_put(&s_tx, data[n])) {
        n++;
    }
    if (n != 0u) {
        tx_irq_enable();                 /* ISR drains the ring */
    }
    return n;
}

size_t serial_tx_free(void)
{
    return rb_free(&s_tx);
}

bool serial_tx_idle(void)
{
    /* TC is set only once the TX FIFO is empty and the last stop bit has gone. */
    return (rb_count(&s_tx) == 0u) && ((LPUART1->ISR & ISR_TC) != 0u);
}

void serial_flush(void)
{
    while (!serial_tx_idle()) { }
}

void serial_get_stats(serial_stats_t *out)
{
    out->rx_bytes        = c_rx_bytes;
    out->tx_bytes        = c_tx_bytes;
    out->rx_ring_dropped = s_rx.dropped;
    out->tx_ring_dropped = s_tx.dropped;
    out->uart_overrun    = c_ore;
    out->framing_err     = c_fe;
    out->noise_err       = c_ne;
    out->parity_err      = c_pe;
    out->isr_calls       = c_isr_calls;
    out->isr_max_cyc     = c_isr_max_cyc;
}

void serial_reset_isr_max(void)
{
    c_isr_max_cyc = 0u;
}

/* ------------------------------------------------------------------------ */

static void lpuart1_service(void);

void LPUART1_IRQHandler(void)
{
    uint32_t t0 = cycles();
    lpuart1_service();
    uint32_t dt = cycles() - t0;
    c_isr_calls++;
    if (dt > c_isr_max_cyc) { c_isr_max_cyc = dt; }
}

static void lpuart1_service(void)
{
    uint32_t isr = LPUART1->ISR;

    /* Overrun: the RX FIFO was full when another byte finished arriving.
     * That byte is lost; the FIFO contents are still good. */
    if (isr & ISR_ORE) {
        c_ore++;
        LPUART1->ICR = ISR_ORE;
    }

    /* Receive: drain the FIFO. Each byte is stored in the RX FIFO with its own
     * error bits, and PE/FE/NE describe "the character in RDR", i.e. the one
     * the next RDR read returns (RM0440, LPUART_ISR bit descriptions). So:
     * read ISR, clear any error flags BEFORE reading RDR (clearing after
     * would wipe the flags of the next byte), then read the byte and drop it
     * if it was corrupted. */
    for (unsigned n = FIFO_DEPTH; n != 0u && (isr & ISR_RXFNE); n--) {
        uint32_t err = isr & (ISR_PE | ISR_FE | ISR_NE);
        if (err != 0u) {
            LPUART1->ICR = err;
        }
        uint8_t b = (uint8_t)LPUART1->RDR;
        if (err != 0u) {
            if (err & ISR_FE) { c_fe++; }
            if (err & ISR_NE) { c_ne++; }
            if (err & ISR_PE) { c_pe++; }
        } else {
            c_rx_bytes++;
            (void)rb_put(&s_rx, b);      /* drop counted inside rb_put */
        }
        isr = LPUART1->ISR;
    }

    /* Transmit: top the FIFO up from the ring, or switch the interrupt off. */
    if (LPUART1->CR1 & CR1_TXFNFIE) {
        for (unsigned n = FIFO_DEPTH; n != 0u && (isr & ISR_TXFNF); n--) {
            uint8_t b;
            if (!rb_get(&s_tx, &b)) {
                LPUART1->CR1 &= ~CR1_TXFNFIE;
                break;
            }
            LPUART1->TDR = b;
            c_tx_bytes++;
            isr = LPUART1->ISR;
        }
    }
}
