/*
 * master/rfid.h
 *
 * PN532 NFC/RFID reader driver over hardware SPI (ATmega16).
 *
 * ── Wiring ──────────────────────────────────────────────────
 *  PN532 pin   ATmega16 pin   Notes
 *  ─────────   ────────────   ──────────────────────────────
 *  SCK         PB7            Hardware SPI clock
 *  MOSI        PB5            Data host → PN532 (LSB first)
 *  MISO        PB6            Data PN532 → host
 *  SS (/CS)    PB4            Active low chip select
 *  GND         GND
 *  VCC (3.3V)  3.3V supply    *NOT* 5V — use a level-shifter
 *                             or a PN532 breakout with on-board
 *                             3.3 V regulator
 *
 * ── Interface mode selection ────────────────────────────────
 *  The PN532 breakout must be set to SPI mode:
 *    SEL0 = LOW, SEL1 = HIGH  (or DIP switch 1=OFF, 2=ON)
 * ────────────────────────────────────────────────────────────
 */

#ifndef RFID_H
#define RFID_H

#include <stdint.h>

/*
 * Maximum length of the UID hex string (10-byte UID × 2 chars + NUL).
 * In practice you will see 4-byte UIDs (MIFARE Classic, 8 chars) or
 * 7-byte UIDs (MIFARE Ultralight / DESFire, 14 chars).
 */
#define UID_STR_MAX_LEN  21

/*
 * Initialise SPI hardware and configure the PN532 in normal mode
 * (SAMConfiguration command).  Must be called once at startup before
 * any RFID_read_uid() call.
 */
void RFID_init(void);

/*
 * Block until an ISO 14443-A card enters the RF field, then write the
 * card's UID as an uppercase hex string (no separators) into str_buf.
 * str_buf must point to at least UID_STR_MAX_LEN bytes.
 *
 * Returns the UID byte-length (4, 7, or 10) on success, 0 on error.
 *
 * Example: a MIFARE Classic card with UID 0x04 0xAB 0x12 0x34 will
 * produce the string "04AB1234\0".
 */
uint8_t RFID_read_uid(char *str_buf);

#endif /* RFID_H */
