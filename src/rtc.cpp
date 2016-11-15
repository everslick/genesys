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

#include "system.h"
#include "module.h"
#include "config.h"
#include "ds3231.h"
#include "clock.h"
#include "log.h"

#include "rtc.h"

struct RTC_PrivateData {
  uint32_t set_millis;
  uint32_t set_time;
};

static RTC_PrivateData *p = NULL;

int rtc_gettime(DateTime &dt) {
  ds3231_get(dt);

  return (0);
}

int rtc_gettime(struct timespec *tp) {
  DateTime first, rtc;

  if (!p) return (-1);

  ds3231_get(first);
  ds3231_get(rtc);

  // wait for seconds to jump for ms precision
  while (rtc == first) {
    ds3231_get(rtc);
    system_yield();
  }

  if (tp) {
    tp->tv_sec  = rtc;
    tp->tv_nsec = 0;

    return (0);
  }

  return (-2);
}

int rtc_settime(void) {
  char time_buf[16], diff_buf[16] = { '\0' };
  struct timespec now, rtc, diff;

  if (rtc_gettime(&rtc) < 0) return (-1);

  clock_gettime(CLOCK_REALTIME, &now);
  diff = clock_subtime(&now, &rtc);

  int32_t dt = diff.tv_sec * 1000 + diff.tv_nsec / 1000000;

  if (abs(diff.tv_sec) < 10) {
    snprintf_P(diff_buf, sizeof (diff_buf), PSTR(" (diff=%ims)"), dt);
  }

  system_time(time_buf, rtc.tv_sec);
  log_print(F("RTC:  system time set to %s.%i%s\r\n"),
    time_buf, (int)(rtc.tv_nsec / 1000000), diff_buf
  );
  clock_settime(CLOCK_REALTIME, &rtc);

  return (0);
}

int rtc_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool rtc_init(void) {
  if (p) return (false);

  config_init();

  if (bootup) {
    if (!config->rtc_enabled) {
      log_print(F("RTC:  module disabled in config\r\n"));

      config_fini();

      return (false);
    }
  }

  config_fini();

  if (!ds3231_init()) {
    log_print(F("RTC:  no DS3231 chip found\r\n"));

    return (false);
  }

  p = (RTC_PrivateData *)malloc(sizeof (RTC_PrivateData));
  memset(p, 0, sizeof (RTC_PrivateData));

  return (rtc_settime() == 0);
}

bool rtc_fini(void) {
  if (!p) return (false);

  log_print(F("RTC:  disabling real time clock\r\n"));

  // free private p->data
  free(p);
  p = NULL;

  return (true);
}

void rtc_poll(void) {
  uint32_t interval = 1000 * 60 * 60;
  static uint32_t ms = 0;

  if (p) {
    if ((millis() - ms) > interval) {
      ms = millis();

      rtc_settime();
    }

    if (p->set_time && (millis() > p->set_millis)) {
      DateTime get, set(p->set_time);
      int retry = 2; // try max. three times

      do {
        ds3231_set(set);
        ds3231_get(get);

        if (get == set) {
          log_print(F("RTC:  clock set to %s %s\r\n"),
            set.date_str().c_str(), set.time_str().c_str()
          );
        } else {
          String str = retry ? F("retrying ...") : F("giving up!");
          log_print(F("RTC:  error (set=%s, get=%s), %s\r\n"),
            set.time_str().c_str(), get.time_str().c_str(), str.c_str()
          );
        }
      } while ((get != set) && retry--);

      p->set_time = 0;
    }
  }
}

int rtc_set(struct timespec *tp) {
  if (!tp || !p) return (-1);

  // wait for next second for ms precision
  p->set_millis = millis() + (1000 - (tp->tv_nsec / 1000000));
  p->set_time   = tp->tv_sec + 1;

  return (0);
}

float rtc_temp(void) {
  if (!p) return (0.0);

  return (ds3231_temperature());
}

MODULE(rtc)
