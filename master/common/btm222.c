/*
 * btm222.c
 *
 * BTM-222 low-level helpers – AT command sender and keyword scanner.
 */

#include "config.h"
#include <util/delay.h>
#include <string.h>
#include "uart.h"
#include "btm222.h"

void BT_send_command(const char *cmd) {
    UART_send_string(cmd);
    UART_send_char('\r');
}

uint8_t BT_wait_keyword(const char *keyword, uint16_t timeout_ms) {
    uint8_t idx     = 0;
    uint8_t key_len = (uint8_t)strlen(keyword);
    uint16_t t      = timeout_ms;

    while (t > 0) {
        if (UART_char_available()) {
            char c = UART_recv_char();
            if (c == keyword[idx]) {
                idx++;
                if (idx == key_len) return 1;
            } else {
                /* restart match; re-test current char against start */
                idx = (c == keyword[0]) ? 1 : 0;
            }
        } else {
            _delay_ms(1);
            t--;
        }
    }
    return 0;
}
