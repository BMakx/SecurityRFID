/*
 * btm222.h
 *
 * Low-level BTM-222 Bluetooth module helpers shared by both Master and Slave.
 * Higher-level init / handshake logic lives in each device's own bt_master.c
 * or bt_slave.c to keep their differing AT sequences separate.
 */

#ifndef BTM222_H
#define BTM222_H

#include <stdint.h>

/* Send an AT command followed by '\r' */
void BT_send_command(const char *cmd);

/*
 * Scan incoming bytes for the first occurrence of keyword.
 * Returns 1 if found within timeout_ms, 0 on timeout.
 */
uint8_t BT_wait_keyword(const char *keyword, uint16_t timeout_ms);

#endif /* BTM222_H */
