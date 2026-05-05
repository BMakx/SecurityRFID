/*
 * uart.c
 *
 * ATmega UART driver – 8N1, polling (no interrupts).
 */

#include "config.h"  /* pulls F_CPU before util/delay.h */
#include <util/delay.h>
#include <avr/io.h>
#include "uart.h"

void UART_init(void) {
    UART_init_val((unsigned int)UBRR_VALUE);
}

void UART_init_val(unsigned int ubrr) {
    UBRRH = (unsigned char)(ubrr >> 8);
    UBRRL = (unsigned char)ubrr;
    UCSRB = (1 << RXEN) | (1 << TXEN);
    /* URSEL must be set when writing UCSRC on ATmega16/32 */
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void UART_send_char(char data) {
    while (!(UCSRA & (1 << UDRE))) { }
    UDR = data;
}

void UART_send_string(const char *str) {
    while (*str) UART_send_char(*str++);
}

uint8_t UART_char_available(void) {
    return (UCSRA & (1 << RXC)) ? 1 : 0;
}

char UART_recv_char(void) {
    return (char)UDR;
}

void UART_flush(void) {
    while (UART_char_available()) { (void)UART_recv_char(); }
}

void UART_read_line(char *buffer, uint8_t max_len, uint16_t timeout_ms) {
    uint8_t i = 0;
    buffer[0] = '\0';

    /* timeout_ms == 0 means infinite wait */
    while (timeout_ms > 0 || timeout_ms == 0) {
        if (UART_char_available()) {
            char c = UART_recv_char();
            if (c == '\r' || c == '\n') {
                if (i > 0) { buffer[i] = '\0'; return; }
            } else if (i < max_len - 1) {
                buffer[i++] = c;
            }
        } else {
            _delay_ms(1);
            if (timeout_ms > 0) timeout_ms--;
        }
    }
    buffer[i] = '\0';
}
