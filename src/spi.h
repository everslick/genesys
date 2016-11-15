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

#ifndef _SPI_H_
#define _SPI_H_

#include <Arduino.h>
#include <stdint.h>

#include "gpio.h"

// using the HW SPI implementation from the Arduino libraries
// produces incorrect readings from the ADE chip.
// if PHA is 1 it loses the LSB (least significant BIT) and if PHA is 0 it
// shifts the received byte to the left one bit. POL 1 (clock inactive high) is
// not implemented in the ESP SPI hardware anyway.

// this software bitbanging SPI implementation
// is capable of doing ~2MBit/s transfers but a
// probably faster HW implementation can be found here:
// https://github.com/MetalPhreak/ESP8266_SPI_Driver
// http://d.av.id.au/blog/

static bool busy = false;

static inline bool ICACHE_RAM_ATTR spi_busy(void) {
  return (busy);
}

static bool spi_init(void) {
  pinMode(GPIO_MISO,    INPUT);
  pinMode(GPIO_MOSI,   OUTPUT);
  pinMode(GPIO_SCLK,   OUTPUT);
  pinMode(GPIO_SS,     OUTPUT);

  digitalWrite(GPIO_SS,  HIGH);
  digitalWrite(GPIO_SCLK, LOW);

  return (true);
}

static inline void ICACHE_RAM_ATTR spi_select(void) {
  busy = true;
  GPOC = (1<<GPIO_SS);
}

static inline void ICACHE_RAM_ATTR spi_deselect(void) {
  GPOS = (1<<GPIO_SS);
  busy = false;
}

static inline void ICACHE_RAM_ATTR spi_read_delay(void) {
  delayMicroseconds(4);
}

static inline uint8_t ICACHE_RAM_ATTR spi_read(void) {
  uint8_t b = 0;
 
  for (int i=7; i>=0; i--) {
    GPOS = (1<<GPIO_SCLK);
    b |= GPIP(GPIO_MISO)<<i;
    GPOC = (1<<GPIO_SCLK);
  }

  return b;
}

static inline uint8_t ICACHE_RAM_ATTR spi_read(uint8_t &sum) {
  uint8_t b = 0;
 
  for (int i=7; i>=0; i--) {
    GPOS = (1<<GPIO_SCLK);
    b |= GPIP(GPIO_MISO)<<i;
    GPOC = (1<<GPIO_SCLK);
  }

  sum += ((b>>7)&1) + ((b>>6)&1) + ((b>>5)&1) + ((b>>4)&1) +
         ((b>>3)&1) + ((b>>2)&1) + ((b>>1)&1) + ((b>>0)&1);

  return b;
}

static inline void ICACHE_RAM_ATTR spi_write(uint8_t data) {  
  for (int i=0; i<8; i++) {
    (data & 0x80) ? GPOS = (1<<GPIO_MOSI) : GPOC = (1<<GPIO_MOSI);
    GPOS = (1<<GPIO_SCLK); data<<=1; GPOC = (1<<GPIO_SCLK);
  }
}

#endif // _SPI_H_
