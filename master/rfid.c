/*
 * master/rfid.c
 *
 * MFRC522 (RFID-RC522) on ATmega16 hardware SPI — ISO 14443 Type A UID read.
 * Register map and command flow follow the MFRC522 datasheet; behaviour is
 * aligned with the widely used Arduino MFRC522 library (miguelbalboa/rfid).
 */

#include "common/config.h"
#include <util/delay.h>
#include <avr/io.h>
#include <string.h>
#include "rfid.h"

/* SPI: NSS on PB4, RC522 expects MSB-first SPI mode 0 */
#define SPI_DDR    DDRB
#define SPI_PORT   PORTB
#define SPI_MOSI   PB5
#define SPI_MISO   PB6
#define SPI_SCK    PB7
#define SPI_NSS    PB4

#define NSS_LOW()   (SPI_PORT &= ~(1 << SPI_NSS))
#define NSS_HIGH()  (SPI_PORT |=  (1 << SPI_NSS))

/* NRSTPD — many RC522 boards label this RST (active low) */
#define RST_DDR   DDRD
#define RST_PORT  PORTD
#define RST_PIN   PD2

#define RST_LOW()   (RST_PORT &= ~(1 << RST_PIN))
#define RST_HIGH()  (RST_PORT |=  (1 << RST_PIN))

/* MFRC522 register addresses (SPI: addr << 1, bit0 = 0), same as Arduino enum */
enum {
    Reg_CommandReg    = 0x01 << 1,
    Reg_ComIEnReg     = 0x02 << 1,
    Reg_DivIEnReg     = 0x03 << 1,
    Reg_ComIrqReg     = 0x04 << 1,
    Reg_DivIrqReg     = 0x05 << 1,
    Reg_ErrorReg      = 0x06 << 1,
    Reg_Status1Reg    = 0x07 << 1,
    Reg_Status2Reg    = 0x08 << 1,
    Reg_FIFODataReg   = 0x09 << 1,
    Reg_FIFOLevelReg  = 0x0A << 1,
    Reg_WaterLevelReg = 0x0B << 1,
    Reg_ControlReg    = 0x0C << 1,
    Reg_BitFramingReg = 0x0D << 1,
    Reg_CollReg       = 0x0E << 1,
    Reg_ModeReg       = 0x11 << 1,
    Reg_TxModeReg     = 0x12 << 1,
    Reg_RxModeReg     = 0x13 << 1,
    Reg_TxControlReg  = 0x14 << 1,
    Reg_TxASKReg      = 0x15 << 1,
    Reg_TModeReg      = 0x2A << 1,
    Reg_TPrescalerReg = 0x2B << 1,
    Reg_TReloadRegH   = 0x2C << 1,
    Reg_TReloadRegL   = 0x2D << 1,
    Reg_CRCResultRegH = 0x21 << 1,
    Reg_CRCResultRegL = 0x22 << 1,
    Reg_ModWidthReg   = 0x24 << 1,
    Reg_VersionReg    = 0x37 << 1,
};

enum {
    PCD_Idle       = 0x00,
    PCD_CalcCRC    = 0x03,
    PCD_Transceive = 0x0C,
    PCD_SoftReset  = 0x0F,
};

enum {
    PICC_CMD_REQA   = 0x26,
    PICC_CMD_CT     = 0x88,
    PICC_CMD_SEL_CL1 = 0x93,
    PICC_CMD_SEL_CL2 = 0x95,
    PICC_CMD_SEL_CL3 = 0x97,
};

enum {
    ST_OK = 0,
    ST_ERROR,
    ST_COLLISION,
    ST_TIMEOUT,
    ST_NO_ROOM,
    ST_INTERNAL,
    ST_INVALID,
    ST_CRC_WRONG,
};

typedef struct {
    uint8_t size;
    uint8_t uidByte[10];
    uint8_t sak;
} Uid_t;

static uint8_t spi_transfer(uint8_t b) {
    SPDR = b;
    while (!(SPSR & (1 << SPIF))) { }
    return SPDR;
}

static void spi_init(void) {
    SPI_DDR |= (1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << SPI_NSS);
    SPI_DDR &= ~(1 << SPI_MISO);
    NSS_HIGH();

    /* Master, MSB first, mode 0, F_CPU/16 */
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
    SPSR = 0;
}

static void reg_write(uint8_t reg, uint8_t val) {
    NSS_LOW();
    spi_transfer(reg & 0x7E);
    spi_transfer(val);
    NSS_HIGH();
}

static uint8_t reg_read(uint8_t reg) {
    uint8_t v;
    NSS_LOW();
    spi_transfer(0x80 | (reg & 0x7E));
    v = spi_transfer(0);
    NSS_HIGH();
    return v;
}

static void reg_set_mask(uint8_t reg, uint8_t mask) {
    reg_write(reg, reg_read(reg) | mask);
}

static void reg_clear_mask(uint8_t reg, uint8_t mask) {
    reg_write(reg, reg_read(reg) & (uint8_t)~mask);
}

static void reg_write_bytes(uint8_t reg, const uint8_t *data, uint8_t len) {
    NSS_LOW();
    spi_transfer(reg & 0x7E);
    for (uint8_t i = 0; i < len; i++) spi_transfer(data[i]);
    NSS_HIGH();
}

static void fifo_write(const uint8_t *data, uint8_t len) {
    reg_write_bytes(Reg_FIFODataReg, data, len);
}

static void pcd_reset(void) {
    reg_write(Reg_CommandReg, PCD_SoftReset);
    for (uint8_t i = 0; i < 3; i++) {
        _delay_ms(50);
        if (!(reg_read(Reg_CommandReg) & (1 << 4))) break;
    }
}

static uint8_t pcd_calc_crc(const uint8_t *data, uint8_t length, uint8_t *result) {
    reg_write(Reg_CommandReg, PCD_Idle);
    reg_write(Reg_DivIrqReg, 0x04);
    reg_write(Reg_FIFOLevelReg, 0x80);
    fifo_write(data, length);
    reg_write(Reg_CommandReg, PCD_CalcCRC);

    for (uint16_t i = 0; i < 2000; i++) {
        uint8_t n = reg_read(Reg_DivIrqReg);
        if (n & 0x04) {
            reg_write(Reg_CommandReg, PCD_Idle);
            result[0] = reg_read(Reg_CRCResultRegL);
            result[1] = reg_read(Reg_CRCResultRegH);
            return ST_OK;
        }
        _delay_us(50);
    }
    return ST_TIMEOUT;
}

static uint8_t pcd_communicate_with_picc(
    uint8_t command,
    uint8_t wait_irq,
    uint8_t *send_data,
    uint8_t send_len,
    uint8_t *back_data,
    uint8_t *back_len,
    uint8_t *valid_bits,
    uint8_t rx_align,
    uint8_t check_crc
) {
    uint8_t tx_last_bits = valid_bits ? *valid_bits : 0;
    uint8_t bit_framing = (uint8_t)((rx_align << 4) + tx_last_bits);

    reg_write(Reg_CommandReg, PCD_Idle);
    reg_write(Reg_ComIrqReg, 0x7F);
    reg_write(Reg_FIFOLevelReg, 0x80);
    fifo_write(send_data, send_len);
    reg_write(Reg_BitFramingReg, bit_framing);
    reg_write(Reg_CommandReg, command);
    if (command == PCD_Transceive) reg_set_mask(Reg_BitFramingReg, 0x80);

    for (uint16_t i = 0; i < 800; i++) {
        uint8_t n = reg_read(Reg_ComIrqReg);
        if (n & wait_irq) break;
        if (n & 0x01) return ST_TIMEOUT;
        _delay_us(50);
    }

    uint8_t err = reg_read(Reg_ErrorReg);
    if (err & 0x13) return ST_ERROR;

    uint8_t vb = 0;
    if (back_data && back_len) {
        uint8_t n = reg_read(Reg_FIFOLevelReg);
        if (n > *back_len) return ST_NO_ROOM;
        *back_len = n;

        if (n == 0) {
            vb = reg_read(Reg_ControlReg) & 0x07;
            if (valid_bits) *valid_bits = vb;
        } else {
            uint8_t addr = (uint8_t)(0x80 | (Reg_FIFODataReg & 0x7E));
            uint8_t idx = 0;
            NSS_LOW();
            spi_transfer(addr);
            if (rx_align) {
                uint8_t mask = (uint8_t)((0xFFu << rx_align) & 0xFFu);
                uint8_t val = spi_transfer(addr);
                back_data[0] = (uint8_t)((back_data[0] & ~mask) | (val & mask));
                idx = 1;
            }
            while (idx < (uint8_t)(n - 1)) {
                back_data[idx] = spi_transfer(addr);
                idx++;
            }
            back_data[idx] = spi_transfer(0);
            NSS_HIGH();
            vb = reg_read(Reg_ControlReg) & 0x07;
            if (valid_bits) *valid_bits = vb;
        }
    }

    if (err & 0x08) return ST_COLLISION;

    if (back_data && back_len && check_crc) {
        if (*back_len == 1 && vb == 4) return ST_ERROR;
        if (*back_len < 2 || vb != 0) return ST_CRC_WRONG;
        uint8_t crcbuf[2];
        if (pcd_calc_crc(back_data, (uint8_t)(*back_len - 2), crcbuf) != ST_OK) return ST_CRC_WRONG;
        if (back_data[*back_len - 2] != crcbuf[0] || back_data[*back_len - 1] != crcbuf[1]) return ST_CRC_WRONG;
    }

    return ST_OK;
}

static uint8_t pcd_transceive(
    uint8_t *send_data,
    uint8_t send_len,
    uint8_t *back_data,
    uint8_t *back_len,
    uint8_t *valid_bits,
    uint8_t rx_align,
    uint8_t check_crc
) {
    return pcd_communicate_with_picc(
        PCD_Transceive, 0x30, send_data, send_len, back_data, back_len, valid_bits, rx_align, check_crc
    );
}

static uint8_t picc_reqa_or_wupa(uint8_t cmd, uint8_t *atqa, uint8_t *atqa_size) {
    uint8_t valid_bits = 7;
    if (*atqa_size < 2) return ST_NO_ROOM;
    reg_clear_mask(Reg_CollReg, 0x80);
    uint8_t st = pcd_transceive(&cmd, 1, atqa, atqa_size, &valid_bits, 0, 0);
    if (st != ST_OK) return st;
    if (*atqa_size != 2 || valid_bits != 0) return ST_ERROR;
    return ST_OK;
}

static uint8_t picc_select(Uid_t *uid, uint8_t valid_bits_in) {
    uint8_t uid_complete = 0;
    uint8_t cascade_level = 1;
    uint8_t valid_bits = valid_bits_in;

    if (valid_bits > 80) return ST_INVALID;

    reg_clear_mask(Reg_CollReg, 0x80);

    while (!uid_complete) {
        uint8_t buffer[9];
        uint8_t uid_index;
        uint8_t use_cascade_tag;

        switch (cascade_level) {
            case 1:
                buffer[0] = PICC_CMD_SEL_CL1;
                uid_index = 0;
                use_cascade_tag = (uint8_t)(valid_bits && uid->size > 4);
                break;
            case 2:
                buffer[0] = PICC_CMD_SEL_CL2;
                uid_index = 3;
                use_cascade_tag = (uint8_t)(valid_bits && uid->size > 7);
                break;
            case 3:
                buffer[0] = PICC_CMD_SEL_CL3;
                uid_index = 6;
                use_cascade_tag = 0;
                break;
            default:
                return ST_INTERNAL;
        }

        int16_t current_level_known_bits = (int16_t)valid_bits - (int16_t)(8 * uid_index);
        if (current_level_known_bits < 0) current_level_known_bits = 0;

        uint8_t index = 2;
        if (use_cascade_tag) buffer[index++] = PICC_CMD_CT;

        if (current_level_known_bits) {
            uint8_t bytes_to_copy = (uint8_t)(current_level_known_bits / 8 + (current_level_known_bits % 8 ? 1 : 0));
            uint8_t max_bytes = use_cascade_tag ? 3u : 4u;
            if (bytes_to_copy > max_bytes) bytes_to_copy = max_bytes;
            for (uint8_t c = 0; c < bytes_to_copy; c++) buffer[index++] = uid->uidByte[uid_index + c];
        }
        if (use_cascade_tag) current_level_known_bits += 8;

        uint8_t select_done = 0;
        uint8_t tx_last_bits = 0;
        uint8_t *response_buffer = NULL;
        uint8_t response_length = 0;

        while (!select_done) {
            uint8_t buffer_used;
            uint8_t st;

            if (current_level_known_bits >= 32) {
                buffer[1] = 0x70;
                buffer[6] = (uint8_t)(buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5]);
                st = pcd_calc_crc(buffer, 7, &buffer[7]);
                if (st != ST_OK) return st;
                tx_last_bits = 0;
                buffer_used = 9;
                response_buffer = &buffer[6];
                response_length = 3;
            } else {
                tx_last_bits = (uint8_t)(current_level_known_bits % 8);
                uint8_t whole = (uint8_t)(current_level_known_bits / 8);
                index = (uint8_t)(2 + whole);
                buffer[1] = (uint8_t)((index << 4) + tx_last_bits);
                buffer_used = (uint8_t)(index + (tx_last_bits ? 1u : 0u));
                response_buffer = &buffer[index];
                response_length = (uint8_t)(sizeof(buffer) - index);
            }

            uint8_t rx_align = tx_last_bits;
            reg_write(Reg_BitFramingReg, (uint8_t)((rx_align << 4) + tx_last_bits));

            st = pcd_transceive(buffer, buffer_used, response_buffer, &response_length, &tx_last_bits, rx_align, 0);

            if (st == ST_COLLISION) {
                uint8_t coll = reg_read(Reg_CollReg);
                if (coll & 0x20) return ST_COLLISION;
                uint8_t collision_pos = coll & 0x1F;
                if (collision_pos == 0) collision_pos = 32;
                if (collision_pos <= (uint8_t)current_level_known_bits) return ST_INTERNAL;
                current_level_known_bits = collision_pos;
                uint8_t frac = (uint8_t)(current_level_known_bits % 8);
                uint8_t check_bit = (uint8_t)((current_level_known_bits - 1) % 8);
                index = (uint8_t)(1 + (current_level_known_bits / 8) + (frac ? 1u : 0u));
                buffer[index] |= (uint8_t)(1u << check_bit);
            } else if (st != ST_OK) {
                return st;
            } else {
                if (current_level_known_bits >= 32) {
                    select_done = 1;
                } else {
                    current_level_known_bits = 32;
                }
            }
        }

        index = (uint8_t)(buffer[2] == PICC_CMD_CT ? 3 : 2);
        uint8_t bytes_to_copy = (uint8_t)(buffer[2] == PICC_CMD_CT ? 3 : 4);
        for (uint8_t c = 0; c < bytes_to_copy; c++) uid->uidByte[uid_index + c] = buffer[index++];

        if (response_buffer == NULL) return ST_INTERNAL;
        if (response_length != 3 || tx_last_bits != 0) return ST_ERROR;
        if (pcd_calc_crc(response_buffer, 1, &buffer[2]) != ST_OK) return ST_ERROR;
        if (buffer[2] != response_buffer[1] || buffer[3] != response_buffer[2]) return ST_CRC_WRONG;

        if (response_buffer[0] & 0x04) {
            cascade_level++;
        } else {
            uid_complete = 1;
            uid->sak = response_buffer[0];
        }
    }

    uid->size = (uint8_t)(3 * cascade_level + 1);
    return ST_OK;
}

static void pcd_antenna_on(void) {
    uint8_t v = reg_read(Reg_TxControlReg);
    if ((v & 0x03) != 0x03) reg_write(Reg_TxControlReg, (uint8_t)(v | 0x03));
}

static void mfrc522_init_hw(void) {
    RST_DDR |= (1 << RST_PIN);
    RST_LOW();
    _delay_us(2);
    RST_HIGH();
    _delay_ms(50);

    reg_write(Reg_TxModeReg, 0x00);
    reg_write(Reg_RxModeReg, 0x00);
    reg_write(Reg_ModWidthReg, 0x26);

    reg_write(Reg_TModeReg, 0x80);
    reg_write(Reg_TPrescalerReg, 0xA9);
    reg_write(Reg_TReloadRegH, 0x03);
    reg_write(Reg_TReloadRegL, 0xE8);

    reg_write(Reg_TxASKReg, 0x40);
    reg_write(Reg_ModeReg, 0x3D);
    pcd_antenna_on();
}

void RFID_init(void) {
    spi_init();
    _delay_ms(10);
    pcd_reset();
    mfrc522_init_hw();
    (void)reg_read(Reg_VersionReg);
}

uint8_t RFID_read_uid(char *str_buf) {
    static const char hex[] = "0123456789ABCDEF";

    str_buf[0] = '\0';

    while (1) {
        uint8_t atqa[2];
        uint8_t atqa_len = sizeof(atqa);

        if (picc_reqa_or_wupa(PICC_CMD_REQA, atqa, &atqa_len) != ST_OK) {
            _delay_ms(5);
            continue;
        }

        Uid_t uid;
        memset(&uid, 0, sizeof(uid));

        if (picc_select(&uid, 0) != ST_OK) {
            _delay_ms(5);
            continue;
        }

        if (uid.size == 0 || uid.size > 10) continue;

        uint8_t out = 0;
        for (uint8_t i = 0; i < uid.size; i++) {
            str_buf[out++] = hex[(uid.uidByte[i] >> 4) & 0x0F];
            str_buf[out++] = hex[uid.uidByte[i] & 0x0F];
        }
        str_buf[out] = '\0';
        return uid.size;
    }
}
