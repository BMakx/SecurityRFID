/*
 * slave/bt_slave.c
 *
 * BTM-222 setup for Slave role.
 * No ATZ here because the Slave just needs to be discoverable/connectable;
 * the Master dials in and the SPP link forms automatically.
 */

#include "../common/config.h"
#include <util/delay.h>
#include "../common/uart.h"
#include "../common/btm222.h"
#include "bt_slave.h"

void BTM222_init_slave(void) {
    _delay_ms(1000);
    UART_flush();

    BT_send_command("AT");        _delay_ms(120);
    BT_send_command("ATE0");      _delay_ms(120);  /* echo off            */
    BT_send_command("ATQ0");      _delay_ms(120);  /* result codes on     */
    BT_send_command("ATN=SLAVE"); _delay_ms(120);  /* device name         */
    BT_send_command("ATR=1");     _delay_ms(120);  /* accept connections  */
    BT_send_command("ATP=1234");  _delay_ms(120);  /* pairing PIN         */
}

void BTM222_handshake_slave(void) {
    /* Keep listening until Master sends PING, then confirm with READY */
    while (1) {
        if (BT_wait_keyword("PING", 1000)) {
            UART_send_string("READY\r\n");
            return;
        }
    }
}
