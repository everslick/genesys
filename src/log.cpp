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

#include "filesystem.h"
#include "console.h"
#include "syslog.h"
#include "system.h"
#include "module.h"
#include "clock.h"
#include "util.h"

#include "log.h"

#ifndef QUIET

#define MAX_LOG_LINES 25
#define MAX_LINE_LEN  60

static bool open_file(void);

struct LogLine {
  char text[MAX_LINE_LEN];
  uint16_t length;
  uint32_t time;
  uint8_t color;
  bool written;
  bool sent;

  LogLine(const char *str, uint16_t len);
  LogLine(void);
};

struct LOG_PrivateData {
  // log target (channel)
  uint8_t log_channels;

  // currently set colors
  uint8_t color_time = COL_MAGENTA;
  uint8_t color_text = COL_DEFAULT;

  // scrollback buffer
  LogLine log_lines[MAX_LOG_LINES];
  uint16_t log_lines_index = 0;
  uint16_t log_lines_count = 0;
};

static LOG_PrivateData *p = NULL;

static File f;

LogLine::LogLine(const char *str, uint16_t len) {
  memcpy(text, str, len + 1);
  length  = len;
  time    = clock_time();
  color   = p->color_text;
  written = false;
  sent    = false;
}

LogLine::LogLine(void) {
  text[0] = '\0';
  length  = 0;
  time    = 0;
  color   = 0;
  written = false;
  sent    = false;
}

static LogLine &log_line(uint16_t index) {
  int start = p->log_lines_index - p->log_lines_count;
  int idx = modulo(start + index, p->log_lines_count);

  return (p->log_lines[idx]);
}

static int format_line(const LogLine &line, char *buffer, int size) {
  char col_text[8], col_time[8], buf_time[16];

  int length = snprintf_P(buffer, size, PSTR("%s[%s]%s %s"),
    log_color_str(col_time, p->color_time),
    system_time(buf_time, line.time),
    log_color_str(col_text, line.color),
    line.text
  );

  return (length);
}

static bool logfile(const LogLine &line) {
  if (!f) open_file();

  if (f) {
    char buffer[MAX_LINE_LEN + 40], buf_time[16];

    snprintf_P(buffer, sizeof (buffer), PSTR("[%s] %s"),
      system_time(buf_time, line.time), line.text
    );

    f.print(buffer);
    f.flush();

    return (true);
  }

  return (false);
}

static bool open_file(void) {
  if (rootfs && (p->log_channels | LOG_CHANNEL_FILE)) {
    String file = system_device_name() + String(".log");
    bool create = !rootfs->exists(file);

    f = rootfs->open(file, "a");

    if (f) {
      if (create) {
        logprint(F("LOG:  created log file '%s'\r\n"), file.c_str());
      }

      // print all lines from the history buffer
      for (int i=0; i<p->log_lines_count; i++) {
        LogLine &line = log_line(i);

        if (!line.written) {
          line.written = logfile(line);
        }
      }
    }

    return (true);
  }

  return (false);
}

static bool lognet(const LogLine &line) {
  char buffer[MAX_LINE_LEN + 40];

  int length = format_line(line, buffer, sizeof (buffer));

  return (syslog_print(buffer, length));
}

static bool logserial(const LogLine &line) {
  char buffer[MAX_LINE_LEN + 40];

  int length = format_line(line, buffer, sizeof (buffer));

  return (console_print(buffer, length));
}

static void lograw(const String &text) {
  if (p->log_channels & LOG_CHANNEL_SERIAL) {
    console_print(text.c_str(), text.length());
  }

  if (p->log_channels & LOG_CHANNEL_NETWORK) {
    syslog_print(text.c_str(), text.length());
  }
}

static void print(const char *buffer, int length) {
  if (length >= MAX_LINE_LEN) {
    length = strlen(buffer);
  }

  LogLine line(buffer, length);

  if (p->log_channels & LOG_CHANNEL_FILE) {
    line.written = logfile(line);
  }

  if (p->log_channels & LOG_CHANNEL_SERIAL) {
    logserial(line);
  }

  // add line to history buffer
  p->log_lines[p->log_lines_index++] = line;
  p->log_lines_index %= MAX_LOG_LINES;
  if (p->log_lines_count < MAX_LOG_LINES) {
    p->log_lines_count++;
  }
}

static const String html_color(uint8_t col) {
  if (col == COL_BLACK)   return (F("black"));
  if (col == COL_RED)     return (F("red"));
  if (col == COL_GREEN)   return (F("green"));
  if (col == COL_YELLOW)  return (F("yellow"));
  if (col == COL_BLUE)    return (F("blue"));
  if (col == COL_MAGENTA) return (F("magenta"));
  if (col == COL_CYAN)    return (F("cyan"));
  if (col == COL_WHITE)   return (F("white"));

  return (F("white"));
}

static void wait(uint16_t ms = 100) {
  int start = millis();

  while ((millis() - start) < ms) {
    log_poll();
    system_yield();
  }
}

void logdumpraw(String &str, int lines) {
  char time[16];

  if (!p) return;

  int first = p->log_lines_count - lines;
  if ((first < 0) || (lines < 0)) first = 0;

  for (int i=first; i<p->log_lines_count; i++) {
    LogLine line = log_line(i);

    str += F("[");
    str += system_time(time, line.time);
    str += F("] ");
    str += line.text;
  }
}

void logdumphtml(String &str, int lines) {
  char time[16];

  if (!p) return;

  str += F("<pre>");

  int first = p->log_lines_count - lines;
  if ((first < 0) || (lines < 0)) first = 0;

  for (int i=first; i<p->log_lines_count; i++) {
    LogLine line = log_line(i);
    String txt = line.text;

    txt.replace(F("\r\n"), F("<br />"));

    str += F("<span style='color:");
    str += html_color(p->color_time);
    str += F("'>[");
    str += system_time(time, line.time);
    str += F("] </span>");

    str += F("<span style='color:");
    str += html_color(line.color);
    str += F("'>");
    str += txt;
    str += F("</span>");
  }

  str += F("</pre>");
}

void logcolortext(uint8_t col) {
  if (!p) return;

  p->color_text = col;
}

void logcolortime(uint8_t col) {
  if (!p) return;

  p->color_time = col;
}

void logprogress(const String &prefix, const String &postfix, int value) {
  String str = prefix + String(value) + postfix;
  LogLine line(str.c_str(), str.length());

  if (!p) return;

  if (p->log_channels & LOG_CHANNEL_SERIAL) {
    logserial(line);
  }
  if (p->log_channels & LOG_CHANNEL_NETWORK) {
    lognet(line);
  }

  lograw("\033[1G");
}

void logprint(const __FlashStringHelper *fmt, ...) {
  if (!p || (p->log_channels == LOG_CHANNEL_NONE)) return;

  char buffer[MAX_LINE_LEN];
  bool crlf = false;
  String str = fmt;
  va_list args;
  int length;

  if (str.length() > 1) {
    crlf = (str[str.length()-2] == '\r') && (str[str.length()-1] == '\n');
  }

  va_start(args, fmt);
  length = vsnprintf_P(buffer, sizeof (buffer), (const char *)fmt, args);
  va_end(args);

  if (length >= MAX_LINE_LEN) {
    length = MAX_LINE_LEN - 1;
  }

  if (crlf) {
    buffer[length-2] = '\r';
    buffer[length-1] = '\n';
  }

  print(buffer, length);
}

void logprint(const char *fmt, ...) {
  if (!p || (p->log_channels == LOG_CHANNEL_NONE)) return;

  char buffer[MAX_LINE_LEN];
  bool crlf = false;
  String str = fmt;
  va_list args;
  int length;

  if (str.length() > 1) {
    crlf = (str[str.length()-2] == '\r') && (str[str.length()-1] == '\n');
  }

  va_start(args, fmt);
  length = vsnprintf(buffer, sizeof (buffer), fmt, args);
  va_end(args);

  if (length >= MAX_LINE_LEN) {
    length = MAX_LINE_LEN - 1;
  }

  if (crlf) {
    buffer[length-2] = '\r';
    buffer[length-1] = '\n';
  }

  print(buffer, length);
}

char *log_color_str(char buf[], uint8_t col) {
  sprintf_P(buf, PSTR("\033[0;3%im"), col);

  return (buf);
}

int log_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool log_init(void) {
  char col[8];

  if (p) return (false);

  p = (LOG_PrivateData *)malloc(sizeof (LOG_PrivateData));
  memset(p, 0, sizeof (LOG_PrivateData));

  // init channels
  p->log_channels = DEFAULT_LOG_CHANNELS;

  // set colors
  p->color_time = COL_MAGENTA;
  p->color_text = COL_DEFAULT;

  // reset the terminal color
  lograw(log_color_str(col, COL_DEFAULT));

  if (!bootup) {
    logcolortext(COL_YELLOW);
    logprint(LINE_MEDIUM);
    logcolortext(COL_DEFAULT);
    logprint(F("LOG:  enabling logging on all channels\r\n"));
  }

  return (true);
}

bool log_fini(void) {
  char col[8];

  if (!p) return (false);

  logprint(F("LOG:  disabling logging on all channels\r\n"));

  // print final yellow line
  logcolortext(COL_YELLOW);
  logprint(LINE_MEDIUM);
  logcolortext(COL_DEFAULT);

  // reset terminal color to default
  lograw(log_color_str(col, COL_DEFAULT));

  // give the drivers some time to log the final messages
  wait();

  // add a little space to log
  lograw("\r\n");
  wait();

  // clear log history
  p->log_lines_index = 0;
  p->log_lines_count = 0;

  // close file
  f.close();
  
  // free private data
  free(p);
  p = NULL;

  return (true);
}

void log_poll(void) {
  static uint32_t ms = millis();

  if (!p) return;

  if (f && (p->log_channels & LOG_CHANNEL_FILE)) {
    if (rootfs) {
      // check if the file has been deleted
      if (!f.seek(0, SeekCur)) {
        logprint(F("LOG:  log file was deleted, closing it\r\n"));

        f.close();
      }
    } else {
      logprint(F("LOG:  filesystem was unmounted, closing log file\r\n"));

      f.close();
    }
  }

  if (p->log_channels & LOG_CHANNEL_NETWORK) {
#if 0
    for (int i=0; i<log_lines_count; i++) {
      LogLine &line = log_line(i);

      if (!line.sent) {
        line.sent = lognet(line);
      }
    }
#else
    // FIXME workaround for bug in ESP UDP implementation
    //       https://github.com/esp8266/Arduino/issues/1009
    //       https://github.com/esp8266/Arduino/issues/2285

    if (p->log_lines_count && ((millis() - ms) > 10)) {
      ms = millis();

      for (int i=0; i<p->log_lines_count; i++) {
        LogLine &line = log_line(i);

        if (!line.sent) {
          line.sent = lognet(line);
          break;
        }
      }
    }
#endif
  }
}

#else // QUIET

char *log_color_str(char buf[], uint8_t col) {
  buf[0] = '\0';

  return (buf);
}

int log_state(void) {
  return (MODULE_STATE_INACTIVE);
}

bool log_init(void) {
  return (false);
}

bool log_fini(void) {
  return (false);
}

void log_poll(void) {
}

#endif // QUIET

MODULE(log)
