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

#include "console.h"
#include "logger.h"
#include "system.h"

#include "log.h"

#ifndef QUIET

// currently set text color
static uint8_t color = COL_DEFAULT;

static bool initialized = false;

static void print_header(void) {
  initialized = true;

  logcolor(COL_YELLOW);
  logprint(LINE_MEDIUM);
  logprint(
    F("  *** %s V%s Firmware V%-7.7s %-9.9s ***"),
    system_hw_device().c_str(),  system_hw_version().c_str(),
    system_fw_version().c_str(), system_fw_build().c_str()
  );
  logprint(LINE_MEDIUM);
  logcolor(COL_DEFAULT);
}

static char *color_str(char buf[], uint8_t col) {
  sprintf_P(buf, PSTR("\e[0;3%im"), col);

  return (buf);
}

static void truncate_line(char *text, int &length) {
  if (length >= LOGGER_MAX_LINE_LEN - 2) {
    length = LOGGER_MAX_LINE_LEN - 3;
  }

  text[length+0] = '\r';
  text[length+1] = '\n';
  text[length+2] = '\0';

  length += 2;
}

static void console_log(char *text, int &length, bool cr = false) {
  char buf[40], col_text[8], col_time[8], buf_time[16];

  int len = snprintf_P(buf, sizeof (buf), PSTR("%s[%s]%s "),
    color_str(col_time, LOGGER_TIME_COLOR),
    system_time(buf_time, system_localtime()),
    color_str(col_text, color)
  );

  console_print(buf, len);
  console_print(text, length);

  if (cr) console_print(F("\e[1G"));
}

void logcolor(uint8_t col) {
  color = col;
}

void logprogress(const String &prefix, const String &postfix, int value) {
  char buffer[LOGGER_MAX_LINE_LEN];

  int length = snprintf_P(buffer, sizeof (buffer) - 2, PSTR("%s%i%s"),
    prefix.c_str(), value, postfix.c_str()
  );

  if (!logger_progress(buffer, length)) {
    console_log(buffer, length, true); // CR afterwards
  }
}

void logprint(const __FlashStringHelper *fmt, ...) {
  char buffer[LOGGER_MAX_LINE_LEN];
  va_list args;
  int length;

  if (!initialized) print_header();

  va_start(args, fmt);
  length = vsnprintf_P(buffer, sizeof (buffer) - 2, (const char *)fmt, args);
  va_end(args);

  truncate_line(buffer, length);

  if (!logger_print(buffer, length, color)) {
    console_log(buffer, length);
  }
}

void logprint(const char *fmt, ...) {
  char buffer[LOGGER_MAX_LINE_LEN];
  va_list args;
  int length;

  if (!initialized) print_header();

  va_start(args, fmt);
  length = vsnprintf(buffer, sizeof (buffer) - 2, fmt, args);
  va_end(args);

  truncate_line(buffer, length);
 
  if (!logger_print(buffer, length, color)) {
    console_log(buffer, length);
  }
}

#endif // QUIET
