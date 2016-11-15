/*
  i2c.cpp - Software I2C library for esp8266

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <Arduino.h>

#include "gpio.h"

#include "i2c.h"

#define SDA_LOW()  (GPES = (1 << GPIO_SDA))
#define SDA_HIGH() (GPEC = (1 << GPIO_SDA))
#define SDA_READ() ((GPI & (1 << GPIO_SDA)) != 0)
#define SCL_LOW()  (GPES = (1 << GPIO_SCL))
#define SCL_HIGH() (GPEC = (1 << GPIO_SCL))
#define SCL_READ() ((GPI & (1 << GPIO_SCL)) != 0)

#ifndef FCPU80
#define FCPU80 80000000L
#endif

#if F_CPU == FCPU80
#define I2C_CLOCK_STRETCH_MULTIPLIER 3
#else
#define I2C_CLOCK_STRETCH_MULTIPLIER 6
#endif

static uint32_t stretch_limit = 230 * I2C_CLOCK_STRETCH_MULTIPLIER;
static uint8_t  delay_count   = 19;

static void start_transmission(void) {
  pinMode(GPIO_SDA, INPUT_PULLUP);
}

static void end_transmission(void) {
  pinMode(GPIO_SDA, OUTPUT);
}

static void i2c_delay(uint8_t v) {
  volatile uint32_t reg;

  for (int i=0; i<v; i++) reg = GPI;
}

static bool write_start(void) {
  SCL_HIGH();
  SDA_HIGH();

  if (SDA_READ() == 0) return (false);

  i2c_delay(delay_count);
  SDA_LOW();
  i2c_delay(delay_count);

  return (true);
}

static bool write_stop(void){
  uint32_t i = 0;

  SCL_LOW();
  SDA_LOW();
  i2c_delay(delay_count);
  SCL_HIGH();

  while (SCL_READ() == 0 && (i++) < stretch_limit); // clock stretching

  i2c_delay(delay_count);
  SDA_HIGH();
  i2c_delay(delay_count);

  return (true);
}

static bool write_bit(bool bit) {
  uint32_t i = 0;

  SCL_LOW();
  if (bit) SDA_HIGH(); else SDA_LOW();
  i2c_delay(delay_count+1);
  SCL_HIGH();

  while (SCL_READ() == 0 && (i++) < stretch_limit); // clock stretching

  i2c_delay(delay_count);

  return (true);
}

static bool read_bit(void) {
  uint32_t i = 0;

  SCL_LOW();
  SDA_HIGH();
  i2c_delay(delay_count+2);
  SCL_HIGH();

  while (SCL_READ() == 0 && (i++) < stretch_limit); // Clock stretching

  bool bit = SDA_READ();

  i2c_delay(delay_count);

  return (bit);
}

static bool write_byte(uint8_t byte) {
  uint8_t bit;

  for (bit = 0; bit < 8; bit++) {
    write_bit(byte & 0x80);
    byte <<= 1;
  }

  return (!read_bit()); // NACK/ACK
}

static uint8_t read_byte(bool nack) {
  uint8_t byte = 0;
  uint8_t bit;

  for (bit=0; bit<8; bit++) byte = (byte << 1) | read_bit();
  write_bit(nack);

  return (byte);
}

bool i2c_init(void) {
  pinMode(GPIO_SCL, INPUT_PULLUP);

  SCL_HIGH();

  return (true);
}

uint8_t i2c_write(uint8_t addr, uint8_t *buf, uint32_t len, bool stop) {
  uint32_t i;

  start_transmission();

  if (!write_start()) {
    end_transmission();

    return (4); // line busy
  }

  if (!write_byte(((addr << 1) | 0) & 0xFF)) {
    if (stop) write_stop();

    end_transmission();

    return (2); // received NACK on transmit of addr
  }

  for (i=0; i<len; i++) {
    if (!write_byte(buf[i])) {
      if (stop) write_stop();

      end_transmission();

      return (3); // received NACK on transmit of data
    }
  }

  if (stop) write_stop();

  i = 0;
  while (SDA_READ() == 0 && (i++) < 10){
    SCL_LOW();
    i2c_delay(delay_count);
    SCL_HIGH();
    i2c_delay(delay_count);
  }

  end_transmission();

  return (0);
}

uint8_t i2c_read(uint8_t addr, uint8_t *buf, uint32_t len, bool stop) {
  uint32_t i;

  start_transmission();

  if (!write_start()) {
    end_transmission();

    return (4); // line busy
  }

  if (!write_byte(((addr << 1) | 1) & 0xFF)) {
    if (stop) write_stop();

    end_transmission();

    return (2); // received NACK on transmit of addr
  }

  for (i=0; i<(len-1); i++) buf[i] = read_byte(false);
  buf[len-1] = read_byte(true); // last bit is NACK

  if (stop) write_stop();

  i = 0;
  while (SDA_READ() == 0 && (i++) < 10) {
    SCL_LOW();
    i2c_delay(delay_count);
    SCL_HIGH();
    i2c_delay(delay_count);
  }

  end_transmission();

  return (0);
}

uint8_t i2c_status(void) {
  int clock_count = 20;

  start_transmission();

  // SCL held low by another device, no procedure available to recover
  if (SCL_READ() == 0) {
    end_transmission();

    return (I2C_SCL_HELD_LOW);
  }

  // if SDA low, read the bits slaves have to sent to a max
  while ((SDA_READ() == 0) && (clock_count > 0)) {
    read_bit();
    // I2C bus error. SCL held low beyond slave clock stretch time
    if (SCL_READ() == 0) {
      end_transmission();

      return (I2C_SCL_HELD_LOW_AFTER_READ);
    }
  }

  // I2C bus error. SDA line held low by slave/another_master after n bits.
  if (SDA_READ() == 0) {
    end_transmission();

    return (I2C_SDA_HELD_LOW);
  }

  // line busy. SDA again held low by another device. 2nd master?
  if (!write_start()) {
    end_transmission();

    return (I2C_SDA_HELD_LOW_AFTER_INIT);
  }

  end_transmission();

  return (I2C_OK); 
}
