/*
 * master/bt_master.c
 *
 * BTM-222 setup for Master role.
 * The module is configured, then ATZ reboots it so settings take effect.
 */

#include "common/config.h"
#include <util/delay.h>
#include "common/uart.h"
#include "common/btm222.h"
#include "bt_master.h"

void BTM222_init_master(void) {
    _delay_ms(1000);
    UART_flush();

    BT_send_command("AT");       _delay_ms(200);
    BT_send_command("ATE0");     _delay_ms(200);  /* echo off        */
    BT_send_command("ATQ0");     _delay_ms(500);  /* result codes on */
    BT_send_command("ATN=MASTER"); _delay_ms(500);
    BT_send_command("ATR=0");    _delay_ms(500);  /* no auto-connect */
    BT_send_command("ATP=1234"); _delay_ms(500);  /* pairing PIN     */
    BT_send_command("ATZ");      _delay_ms(3000); /* soft reset      */

    UART_flush();
}

void BTM222_connect(const char *addr) {
    UART_flush();
    UART_send_string("ATD");
    UART_send_string(addr);
    UART_send_char('\r');
    _delay_ms(8000);   /* wait for SPP link to establish */
}

uint8_t BTM222_handshake_master(void) {
    uint8_t tries = 8;
    while (tries--) {
        UART_send_string("PING\r\n");
        if (BT_wait_keyword("READY", 1500)) return 1;
    }
    return 0;
}
