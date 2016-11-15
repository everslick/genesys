/*
    This file is part of Genesys.

    Genesys is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Genesys is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Genesys.  If not, see <http://www.gnu.org/licenses/>.

    Copyright (C) 2016 Clemens Kirchgatterer <clemens@1541.org>.
*/

#include <Arduino.h>

#include "system.h"
#include "log.h"
#include "i2c.h"

#include "at24c32.h"

// I2C slave address  
#define EEPROM_ADDRESS  0x57

// EEPROM size
#define EEPROM_SIZE     4096

// EEPROM page size
#define EEPROM_PAGE       32

// write cycle time (10ms)
#define EEPROM_DELAY      12

// magic string for detection
#define EEPROM_MAGIC "ESPADE"

static uint32_t last_write = 0;

static void wait_until_not_busy(void) {
  // the EEPROM might still be busy storing the last page write,
  // so we wait until at least 10ms have past (write cycle time)

  while (millis() - last_write < EEPROM_DELAY) system_yield();
}

static void eeprom_is_now_busy(void) {
  last_write = millis();
}

static void write(uint16_t addr, const void *data, uint16_t off, uint16_t len) {
  uint8_t buf[len + 2];

  buf[0] = addr >> 8;
  buf[1] = addr & 0xff;
  memcpy(&buf[2], data + off, len);

  wait_until_not_busy();
  i2c_write(EEPROM_ADDRESS, buf, len + 2);
  eeprom_is_now_busy();
}

static bool check_magic(uint16_t addr, const char *str, uint16_t len) {
  char buf[len];

  at24c32_read(addr, buf, len);

  return (!strncmp(buf, str, len));
}

static bool write_magic(uint16_t addr, const char *str, uint16_t len) {
  char zero[EEPROM_PAGE / 2];

  log_print("PROM: formatting AT24C32 eeprom\r\n");

  // clear whole EEPROM
  memset(zero, 0, sizeof (zero));
  for (int i=0; i<EEPROM_SIZE; i+=sizeof (zero)) {
    write(i, zero, 0, sizeof (zero));
  }

  // write magic string
  at24c32_write(addr, str, len);

  return (check_magic(addr, str, len));
}

void at24c32_write(uint16_t addr, const void *data, uint16_t len) {
  int off = 0;

  // we can only write to one page at a time AND according to the
  // datasheet, the AT24C32 can only receive 32 bytes in one go,
  // thus we send 2 address bytes + a maximum of 30 data bytes

  while (len > 0) {
    int bytes = min(min(len, EEPROM_PAGE-2), EEPROM_PAGE-(addr%EEPROM_PAGE));

    write(addr, data, off, bytes);

    addr += bytes;
    off  += bytes;
    len  -= bytes;
  }
}

void at24c32_read(uint16_t addr, void *data, uint16_t len) {
  uint8_t buf[2] = { addr >> 8, addr & 0xFF };

  wait_until_not_busy();

  i2c_write(EEPROM_ADDRESS, buf, 2);
  i2c_read(EEPROM_ADDRESS, (uint8_t *)data, len);
}

bool at24c32_init(void) {
  String magic = F(EEPROM_MAGIC);

  const char *str = magic.c_str();
  uint16_t bytes = magic.length();
  uint16_t addr = EEPROM_SIZE - bytes;

  if (check_magic(addr, str, bytes)) return (true);
  if (write_magic(addr, str, bytes)) return (true);

  return (false);
}
