/*
 * master/rfid.h
 *
 * MFRC522 (RFID-RC522 breakout) over hardware SPI on ATmega16.
 *
 * Wiring (typical RC522 board → ATmega16):
 *
 *   RC522 pin   ATmega16      Notes
 *   ─────────   ──────────    ─────────────────────────────────────
 *   SDA (NSS)   PB4           SPI chip select, active low
 *   SCK         PB7           SPI clock
 *   MOSI        PB5           Host → RC522
 *   MISO        PB6           RC522 → host
 *   RST         PD2           Reset / power-down (NRSTPD), active low
 *   3.3V        3.3V          Many boards are 3.3 V logic; level-shift
 *                             MOSI/SCK/RST/SS if the MCU runs at 5 V
 *   GND         GND
 *
 * SPI: MSB first, mode 0 (CPOL=0, CPHA=0), within MFRC522 limits.
 */

#ifndef RFID_H
#define RFID_H

#include <stdint.h>

/* 10-byte UID × 2 hex digits + NUL */
#define UID_STR_MAX_LEN  21

void RFID_init(void);

/*
 * Block until a Type A PICC is detected, then fill str_buf with the UID as
 * uppercase hex (no separators). str_buf must hold at least UID_STR_MAX_LEN
 * bytes. Returns UID length in bytes (4, 7, or 10), or 0 on persistent error.
 */
uint8_t RFID_read_uid(char *str_buf);

#endif /* RFID_H */
