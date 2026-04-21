# master/Makefile
#
# Build the Master firmware for ATmega16 / ATmega32.
# Adjust MCU and PROGRAMMER as needed.

MCU       = atmega16
F_CPU     = 11059200UL
BAUD      = 19200UL
PROGRAMMER = usbasp

CC        = avr-gcc
OBJCOPY   = avr-objcopy
AVRDUDE   = avrdude

CFLAGS    = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra \
            -I.

# All object files needed for the Master binary
OBJ = main.o \
      leds.o \
      bt_master.o \
      rfid.o \
      common/uart.o \
      common/lcd.o \
      common/btm222.o

TARGET = master

.PHONY: all flash clean

all: $(TARGET).hex

$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

flash: $(TARGET).hex
	$(AVRDUDE) -c $(PROGRAMMER) -p $(MCU) -U flash:w:$(TARGET).hex

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).hex
