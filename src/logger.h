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

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <Arduino.h>

#define LOGGER_TIME_COLOR     2 // GREEN
#define LOGGER_TEXT_COLOR     9 // DEFAULT

#define LOGGER_MAX_LOG_LINES 30
#define LOGGER_MAX_LINE_LEN  65

int logger_state(void);
bool logger_init(void);
bool logger_fini(void);
void logger_poll(void);

bool logger_print(const char *str, uint16_t len, uint8_t col);
bool logger_progress(const char *str, uint16_t len);

void logger_dump_html(String &str, int lines = -1);
void logger_dump_raw(String &str, int lines = -1);

#endif // _LOGGER_H_
