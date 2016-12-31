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

#include <WiFiUdp.h>

#include "filesystem.h"
#include "console.h"
#include "module.h"
#include "system.h"
#include "config.h"
#include "util.h"
#include "net.h"
#include "log.h"

#include "logger.h"

#ifndef QUIET

/*
  Debug and log output can be sent to different log channels. This log      \n
  channel can be specified in log_init().
*/
enum LogChannel {
  LOG_CHANNEL_NONE    = 0, ///< print to /dev/null
  LOG_CHANNEL_CONSOLE = 1, ///< print to serial console
  LOG_CHANNEL_NETWORK = 2, ///< send each log line over a UDP network socket
  LOG_CHANNEL_FILE    = 4, ///< write each log line to logger file
};

struct LogLine {
  char text[LOGGER_MAX_LINE_LEN];
  uint16_t length;
  uint32_t time;
  uint8_t color;
  bool written;
  bool sent;

  LogLine(const char *str, uint16_t len, int col);
  LogLine(void);
};

struct LOGGER_PrivateData {
  WiFiUDP *udp = NULL;

  // settings from config
  uint32_t udp_host;
  uint16_t udp_port;

  // log target (channel)
  uint8_t log_channels;

  // scrollback buffer
  LogLine log_lines[LOGGER_MAX_LOG_LINES];
  uint16_t log_lines_index = 0;
  uint16_t log_lines_count = 0;
};

static LOGGER_PrivateData *p = NULL;

static File f;

static bool logfile(const LogLine &line);

LogLine::LogLine(const char *str, uint16_t len, int col) {
  memcpy(text, str, len + 1);
  length  = len;
  time    = system_localtime();
  color   = col;
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

static char *color_str(char buf[], uint8_t col) {
  sprintf_P(buf, PSTR("\e[0;3%im"), col);

  return (buf);
}

static int format_line(const LogLine &line, char *buffer, int size) {
  char col_text[8], col_time[8], buf_time[16];

  int length = snprintf_P(buffer, size, PSTR("%s[%s]%s %s"),
    color_str(col_time, LOGGER_TIME_COLOR),
    system_time(buf_time, line.time),
    color_str(col_text, line.color),
    line.text
  );

  return (length);
}

static const String html_color(uint8_t col) {
  if (col == COL_BLACK)   return (F("#000"));
  if (col == COL_RED)     return (F("#f22"));
  if (col == COL_GREEN)   return (F("#2f2"));
  if (col == COL_YELLOW)  return (F("yellow"));
  if (col == COL_BLUE)    return (F("#22f"));
  if (col == COL_MAGENTA) return (F("magenta"));
  if (col == COL_CYAN)    return (F("cyan"));
  if (col == COL_WHITE)   return (F("#fff"));

  return (F("white"));
}

static void wait(uint16_t ms = 100) {
  int start = millis();

  while ((millis() - start) < ms) {
    logger_poll();
    system_yield();
  }
}

static bool file_open(void) {
  if (rootfs && (p->log_channels | LOG_CHANNEL_FILE)) {
    String file = system_device_name() + String(F(".log"));
    bool create = !rootfs->exists(file);

    f = rootfs->open(file, "a");

    if (f) {
      // print all lines from the history buffer
      for (int i=0; i<p->log_lines_count; i++) {
        LogLine &line = log_line(i);

        if (!line.written) {
          line.written = logfile(line);
        }
      }

      // then append new log afterwards
      if (create) {
        log_print(F("LOG:  created log file '%s'"), file.c_str());
      }
    }

    return (true);
  }

  return (false);
}

static bool file_close(void) {
  if (f) {
    f.close();
  }

  return (false);
}

static bool udp_write(const char *str, uint16_t len) {
  if (!p || !p->udp) return (false);
  if (!net_connected()) return (false);

  IPAddress ip(p->udp_host);

  p->udp->beginPacket(ip, p->udp_port);
  p->udp->write(str, len);

  return ((p->udp->endPacket() == 1));
}

static void udp_begin(void) {
  if (!p || !net_connected()) return;

  if (p->udp_host && p->udp_port && (p->log_channels | LOG_CHANNEL_NETWORK)) {
    IPAddress ip(p->udp_host);

    p->udp = new WiFiUDP();

    if (p->udp->begin(p->udp_port)) {
      log_print(F("LOG:  connected to logging server: %s"),
        ip.toString().c_str()
      );
    }
  }
}

static void udp_end(void) {
  if (p && p->udp) {
    p->udp->stop();

    delete (p->udp);
    p->udp = NULL;
  }
}

static bool logfile(const LogLine &line) {
  if (!f) file_open();
  if (!f) return (false);

  char buffer[LOGGER_MAX_LINE_LEN + 40], buf_time[16];

  snprintf_P(buffer, sizeof (buffer), PSTR("[%s] %s"),
    system_time(buf_time, line.time), line.text
  );

  f.print(buffer);
  f.flush();

  return (true);
}

static bool lognetwork(const LogLine &line) {
  char buffer[LOGGER_MAX_LINE_LEN + 40];

  int length = format_line(line, buffer, sizeof (buffer));

  return (udp_write(buffer, length));
}

static bool logconsole(const LogLine &line) {
  char buffer[LOGGER_MAX_LINE_LEN + 40];

  int length = format_line(line, buffer, sizeof (buffer));

  return (console_print(buffer, length));
}

static void lograw(const String &text) {
  if (p->log_channels & LOG_CHANNEL_CONSOLE) {
    console_print(text.c_str(), text.length());
  }

  if (p->log_channels & LOG_CHANNEL_NETWORK) {
    udp_write(text.c_str(), text.length());
  }
}

int logger_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool logger_init(void) {
  char col[8];

  if (p) return (false);

  config_init();

  if (bootup && !config->logger_enabled) {
    config_fini();

    return (false);
  }

  p = (LOGGER_PrivateData *)malloc(sizeof (LOGGER_PrivateData));
  memset(p, 0, sizeof (LOGGER_PrivateData));

  // init channels
  p->log_channels = config->logger_channels;

  p->udp_host = config->logger_host;
  p->udp_port = config->logger_port;

  udp_begin();
  file_open();

  config_fini();

  // reset the terminal color
  lograw(color_str(col, COL_DEFAULT));

  return (true);
}

bool logger_fini(void) {
  char col[8];

  if (!p) return (false);

  log_print(F("LOG:  shutting down logger"));

  // reset terminal color to default
  lograw(color_str(col, COL_DEFAULT));

  // give the drivers some time to log the final messages
  wait();

  // clear log history
  p->log_lines_index = 0;
  p->log_lines_count = 0;

  file_close();
  udp_end();

  // free private p->data
  free(p);
  p = NULL;

  return (true);
}

void logger_poll(void) {
  static uint32_t ms = millis();

  if (!p) return;

  if (!p->udp) udp_begin();

  if (f && (p->log_channels & LOG_CHANNEL_FILE)) {
    if (rootfs) {
      // check if the file has been deleted
      if (!f.seek(0, SeekCur)) {
        log_print(F("LOG:  log file was deleted, closing it"));

        f.close();
      }
    } else {
      log_print(F("LOG:  filesystem was unmounted, closing log file"));

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
          line.sent = lognetwork(line);
          break;
        }
      }
    }
#endif
  }
}

bool logger_progress(const char *str, uint16_t len) {
  char col_text[8], col_time[8], buf_time[16];
  char buffer[LOGGER_MAX_LINE_LEN + 40];

  if (!p) return (false);

  snprintf_P(buffer, sizeof (buffer), PSTR("%s[%s]%s %s\e[1G"),
    color_str(col_time, LOGGER_TIME_COLOR),
    system_time(buf_time, system_localtime()),
    color_str(col_text, LOGGER_TEXT_COLOR),
    str
  );

  lograw(buffer);

  return (true);
}

bool logger_print(const char *str, uint16_t len, uint8_t col) {
  if (!p) return (false);

  LogLine line(str, len, col);

  if (p->log_channels & LOG_CHANNEL_FILE) {
    line.written = logfile(line);
  }

  // to avoid out of order logs on the logging server,
  // the following code is commented. instead we let
  // logger_poll() handle the sending

  // if (p->log_channels & LOG_CHANNEL_NETWORK) {
  //   line.sent = lognetwork(line);
  // }

  if (p->log_channels & LOG_CHANNEL_CONSOLE) {
    logconsole(line);
  }

  // add line to history buffer
  p->log_lines[p->log_lines_index++] = line;
  p->log_lines_index %= LOGGER_MAX_LOG_LINES;
  if (p->log_lines_count < LOGGER_MAX_LOG_LINES) {
    p->log_lines_count++;
  }

  return (true);
}

void logger_dump_raw(String &str, int lines) {
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

void logger_dump_html(String &str, int lines) {
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
    str += html_color(LOGGER_TIME_COLOR);
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

#else // QUIET

int logger_state(void) {
  return (MODULE_STATE_INACTIVE);
}

bool logger_init(void) {
  return (false);
}

bool logger_fini(void) {
  return (false);
}

void logger_poll(void) {}

bool logger_progress(const char *str, uint16_t len) {
  return (false);
}

bool logger_print(const char *str, uint16_t len, uint8_t col) {
  return (false);
}

void logger_dump_html(String &str, int lines) {}
void logger_dump_raw(String &str, int lines) {}

#endif // QUIET

MODULE(logger)
