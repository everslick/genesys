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

#include "i18n.h"
#include "log.h"

#include "datetime.h"

static const uint8_t PROGMEM days_in_month[] = {
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const uint8_t PROGMEM day_of_week[] = {
  0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
};

DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day) {
  local = false;
  valid = true;

  Init(year, month, day, 0, 0, 0);
}

// sample input: date = "2016/06/05 12:34:56"
DateTime::DateTime(const String &date) {
  local = false;
  valid = false;

  if (date.length() >= 19) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int n = sscanf(date.c_str(), "%d/%d/%d %d:%d:%d",
      &year, &month, &day, &hour, &minute, &second
    );

    if (n == 6) {
      Init(year, month, day, hour, minute, second);
      valid = true;
    }
  }
}

DateTime::DateTime(time_t time) {
  local = false;
  valid = true;

  Init(time);
}

const String
DateTime::date_str(void) {
  int year  = _year + 1970;
  int month = _month;
  int day   = _day;
  char buf[16];

  snprintf_P(buf, sizeof (buf), PSTR(I18N_DATE_FORMAT), I18N_DATE_ORDER);

  return (buf);
}

const String
DateTime::time_str(void) {
  char buf[16];

  snprintf_P(buf, sizeof (buf), PSTR("%02i:%02i:%02i"),
    Hour(), Minute(), Second()
  );

  return (buf);
}

const String
DateTime::str(void) {
  return (date_str() + " " + time_str());
}

uint8_t
DateTime::DayOfWeek(uint8_t year, uint8_t month, uint8_t day) {
  uint8_t t = pgm_read_byte(&day_of_week[month - 1]);
  uint16_t y = year + 1970;
  
  y -= (month < 3) ? 1 : 0;

  return (y + y/4 - y/100 + y/400 + t + day) % 7;
}

uint8_t
DateTime::DayOfWeek(void) const {
  return (DayOfWeek(_year, _month, _day));
}

uint8_t
DateTime::LeapDay(uint8_t year) {
  return ((IsLeapYear(year) ? 1 : 0));
}

bool
DateTime::IsLeapYear(uint8_t year) {
  uint16_t y = year + 1970;

  return (((y%4 == 0) && (y%100 != 0)) || (y%400 == 0));
}

uint8_t
DateTime::DaysInMonth(uint8_t year, uint8_t month) {
  uint8_t d = pgm_read_byte(&days_in_month[month - 1]);

  if (month == 2) d += LeapDay(year);

  return (d);
}

uint8_t
DateTime::NthDayOfWeek(uint8_t year, uint8_t month, int8_t n, uint8_t dow) {
  // search forward
  if (n > 0) {
    uint8_t date = 1;
    uint8_t first = DayOfWeek(year, month, date);

    while (first != dow) {
      first = (first + 1) % 7;
      date++;
    }

    date += (n - 1) * 7;

    return (date);
  }

  // search backwards
  if (n < 0) {
    uint8_t date = DaysInMonth(year, month);
    uint8_t last = DayOfWeek(year, month, date);

    while (last != dow) {
      last = (last - 1) % 7;
      date--;
    }

    date -= (abs(n) - 1) * 7;

    return (date);
  }

  return (0);
}

uint32_t
DateTime::DST(uint8_t year, uint8_t month, uint8_t hour, int8_t n, uint8_t dow) {
  uint8_t day = NthDayOfWeek(year, month, n, dow);

  DateTime dt(year + 1970, month, day);

  return (dt + (hour * 3600));
}

void
DateTime::ConvertToLocalTime(void) {
  if (!local) {
    uint32_t dst_start = DST(_year, I18N_DST_START);
    uint32_t dst_end   = DST(_year, I18N_DST_END);
    uint32_t time = TotalSeconds();

    // add time zone offset
    time += I18N_TZ_OFFSET * 60;

    // add daylight saving offset
    time += ((time >= dst_start) && (time < dst_end)) ? 3600 : 0;

    Init(time);

    local = true;
  }
}

uint32_t
DateTime::TotalSeconds(void) {
  uint32_t days = _day - 1;

  for (uint8_t m=1; m<_month; m++) {
    days += pgm_read_byte(&days_in_month[m - 1]);
  }

  if (_month > 2) days += LeapDay(_year);

  for (uint8_t y=0; y<_year; y++) {
    days += 365 + LeapDay(y);
  }

  return (((days * 24 + _hour) * 60 + _minute) * 60 + _second);
}

void DateTime::Init(uint32_t time) {
  uint32_t seconds = time;
  uint8_t leap_day;
  uint16_t days;

  _second = seconds % 60;
   time   = seconds / 60;
  _minute = time % 60;
   time  /= 60;
  _hour   = time % 24;
   days   = time / 24;

  for (_year=0;; _year++) {
    leap_day = LeapDay(_year);
    if (days < 365 + leap_day) break;
    days -= 365 + leap_day;
  }

  for (_month=1;; _month++) {
    uint8_t d = DaysInMonth(_year, _month);
    if (days < d) break;
    days -= d;
  }

  _day = days + 1;
}

void
DateTime::Init(uint16_t year, uint8_t month,  uint8_t day,
                uint8_t hour, uint8_t minute, uint8_t second) {

  _year   = year - 1970;
  _month  = month;
  _day    = day;
  _hour   = hour;
  _minute = minute;
  _second = second;
}
