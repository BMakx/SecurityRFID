/*
 * slave/buttons.h
 *
 * Two tactile buttons on PORT B, both Active Low with internal pull-ups.
 *
 *   PB0 – ACCEPT (BTN_OK)
 *   PB1 – REJECT (BTN_NO)
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

#define BTN_OK  0   /* PB0 */
#define BTN_NO  1   /* PB1 */

/* Configure PB0 and PB1 as inputs with pull-ups enabled */
void Buttons_init(void);

/*
 * Blocking wait: returns BTN_OK or BTN_NO once a button is pressed and
 * released. A 50 ms debounce delay is applied.
 */
uint8_t Buttons_wait_press(void);

#endif /* BUTTONS_H */
