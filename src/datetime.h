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

#ifndef _DATETIME_H_
#define _DATETIME_H_

#include <Arduino.h>
#include <stdint.h>

class DateTime {

public:

  DateTime(uint16_t year, uint8_t month, uint8_t day);
  DateTime(const String &date);
  DateTime(time_t time = 0);

  const String date_str(void);
  const String time_str(void);
  const String str(void);

  // 32-bit time as seconds since epoch
  uint32_t TotalSeconds(void);

  // add timezone offset and daylight saving time
  void ConvertToLocalTime(void);

  uint16_t Year(void)      const { return _year + 1970; }
  uint8_t  Month(void)     const { return _month;       }
  uint8_t  Day(void)       const { return _day;         }
  uint8_t  Hour(void)      const { return _hour;        }
  uint8_t  Minute(void)    const { return _minute;      }
  uint8_t  Second(void)    const { return _second;      }
  uint8_t  DayOfWeek(void) const;

  // add seconds
  void operator += (uint32_t seconds) {
    DateTime value = DateTime(TotalSeconds() + seconds);
    *this = value;
  }

  // allows for comparisons to just work (==, <, >, <=, >=, !=)
  operator uint32_t(void) { 
    return (TotalSeconds());
  }

  void Init(uint32_t time);
  void Init(uint16_t year, uint8_t month,  uint8_t day,
             uint8_t hour, uint8_t minute, uint8_t second);

  bool Valid(void) { return (valid); }

  static uint32_t DST(uint8_t year, uint8_t month, uint8_t hour, int8_t n, uint8_t dow);

private:

  bool local;
  bool valid;

  uint8_t _year;
  uint8_t _month;
  uint8_t _day;
  uint8_t _hour;
  uint8_t _minute;
  uint8_t _second;

  static bool    IsLeapYear(uint8_t year);
  static uint8_t LeapDay(uint8_t year);
  static uint8_t DayOfWeek(uint8_t year, uint8_t month, uint8_t day);
  static uint8_t DaysInMonth(uint8_t year, uint8_t month);
  static uint8_t NthDayOfWeek(uint8_t year, uint8_t month, int8_t n, uint8_t dow);

};

#endif // _DATETIME_H_
