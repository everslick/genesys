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

/**
  \file
  \brief  Macros to generate debug and error logs.
  \author Clemens Kirchgatterer <clemens@1541.org>
  \date   2004-08-18
*/

#ifndef _LOG_H_
#define _LOG_H_

#include <Arduino.h>

#define COL_BLACK   0
#define COL_RED     1
#define COL_GREEN   2
#define COL_YELLOW  3
#define COL_BLUE    4
#define COL_MAGENTA 5
#define COL_CYAN    6
#define COL_WHITE   7
#define COL_DEFAULT 9

void logprogress(const String &prefix, const String &postfix, int value);
void logcolor(uint8_t col);

void logprint(const __FlashStringHelper *fmt, ...);
void logprint(const char *fmt, ...);

#ifdef QUIET

#define log_progress(PRE, POST, VAL)
#define log_color(COL)
#define log_print(...)

#else // QUIET

#define log_progress(PRE, POST, VAL) logprogress(PRE, POST, VAL)
#define log_color(COL)               logcolor(COL)
#define log_print(...)               logprint(__VA_ARGS__)

#endif // QUIET

#define LINE_THIN   F("----------------------------------------------------")
#define LINE_MEDIUM F("====================================================")
#define LINE_THICK  F("####################################################")

#endif // _LOG_H_
