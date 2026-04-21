/*
 * master/leds.h
 *
 * LED feedback signals on PORT A (PA0–PA7, Active Low).
 */

#ifndef LEDS_H
#define LEDS_H

void LEDs_init(void);

/* Flash all LEDs 6 × (250 ms ON / 250 ms OFF) ≈ 3 s */
void LEDs_signal_ok(void);

/* Chase single LED across all 8 positions, 4 full passes ≈ 3.2 s */
void LEDs_signal_reject(void);

#endif /* LEDS_H */
