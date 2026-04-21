/*
 * master/rfid.c
 *
 * Minimal PN532 driver using ATmega16 hardware SPI.
 *
 * Only the two commands we need are implemented:
 *   1. SAMConfiguration  – put the PN532 into normal (active) mode
 *   2. InListPassiveTarget – scan for one ISO 14443-A card and return UID
 *
 * ── PN532 SPI framing notes ─────────────────────────────────
 *  • Data is transferred LSB first (DORD=1 in SPCR).
 *  • SPI mode 0 (CPOL=0, CPHA=0).
 *  • Every transaction starts by asserting SS low.
 *  • After sending a command frame the host must poll the status byte
 *    (0x01 written, 0x01 read back means "ready") before reading the ACK
 *    frame, and again before reading the response frame.
 *  • Frame structure:  PREAMBLE 00 | START 00 FF | LEN | LCS | TFI | DATA | DCS | POSTAMBLE 00
 * ────────────────────────────────────────────────────────────
 */

#include "common/config.h"
#include <util/delay.h>
#include <avr/io.h>
#include <string.h>
#include "rfid.h"

/* ── SPI / SS pin definitions ───────────────────────────────── */
#define SPI_DDR   DDRB
#define SPI_PORT  PORTB
#define SPI_MOSI  PB5
#define SPI_MISO  PB6
#define SPI_SCK   PB7
#define SPI_SS    PB4   /* chip select for PN532, active low */

#define SS_LOW()   (SPI_PORT &= ~(1 << SPI_SS))
#define SS_HIGH()  (SPI_PORT |=  (1 << SPI_SS))

/* ── PN532 SPI data-direction indicator byte ────────────────── */
#define PN532_SPI_DATAWRITE  0x01
#define PN532_SPI_STATREAD   0x02
#define PN532_SPI_DATAREAD   0x03

/* ── PN532 frame constants ──────────────────────────────────── */
#define PN532_PREAMBLE   0x00
#define PN532_STARTCODE1 0x00
#define PN532_STARTCODE2 0xFF
#define PN532_POSTAMBLE  0x00
#define PN532_TFI_HOST   0xD4   /* host → PN532 */
#define PN532_TFI_PN532  0xD5   /* PN532 → host */

/* ── PN532 command codes ────────────────────────────────────── */
#define PN532_CMD_SAMCONFIGURATION   0x14
#define PN532_CMD_INLISTPASSIVETARGET 0x4A

/* ── Low-level SPI ──────────────────────────────────────────── */

static void spi_init(void) {
    /* MOSI, SCK, SS as outputs; MISO as input */
    SPI_DDR |= (1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << SPI_SS);
    SPI_DDR &= ~(1 << SPI_MISO);
    SS_HIGH();   /* deselect */

    /*
     * SPI master, LSB first (DORD=1), mode 0 (CPOL=0 CPHA=0),
     * clock = F_CPU / 16  ≈ 690 kHz at 11.0592 MHz — well within PN532's 5 MHz limit.
     */
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << DORD) | (1 << SPR0);
    SPSR = 0;
}

static uint8_t spi_transfer(uint8_t byte) {
    SPDR = byte;
    while (!(SPSR & (1 << SPIF))) { }
    return SPDR;
}

/* ── PN532 frame helpers ────────────────────────────────────── */

/*
 * Wait until the PN532 SPI status register reports it has data ready.
 * Times out after ~200 ms.  Returns 1 if ready, 0 on timeout.
 */
static uint8_t pn532_wait_ready(void) {
    for (uint16_t i = 0; i < 200; i++) {
        SS_LOW();
        spi_transfer(PN532_SPI_STATREAD);
        uint8_t status = spi_transfer(0x00);
        SS_HIGH();
        if (status == 0x01) return 1;
        _delay_ms(1);
    }
    return 0;
}

/*
 * Send a command frame to the PN532.
 * cmd_buf[0] must be the PN532 command byte; remaining bytes are parameters.
 */
static void pn532_send_frame(const uint8_t *cmd_buf, uint8_t cmd_len) {
    uint8_t lcs, dcs;
    uint8_t len = cmd_len + 1;   /* +1 for TFI byte */

    lcs = (uint8_t)(~len + 1);   /* length checksum: LEN + LCS = 0x00 */

    /* data checksum covers TFI + all data bytes */
    dcs = PN532_TFI_HOST;
    for (uint8_t i = 0; i < cmd_len; i++) dcs += cmd_buf[i];
    dcs = (uint8_t)(~dcs + 1);

    SS_LOW();
    _delay_us(2);

    spi_transfer(PN532_SPI_DATAWRITE);
    spi_transfer(PN532_PREAMBLE);
    spi_transfer(PN532_STARTCODE1);
    spi_transfer(PN532_STARTCODE2);
    spi_transfer(len);
    spi_transfer(lcs);
    spi_transfer(PN532_TFI_HOST);
    for (uint8_t i = 0; i < cmd_len; i++) spi_transfer(cmd_buf[i]);
    spi_transfer(dcs);
    spi_transfer(PN532_POSTAMBLE);

    SS_HIGH();
}

/*
 * Read and validate the 6-byte ACK frame from the PN532.
 * Returns 1 if the ACK is valid (00 00 FF 00 FF 00), 0 otherwise.
 */
static uint8_t pn532_read_ack(void) {
    static const uint8_t expected_ack[6] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t ack[6];

    SS_LOW();
    _delay_us(2);
    spi_transfer(PN532_SPI_DATAREAD);
    for (uint8_t i = 0; i < 6; i++) ack[i] = spi_transfer(0x00);
    SS_HIGH();

    return (memcmp(ack, expected_ack, 6) == 0) ? 1 : 0;
}

/*
 * Read a response frame into resp_buf (max resp_max bytes of payload).
 * Returns the number of payload bytes (after TFI), or 0 on error.
 */
static uint8_t pn532_read_response(uint8_t *resp_buf, uint8_t resp_max) {
    uint8_t len, lcs, tfi;

    SS_LOW();
    _delay_us(2);
    spi_transfer(PN532_SPI_DATAREAD);

    /* consume preamble + start code */
    spi_transfer(0x00);   /* PREAMBLE */
    spi_transfer(0x00);   /* STARTCODE1 */
    spi_transfer(0x00);   /* STARTCODE2 */

    len = spi_transfer(0x00);   /* LEN  */
    lcs = spi_transfer(0x00);   /* LCS  */

    if ((uint8_t)(len + lcs) != 0x00) {
        SS_HIGH();
        return 0;
    }

    tfi = spi_transfer(0x00);   /* TFI */
    if (tfi != PN532_TFI_PN532) {
        SS_HIGH();
        return 0;
    }

    /* DATA bytes = LEN - 1 (TFI is included in LEN) */
    uint8_t data_len = len - 1;
    uint8_t read_len = (data_len < resp_max) ? data_len : resp_max;

    for (uint8_t i = 0; i < read_len; i++) resp_buf[i] = spi_transfer(0x00);
    /* drain any bytes we chose not to store */
    for (uint8_t i = read_len; i < data_len; i++) spi_transfer(0x00);

    spi_transfer(0x00);   /* DCS */
    spi_transfer(0x00);   /* POSTAMBLE */

    SS_HIGH();
    return data_len;
}

/* ── PN532 command wrappers ─────────────────────────────────── */

/*
 * SAMConfiguration: set PN532 to normal mode (SAM unused).
 * Must be called once before scanning.
 */
static void pn532_sam_config(void) {
    /* SAMConfiguration: Mode=Normal(0x01), Timeout=0, IRQ=0 */
    const uint8_t cmd[] = {PN532_CMD_SAMCONFIGURATION, 0x01, 0x00, 0x00};
    pn532_send_frame(cmd, sizeof(cmd));

    if (!pn532_wait_ready()) return;
    pn532_read_ack();
    if (!pn532_wait_ready()) return;
    uint8_t resp[2];
    pn532_read_response(resp, sizeof(resp));
}

/* ── Public API ─────────────────────────────────────────────── */

void RFID_init(void) {
    spi_init();
    _delay_ms(50);   /* PN532 power-on stabilisation time */

    /*
     * Wake the PN532 from power-down / reset state:
     * pull SS low for ≥1 ms, then release.  The chip needs to see a
     * falling edge on SS before it accepts SPI traffic.
     */
    SS_LOW();
    _delay_ms(2);
    SS_HIGH();
    _delay_ms(10);

    pn532_sam_config();
}

uint8_t RFID_read_uid(char *str_buf) {
    static const char hex[] = "0123456789ABCDEF";

    /*
     * InListPassiveTarget: MaxTg=1 card, BrTy=0x00 (ISO 14443-A / Mifare).
     * This command does NOT block in firmware — the PN532 will return a
     * "no target" response immediately if no card is present.
     * We loop here in software so the caller gets a clean blocking API.
     */
    const uint8_t cmd[] = {PN532_CMD_INLISTPASSIVETARGET, 0x01, 0x00};
    uint8_t resp[20];
    uint8_t resp_len;

    str_buf[0] = '\0';

    while (1) {
        pn532_send_frame(cmd, sizeof(cmd));

        if (!pn532_wait_ready()) continue;
        if (!pn532_read_ack())   continue;
        if (!pn532_wait_ready()) continue;

        resp_len = pn532_read_response(resp, sizeof(resp));
        if (resp_len < 7) continue;   /* too short to contain a UID */

        /*
         * InListPassiveTarget response layout (ISO 14443-A):
         *   resp[0]  = command-code echo (0x4B)
         *   resp[1]  = NbTg (number of targets found)
         *   resp[2]  = Tg   (target number, always 1 here)
         *   resp[3]  = ATQA byte 1
         *   resp[4]  = ATQA byte 2
         *   resp[5]  = SAK
         *   resp[6]  = NIDLength (UID byte count)
         *   resp[7…] = UID bytes
         */
        if (resp[1] == 0) continue;   /* no target found, try again */

        uint8_t uid_len = resp[6];
        if (uid_len == 0 || uid_len > 10) continue;   /* sanity check */

        /* Convert raw UID bytes to uppercase hex string */
        uint8_t out = 0;
        for (uint8_t i = 0; i < uid_len; i++) {
            str_buf[out++] = hex[(resp[7 + i] >> 4) & 0x0F];
            str_buf[out++] = hex[ resp[7 + i]       & 0x0F];
        }
        str_buf[out] = '\0';

        return uid_len;
    }
}
