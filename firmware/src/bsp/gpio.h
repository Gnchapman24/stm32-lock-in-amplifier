/*
 * gpio.h - the LED and the debug/timing pins (D28).
 */
#ifndef GPIO_H
#define GPIO_H

#include <stdbool.h>

/* LD2 (PA5) and the two debug pins PA9/PA10 as push-pull outputs, all low.
 * Also disables the UCPD dead-battery pull-downs (see gpio.c). */
void gpio_init(void);

void led_set(bool on);
bool led_get(void);

#endif /* GPIO_H */
