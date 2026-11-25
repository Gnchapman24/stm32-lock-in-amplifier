/*
 * gpio.c - see gpio.h
 */
#include "gpio.h"
#include "board.h"

/* MODER / OSPEEDR are 2 bits per pin. */
#define MODE_OUTPUT     1u
#define SPEED_HIGH      2u      /* "high speed": fast enough for clean ns-scale edges */

static void make_output(GPIO_TypeDef *port, uint32_t pin, uint32_t speed)
{
    PIN_LOW(port, pin);                                   /* defined level before driving */
    port->OTYPER  &= ~(1u << pin);                        /* push-pull */
    port->OSPEEDR  = (port->OSPEEDR & ~(3u << (2u * pin))) | (speed << (2u * pin));
    port->PUPDR   &= ~(3u << (2u * pin));                 /* no pull */
    port->MODER    = (port->MODER   & ~(3u << (2u * pin))) | (MODE_OUTPUT << (2u * pin));
}

void gpio_init(void)
{
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->AHB2ENR;                     /* read back so the clock enables have taken effect */
    (void)RCC->APB1ENR1;

    /* PA9/PA10 double as the UCPD1 dead-battery inputs: driving them high
     * enables a 5.1 kOhm pull-down on PB6/PB4 (DS12288 Table 12, note 6).
     * Those pins are unused, but turn the feature off so it can't surprise us. */
    PWR->CR3 |= PWR_CR3_UCPD_DBDIS;

    make_output(LED_PORT,     LED_PIN,     0u);           /* low speed is plenty for an LED */
    make_output(DBG_ISR_PORT, DBG_ISR_PIN, SPEED_HIGH);
    make_output(DBG_AUX_PORT, DBG_AUX_PIN, SPEED_HIGH);
}

void led_set(bool on)
{
    if (on) { PIN_HIGH(LED_PORT, LED_PIN); }
    else    { PIN_LOW(LED_PORT, LED_PIN);  }
}

bool led_get(void)
{
    return ((LED_PORT->ODR >> LED_PIN) & 1u) != 0u;
}
