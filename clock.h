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

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include <sys/types.h>
#include <stdint.h>
#include <limits.h>

enum {
  CLOCK_REALTIME,
  CLOCK_MONOTONIC
};

//struct timespec {
//  time_t tv_sec; /* seconds */
//  long tv_nsec;  /* nanoseconds */
//};

struct timespec clock_subtime(struct timespec *a, struct timespec *b);
struct timespec clock_addtime(struct timespec *a, struct timespec *b);

// returns 1 if a > b, -1 if a < b, 0 if a == b
int clock_cmptime(struct timespec *a, struct timespec *b);

int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_settime(clockid_t clk_id, struct timespec *tp);

time_t clock_time(void);

#endif // _CLOCK_H_
