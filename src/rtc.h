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

#ifndef _RTC_H_
#define _RTC_H_

#include "datetime.h"

int rtc_set(struct timespec *tp);

int rtc_gettime(DateTime &dt);
int rtc_gettime(struct timespec *tp = NULL);
int rtc_settime(void);

float rtc_temp(void);

int rtc_state(void);
bool rtc_init(void);
bool rtc_fini(void);
void rtc_poll(void);

#endif // _RTC_H_
