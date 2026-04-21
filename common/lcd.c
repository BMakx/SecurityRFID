/*
 * lcd.c
 *
 * HD44780-compatible LCD in 4-bit mode, wired to PORT C.
 * Initialisation sequence follows the datasheet power-on flow.
 */

#include "config.h"
#include <util/delay.h>
#include <avr/io.h>
#include "lcd.h"

/* Pulse the Enable line: bring E high then low */
static inline void lcd_pulse_E(void) {
    PORTC |=  (1 << 5);   /* E high */
    _delay_ms(1);
    PORTC &= ~(1 << 5);   /* E low  */
}

void LCD_send_4bits(uint8_t bits, uint8_t rs) {
    uint8_t port = 0;
    if (rs)  port |= (1 << 6);   /* RS line */
             port |= (1 << 5);   /* E  line – will be lowered by pulse */
             port |= (bits >> 4);
    PORTC = port;
    _delay_ms(1);
    PORTC &= ~(1 << 5);          /* E low */
}

void LCD_send_byte(uint8_t byte, uint8_t rs) {
    LCD_send_4bits(byte & 0xF0, rs);   /* high nibble first */
    LCD_send_4bits(byte << 4,   rs);   /* low  nibble       */
}

void LCD_command(uint8_t command) {
    LCD_send_byte(command, 0);
    _delay_ms(2);
}

void LCD_init(void) {
    DDRC  = 0xFF;
    PORTC = 0x00;
    _delay_ms(40);

    /* --- Power-on reset sequence (8-bit mode attempts, then switch to 4-bit) --- */
    PORTC = 0b00100011; PORTC = 0b00000011; _delay_ms(5);
    PORTC = 0b00100011; PORTC = 0b00000011; _delay_us(100);
    PORTC = 0b00100011; PORTC = 0b00000011; _delay_us(50);

    /* Switch to 4-bit interface */
    PORTC = 0b00100010; PORTC = 0b00000010;

    /* Function set: 4-bit, 2 lines, 5x8 font */
    PORTC = 0b00101100; PORTC = 0b00001100; _delay_ms(50);
    PORTC = 0b00100000; PORTC = 0b00000000;
    PORTC = 0b00101100; PORTC = 0b00001100; _delay_ms(50);

    /* Display OFF */
    PORTC = 0b00100000; PORTC = 0b00000000;
    PORTC = 0b00100001; PORTC = 0b00000001; _delay_ms(100);

    /* Display clear */
    PORTC = 0b00100001; PORTC = 0b00000001;
    PORTC = 0b00101000; PORTC = 0b00001000; _delay_ms(50);

    /* Display ON, cursor OFF, blink OFF */
    PORTC = 0b01100000; PORTC = 0b01000100;
    PORTC = 0b01100001; PORTC = 0b01000001; _delay_ms(50);
}

void LCD_write_string(const char *s) {
    while (*s) LCD_send_byte(*s++, 1);
}

void LCD_clear(void) {
    LCD_command(0x01);
    _delay_ms(2);
}

void LCD_set_cursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0 ? 0x80 : 0xC0) + col;
    LCD_command(address);
}
