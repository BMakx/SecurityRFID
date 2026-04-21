/*
 * master/bt_master.h
 *
 * Master-side BTM-222 lifecycle: initialisation, connection, handshake.
 */

#ifndef BT_MASTER_H
#define BT_MASTER_H

#include <stdint.h>

/* Configure BTM-222 as Master role and reboot the module */
void BTM222_init_master(void);

/* Dial the slave at the given Bluetooth address (ATD<addr>) */
void BTM222_connect(const char *addr);

/*
 * Perform PING/READY handshake.
 * Returns 1 on success, 0 if all attempts are exhausted.
 */
uint8_t BTM222_handshake_master(void);

#endif /* BT_MASTER_H */
