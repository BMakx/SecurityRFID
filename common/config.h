/*
 * config.h
 *
 * Central place for hardware constants shared by both Master and Slave.
 * Change F_CPU or BAUD here and every module picks it up automatically.
 */

#ifndef CONFIG_H
#define CONFIG_H

#define F_CPU   11059200UL
#define BAUD    19200UL

/* UBRR for async 8N1 at the baud rate above */
#define UBRR_VALUE  ((F_CPU / 16UL / BAUD) - 1)

/*
 * LCD is wired to PORT C on both devices:
 *
 *  PC7   PC6  PC5  PC4  PC3  PC2  PC1  PC0
 *  RW(0) RS   E    L    D7   D6   D5   D4
 *
 * RW is permanently pulled low (write-only mode).
 */

#endif /* CONFIG_H */
