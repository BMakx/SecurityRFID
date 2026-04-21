/*
 * slave/buttons.c
 *
 * Debounced blocking button read for PB0 (ACCEPT) and PB1 (REJECT).
 */

#include "../common/config.h"
#include <util/delay.h>
#include <avr/io.h>
#include "buttons.h"

void Buttons_init(void) {
    /* Set PB0 and PB1 as inputs */
    DDRB &= ~((1 << BTN_OK) | (1 << BTN_NO));
    /* Enable internal pull-up resistors */
    PORTB |= (1 << BTN_OK) | (1 << BTN_NO);
}

uint8_t Buttons_wait_press(void) {
    while (1) {
        if (!(PINB & (1 << BTN_OK))) {
            _delay_ms(50);                        /* debounce */
            while (!(PINB & (1 << BTN_OK))) { }   /* wait for release */
            return BTN_OK;
        }
        if (!(PINB & (1 << BTN_NO))) {
            _delay_ms(50);
            while (!(PINB & (1 << BTN_NO))) { }
            return BTN_NO;
        }
    }
}
