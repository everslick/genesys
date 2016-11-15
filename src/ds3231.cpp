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

#include "i2c.h"

#include "ds3231.h"

// I2C slave address  
#define DS3231_ADDRESS 0x68  

// DS3231 register addresses
#define DS3231_REG_TIMEDATE   0x00
#define DS3231_REG_ALARMONE   0x07
#define DS3231_REG_ALARMTWO   0x0B

#define DS3231_REG_CONTROL    0x0E
#define DS3231_REG_STATUS     0x0F
#define DS3231_REG_AGING      0x10

#define DS3231_REG_TEMP       0x11

// DS3231 control register bits
#define DS3231_A1IE  0
#define DS3231_A2IE  1
#define DS3231_INTCN 2
#define DS3231_RS1   3
#define DS3231_RS2   4
#define DS3231_CONV  5
#define DS3231_BBSQW 6
#define DS3231_EOSC  7
#define DS3231_AIEMASK (_BV(DS3231_A1IE) | _BV(DS3231_A2IE))
#define DS3231_RSMASK (_BV(DS3231_RS1) | _BV(DS3231_RS2))

// DS3231 status register bits
#define DS3231_A1F      0
#define DS3231_A2F      1
#define DS3231_BSY      2
#define DS3231_EN32KHZ  3
#define DS3231_OSF      7
#define DS3231_AIFMASK (_BV(DS3231_A1F) | _BV(DS3231_A2F))

enum DS3231_SQUARE_WAVE_CLOCK {
  DS3231_SQUARE_WAVE_CLOCK_1HZ  = 0b00000000,
  DS3231_SQUARE_WAVE_CLOCK_1KHZ = 0b00001000,
  DS3231_SQUARE_WAVE_CLOCK_4KHZ = 0b00010000,
  DS3231_SQUARE_WAVE_CLOCK_8KHZ = 0b00011000
};

enum DS3231_SQUARE_WAVE_MODE {
  DS3231_SQUARE_WAVE_MODE_NONE,
  DS3231_SQUARE_WAVE_MODE_BATTERY,
  DS3231_SQUARE_WAVE_MODE_CLOCK,
  DS3231_SQUARE_WAVE_MODE_ALARMONE,
  DS3231_SQUARE_WAVE_MODE_ALARMTWO,
  DS3231_SQUARE_WAVE_MODE_ALARMBOTH
};

static uint8_t BcdToUint8(uint8_t val) {
  return (val - 6 * (val >> 4));
}

static uint8_t Uint8ToBcd(uint8_t val) {
  return (val + 6 * (val / 10));
}

static uint8_t BcdToBin24Hour(uint8_t bcdHour) {
  uint8_t hour;

  if (bcdHour & 0x40) {
    // 12 hour mode, convert to 24
    bool isPm = ((bcdHour & 0x20) != 0);

    hour = BcdToUint8(bcdHour & 0x1f);
    if (isPm) hour += 12;
  } else {
    hour = BcdToUint8(bcdHour);
  }

  return (hour);
}

static uint8_t get_reg(uint8_t reg) {
  uint8_t val;

  i2c_write(DS3231_ADDRESS, &reg, 1);
  i2c_read(DS3231_ADDRESS, &val, 1);

  return (val);
}

static void set_reg(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = { reg, val };

  i2c_write(DS3231_ADDRESS, buf, 2);
}

static bool ds3231_running(void) {
  uint8_t creg = get_reg(DS3231_REG_CONTROL);

  return (!(creg & _BV(DS3231_EOSC)));
}

static void ds3231_start(void) {
  uint8_t creg = get_reg(DS3231_REG_CONTROL);

  creg &= ~_BV(DS3231_EOSC);

  set_reg(DS3231_REG_CONTROL, creg);
}

static void ds3231_stop(void) {
  uint8_t creg = get_reg(DS3231_REG_CONTROL);

  creg |= _BV(DS3231_EOSC);

  set_reg(DS3231_REG_CONTROL, creg);
}

static void ds3231_enable_32khz_pin(bool enable) {
  uint8_t sreg = get_reg(DS3231_REG_STATUS);    

  if (enable == true) {
    sreg |=  _BV(DS3231_EN32KHZ); 
  } else {
    sreg &= ~_BV(DS3231_EN32KHZ); 
  }

  set_reg(DS3231_REG_STATUS, sreg);
}

static void ds3231_square_wave_mode(DS3231_SQUARE_WAVE_MODE mode) {
  uint8_t creg = get_reg(DS3231_REG_CONTROL);
  
  // clear all relevant bits to a known "off" state
  creg &= ~(DS3231_AIEMASK | _BV(DS3231_BBSQW));
  creg |= _BV(DS3231_INTCN);  // set INTCN to disables SQW

  if (mode == DS3231_SQUARE_WAVE_MODE_BATTERY) {
    creg |= _BV(DS3231_BBSQW); // set battery backup flag
    creg &= ~_BV(DS3231_INTCN); // clear INTCN to enable SQW 
  } else if (mode == DS3231_SQUARE_WAVE_MODE_CLOCK) {
    creg &= ~_BV(DS3231_INTCN); // clear INTCN to enable SQW 
  } else if (mode == DS3231_SQUARE_WAVE_MODE_ALARMONE) {
    creg |= _BV(DS3231_A1IE);
  } else if (mode == DS3231_SQUARE_WAVE_MODE_ALARMTWO) {
    creg |= _BV(DS3231_A2IE);
  } else if (mode == DS3231_SQUARE_WAVE_MODE_ALARMBOTH) {
    creg |= _BV(DS3231_A1IE) | _BV(DS3231_A2IE);
  }
      
  set_reg(DS3231_REG_CONTROL, creg);
}

static void ds3231_square_wave_clock(DS3231_SQUARE_WAVE_CLOCK freq) {
  uint8_t creg = get_reg(DS3231_REG_CONTROL);

  creg &= ~DS3231_RSMASK; // set to 0
  creg |= (freq & DS3231_RSMASK); // set freq bits

  set_reg(DS3231_REG_CONTROL, creg);
}

static void ds3231_force_temperature_compensation(bool block) {
  uint8_t creg = get_reg(DS3231_REG_CONTROL); 

  creg |= _BV(DS3231_CONV); // write CONV bit
  set_reg(DS3231_REG_CONTROL, creg);

  while (block && (creg & _BV(DS3231_CONV)) != 0) {
    // block until CONV is 0
    creg = get_reg(DS3231_REG_CONTROL); 
  } 
}

static int8_t ds3231_aging_offset(void) {
  return get_reg(DS3231_REG_AGING);
}

static void ds3231_aging_offset(int8_t value) {
  set_reg(DS3231_REG_AGING, value);
}

bool ds3231_init(void) {
  DateTime zero;

  if (ds3231_running()) {
    ds3231_stop();
    if (ds3231_running()) {
      return (false);
    }
  } else {
    ds3231_start();
    if (!ds3231_running()) {
      return (false);
    }
  }

  if (!ds3231_running()) ds3231_start();

  uint8_t status = get_reg(DS3231_REG_STATUS);

  if (status & _BV(DS3231_OSF)) {
    ds3231_set(zero);
  }

  ds3231_enable_32khz_pin(false);
  ds3231_square_wave_mode(DS3231_SQUARE_WAVE_MODE_NONE); 

  status = get_reg(DS3231_REG_STATUS);

  return (!(status & _BV(DS3231_OSF)));
}

void ds3231_set(const DateTime &dt) {
  uint8_t buf[8];

  // clear the invalid flag
  uint8_t status = get_reg(DS3231_REG_STATUS);
  status &= ~_BV(DS3231_OSF); // clear the flag
  set_reg(DS3231_REG_STATUS, status);

  // set the date time
  buf[0] = DS3231_REG_TIMEDATE;
  buf[1] = Uint8ToBcd(dt.Second());
  buf[2] = Uint8ToBcd(dt.Minute());
  buf[3] = Uint8ToBcd(dt.Hour()); // 24 hour mode only

  uint8_t year = dt.Year() - 2000;
  uint8_t centuryFlag = 0;

  if (year >= 100) {
    year -= 100;
    centuryFlag = _BV(7);
  }

  buf[4] = Uint8ToBcd(dt.DayOfWeek());
  buf[5] = Uint8ToBcd(dt.Day());
  buf[6] = Uint8ToBcd(dt.Month()) | centuryFlag;
  buf[7] = Uint8ToBcd(year);

  i2c_write(DS3231_ADDRESS, buf, 8);
}

void ds3231_get(DateTime &dt) {
  uint8_t buf[8];

  buf[0] = DS3231_REG_TIMEDATE;
  i2c_write(DS3231_ADDRESS, buf, 1);

  i2c_read(DS3231_ADDRESS, buf, 7);
  uint8_t  second = BcdToUint8(buf[0] & 0x7F);
  uint8_t  minute = BcdToUint8(buf[1]);
  uint8_t  hour   = BcdToBin24Hour(buf[2]);
  uint8_t  day    = BcdToUint8(buf[4]);
  uint8_t  month  = BcdToUint8(buf[5] & 0x7f);
  uint16_t year   = BcdToUint8(buf[6]) + 2000;

  // century wrap flag
  if (buf[5] & _BV(7)) year += 100;

  dt.Init(year, month, day, hour, minute, second);
}

float ds3231_temperature(void) {
  uint8_t buf[2];

  buf[0] = DS3231_REG_TEMP;
  i2c_write(DS3231_ADDRESS, buf, 1);

  i2c_read(DS3231_ADDRESS, buf, 2);
  float degrees = (buf[0] & 0x7F) + (buf[1] >> 6) * 0.25;
  if (buf[0] & 0x80) degrees *= -1;

  return (degrees);
}
