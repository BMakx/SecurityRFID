/*
 * master/main.c
 *
 * Entry point for the Master (guard station) device.
 *
 * Flow:
 *   1. Initialise peripherals (LCD, UART, LEDs, MFRC522 reader).
 *   2. Configure BTM-222 as Master and connect to Slave.
 *   3. Loop: wait for card → send UID → wait for Slave decision → signal result.
 */

#include "../common/config.h"   /* F_CPU must be defined before util/delay.h */
#include <util/delay.h>
#include <string.h>
#include "../common/lcd.h"
#include "../common/uart.h"
#include "bt_master.h"
#include "leds.h"
#include "rfid.h"
#include "config.h"             /* SLAVE_BT_ADDR, CYCLE_PAUSE_MS */

int main(void) {
    LEDs_init();
    LCD_init();
    UART_init();
    RFID_init();

    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_string("MASTER START");

    BTM222_init_master();

    LCD_clear();
    LCD_write_string("Laczenie...");
    BTM222_connect(SLAVE_BT_ADDR);

    if (!BTM222_handshake_master()) {
        LCD_clear();
        LCD_write_string("BLAD POLACZENIA");
        while (1) { }   /* halt – nothing to do without a connection */
    }

    /* --- Main authorisation loop --- */
    while (1) {
        LCD_clear();
        LCD_set_cursor(0, 0);
        LCD_write_string("Przybliz karte");
        LCD_set_cursor(1, 0);
        LCD_write_string("do czytnika...");

        char uid_str[UID_STR_MAX_LEN];
        RFID_read_uid(uid_str);

        LCD_clear();
        LCD_set_cursor(0, 0);
        LCD_write_string("UID:");
        LCD_set_cursor(0, 4);
        LCD_write_string(uid_str);

        LCD_set_cursor(1, 0);
        LCD_write_string("Wysylam...");

        _delay_ms(200);
        UART_send_string(uid_str);
        UART_send_string("\r\n");

        LCD_set_cursor(1, 0);
        LCD_write_string("Czekam decyzje");

        char response[16];
        UART_read_line(response, sizeof(response), 0);  /* block until answer */

        LCD_clear();
        if (strcmp(response, "OK") == 0) {
            LCD_write_string("DOSTEP PRZYZNANY");
            LEDs_signal_ok();
        } else if (strcmp(response, "ODRZUCONO") == 0) {
            LCD_write_string("BRAK DOSTEPU");
            LEDs_signal_reject();
        }

        LCD_set_cursor(1, 0);
        LCD_write_string("Restart za 5s...");
        _delay_ms(CYCLE_PAUSE_MS);
    }

    return 0;   /* unreachable */
}
