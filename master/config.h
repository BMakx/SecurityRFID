/*
 * master/config.h
 *
 * Master-specific compile-time settings.
 * Generic hardware constants live in common/config.h.
 */

#ifndef MASTER_CONFIG_H
#define MASTER_CONFIG_H

/* Bluetooth address of the Slave device */
#define SLAVE_BT_ADDR   "00126f9e3e5c"

/* How many ms to pause between authorisation cycles */
#define CYCLE_PAUSE_MS  5000

#endif /* MASTER_CONFIG_H */
