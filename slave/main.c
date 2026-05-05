/*
 * slave/main.c
 *
 * Entry point for the Slave (operator terminal) device.
 *
 * Flow:
 *   1. Initialise peripherals.
 *   2. Configure BTM-222 as Slave and wait for Master's handshake.
 *   3. Loop: receive ID from Master → prompt operator → send decision.
 */

#include "common/config.h"   /* F_CPU before util/delay.h */
#include <util/delay.h>
#include <string.h>
#include "common/lcd.h"
#include "common/uart.h"
#include "bt_slave.h"
#include "buttons.h"

int main(void) {
    LCD_init();
    UART_init();
    Buttons_init();

    LCD_clear();
    LCD_write_string("SLAVE START");

    BTM222_init_slave();

    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_string("Czekam na polacz");

    BTM222_handshake_slave();

    char rx_buf[20];

    /* --- Main authorisation loop --- */
    while (1) {
        LCD_clear();
        LCD_write_string("Czekam na ID...");

        UART_read_line(rx_buf, sizeof(rx_buf), 0);   /* block until new line */

        /* Filter out handshake artefacts that may linger in the stream */
        if (strlen(rx_buf) == 0
            || strcmp(rx_buf, "PING")  == 0
            || strcmp(rx_buf, "READY") == 0) {
            continue;
        }

        /* Display the received ID and prompt the operator */
        LCD_clear();
        LCD_set_cursor(0, 0);
        LCD_write_string(rx_buf);
        LCD_set_cursor(1, 0);
        LCD_write_string("1:Zgoda 2:Odrzuc");

        uint8_t btn = Buttons_wait_press();

        if (btn == BTN_OK) {
            UART_send_string("OK\r\n");
            LCD_clear();
            LCD_write_string("ZAAKCEPTOWANO");
        } else {
            UART_send_string("ODRZUCONO\r\n");
            LCD_clear();
            LCD_write_string("ODRZUCONO");
        }

        _delay_ms(3000);   /* let the operator read the confirmation */
    }

    return 0;   /* unreachable */
}
