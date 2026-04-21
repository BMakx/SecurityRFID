/*
 * master/leds.c
 *
 * LED signals on PORT A, Active Low (writing 0 lights a diode).
 */

#include "common/config.h"
#include <util/delay.h>
#include <avr/io.h>
#include "leds.h"

void LEDs_init(void) {
    DDRA  = 0xFF;   /* all outputs */
    PORTA = 0xFF;   /* all off     */
}

void LEDs_signal_ok(void) {
    /* 6 blink cycles × 500 ms ≈ 3 s total */
    for (uint8_t i = 0; i < 6; i++) {
        PORTA = 0x00;       /* all ON  */
        _delay_ms(250);
        PORTA = 0xFF;       /* all OFF */
        _delay_ms(250);
    }
}

void LEDs_signal_reject(void) {
    /* Wave across 8 LEDs, repeat 4 times ≈ 3.2 s */
    for (uint8_t cycle = 0; cycle < 4; cycle++) {
        for (uint8_t i = 0; i < 8; i++) {
            PORTA = ~(1 << i);  /* light only the i-th LED */
            _delay_ms(100);
        }
    }
    PORTA = 0xFF;  /* leave all off when done */
}
