/*
 * lcd.h
 *
 * 4-bit HD44780 LCD driver mapped to PORT C.
 *
 * Pinout (identical on both Master and Slave boards):
 *   PC7   PC6  PC5  PC4  PC3  PC2  PC1  PC0
 *   RW(0) RS   E    BL   D7   D6   D5   D4
 */

#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void LCD_init(void);
void LCD_clear(void);
void LCD_set_cursor(uint8_t row, uint8_t col);
void LCD_write_string(const char *s);

/* Low-level helpers exposed so higher layers can roll custom sequences */
void LCD_command(uint8_t command);
void LCD_send_byte(uint8_t byte, uint8_t rs);
void LCD_send_4bits(uint8_t bits, uint8_t rs);

#endif /* LCD_H */
