/*
 * slave/bt_slave.h
 *
 * Slave-side BTM-222 lifecycle: initialisation and initial handshake.
 */

#ifndef BT_SLAVE_H
#define BT_SLAVE_H

/* Configure BTM-222 as connectable Slave role */
void BTM222_init_slave(void);

/*
 * Wait indefinitely for PING from Master, reply with READY.
 * Blocks until the exchange is complete.
 */
void BTM222_handshake_slave(void);

#endif /* BT_SLAVE_H */
