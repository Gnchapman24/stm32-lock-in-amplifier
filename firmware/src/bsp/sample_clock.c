/*
 * sample_clock.c - see sample_clock.h
 */
#include "sample_clock.h"
#include "board.h"

/* OC3M = 110 selects PWM mode 1: output high while CNT < CCR3. */
#define OC3M_PWM_MODE1  (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1)

void sample_clock_init(void)
{
    /* Clocks: the timer, and the GPIO port for PB10. */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;
    (void)RCC->APB1ENR1;                 /* read back so the enables have taken effect */
    (void)RCC->AHB2ENR;

    /* PB10: alternate function, push-pull, high speed, no pull. */
    SAMPLE_PWM_PORT->MODER   = (SAMPLE_PWM_PORT->MODER & ~(3u << (2u * SAMPLE_PWM_PIN)))
                               | (2u << (2u * SAMPLE_PWM_PIN));
    SAMPLE_PWM_PORT->OTYPER &= ~(1u << SAMPLE_PWM_PIN);
    SAMPLE_PWM_PORT->OSPEEDR = (SAMPLE_PWM_PORT->OSPEEDR & ~(3u << (2u * SAMPLE_PWM_PIN)))
                               | (2u << (2u * SAMPLE_PWM_PIN));
    SAMPLE_PWM_PORT->PUPDR  &= ~(3u << (2u * SAMPLE_PWM_PIN));
    /* AFR[1] holds pins 8..15, four bits each. */
    SAMPLE_PWM_PORT->AFR[1] = (SAMPLE_PWM_PORT->AFR[1] & ~(0xFu << (4u * (SAMPLE_PWM_PIN - 8u))))
                              | (SAMPLE_PWM_AF << (4u * (SAMPLE_PWM_PIN - 8u)));

    /* Counter: stopped, up-counting, auto-reload preloaded. */
    SAMPLE_TIM->CR1   = TIM_CR1_ARPE;
    SAMPLE_TIM->PSC   = 0u;              /* count at SYSCLK (APB1 prescaler is 1, see clock.c) */
    SAMPLE_TIM->ARR   = SAMPLE_ARR;

    /* TRGO = update event (MMS = 010), so the ADC and DAC get one pulse per sample. */
    SAMPLE_TIM->CR2   = TIM_CR2_MMS_1;

    /* Channel 3: PWM mode 1 with preload, output enabled. */
    SAMPLE_TIM->CCR3  = SAMPLE_CCR3;
    SAMPLE_TIM->CCMR2 = OC3M_PWM_MODE1 | TIM_CCMR2_OC3PE;
    SAMPLE_TIM->CCER |= TIM_CCER_CC3E;

    /* Load PSC/ARR/CCR3 now, rather than at the first update. */
    SAMPLE_TIM->EGR   = TIM_EGR_UG;
    SAMPLE_TIM->SR    = 0u;              /* clear the update flag that UG just set */
}

void sample_clock_start(void)
{
    SAMPLE_TIM->CR1 |= TIM_CR1_CEN;
}

void sample_clock_stop(void)
{
    SAMPLE_TIM->CR1 &= ~TIM_CR1_CEN;
}

bool sample_clock_running(void)
{
    return (SAMPLE_TIM->CR1 & TIM_CR1_CEN) != 0u;
}

uint32_t sample_clock_expected_hz(void)
{
    return SystemCoreClock / (SAMPLE_TIM->PSC + 1u) / (SAMPLE_TIM->ARR + 1u);
}
