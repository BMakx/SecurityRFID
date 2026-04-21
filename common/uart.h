/*
 * uart.h
 *
 * Thin driver for the ATmega UART peripheral (single USART, 8N1).
 * Master calls UART_init() with no arguments (UBRR baked from config.h).
 * Slave calls UART_init_val() if it needs a runtime-computed UBRR.
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/* Initialise UART using the compile-time UBRR_VALUE from config.h */
void UART_init(void);

/* Same but accepts the UBRR at runtime – kept for Slave compatibility */
void UART_init_val(unsigned int ubrr);

void    UART_send_char(char data);
void    UART_send_string(const char *str);
uint8_t UART_char_available(void);

/*
 * Read one byte from the receive register.
 * Call only after UART_char_available() returns non-zero.
 */
char    UART_recv_char(void);

void    UART_flush(void);

/*
 * Read characters until CR/LF or until timeout_ms expires.
 * Pass timeout_ms = 0 to wait forever.
 */
void UART_read_line(char *buffer, uint8_t max_len, uint16_t timeout_ms);

#endif /* UART_H */
