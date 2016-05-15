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
#include <stdint.h>

#include "clock.h"

static struct timespec sys_time_tp(void);
static uint32_t        sys_time_ms(void);

static struct timespec offset_tp = sys_time_tp();
static uint32_t        offset_ms = sys_time_ms();

static struct timespec sys_time_tp(void) {
  struct timespec ret;

  ret.tv_sec  = millis() / 1e3;
  ret.tv_nsec = micros() * 1e3;

  return (ret);
}

static uint32_t sys_time_ms(void) {
  return (millis());
}

struct timespec clock_subtime(struct timespec *a, struct timespec *b) {
  struct timespec ret;

  ret.tv_sec  = b->tv_sec  - a->tv_sec;
  ret.tv_nsec = b->tv_nsec - a->tv_nsec;

  if (ret.tv_nsec < 0) {
    ret.tv_sec  -= 1;
    ret.tv_nsec += 1e9; 
  }

  return (ret);
}

struct timespec clock_addtime(struct timespec *a, struct timespec *b) {
  struct timespec ret;

  ret.tv_sec  = b->tv_sec  + a->tv_sec;
  ret.tv_nsec = b->tv_nsec + a->tv_nsec;

  if (ret.tv_nsec >= 1e9) {
    ret.tv_sec  += 1;
    ret.tv_nsec -= 1e9; 
  }

  return (ret);
}

int clock_cmptime(struct timespec *a, struct timespec *b) {
  if (a->tv_sec != b->tv_sec) {
    return ((a->tv_sec > b->tv_sec) ? 1 : -1);
  }
  if (a->tv_nsec != b->tv_nsec) {
    return ((a->tv_nsec > b->tv_nsec) ? 1 : -1);
  }

  return (0);
}

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
  uint32_t diff_ms = sys_time_ms() - offset_ms;
  struct timespec diff_tp;

  if (!tp) return (-1);

  switch (clk_id) {
    case CLOCK_REALTIME:
      diff_tp.tv_sec  = diff_ms / 1e3;
      diff_tp.tv_nsec = (diff_ms - diff_tp.tv_sec * 1e3) * 1e6;

      *tp = clock_addtime(&offset_tp, &diff_tp);
    break;

    case CLOCK_MONOTONIC:
      *tp = sys_time_tp();
    break;

    default: return (-1);
  }

  return (0);
}

int clock_settime(clockid_t clk_id, struct timespec *tp) {
  if ((!tp) || (clk_id != CLOCK_REALTIME)) return (-1);

  offset_tp = *tp;
  offset_ms = sys_time_ms();

  return (0);
}

time_t clock_time(void) {
  struct timespec now;

  clock_gettime(CLOCK_REALTIME, &now);

  return (now.tv_sec);
}
