#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "system.h"
#include "config.h"
#include "clock.h"
#include "net.h"
#include "log.h"

#ifndef RELEASE

#define MAX_LOG_LINES 15
#define MAX_LINE_LEN  80

// log target (channel)
static uint8_t   log_channels = DEFAULT_LOG_CHANNELS;
static const char *log_server = DEFAULT_LOG_SERVER;
static uint16_t      log_port = DEFAULT_LOG_PORT;

// UDP socket
static WiFiUDP *udp = NULL;
static IPAddress udp_ip;
static uint16_t udp_port;

// currently set colors
static uint8_t color_time = COL_MAGENTA;
static uint8_t color_text = COL_DEFAULT;

struct LogLine {
  char text[MAX_LINE_LEN];
  uint16_t length;
  uint32_t time;
  uint8_t color;
  bool sent;

  LogLine(const char *str, uint16_t len) {
    memcpy(text, str, len + 1);
    length = len;
    time = clock_time();
    color = color_text;
    sent = false;
  }

  LogLine(void) {}
};

static LogLine log_lines[MAX_LOG_LINES];
static uint16_t log_lines_index = 0;
static uint16_t log_lines_count = 0;

static void (*message_cb)(const char *) = NULL;

static int ICACHE_FLASH_ATTR modulo(int a, int b) {
  int r = a % b;

  return ((r < 0) ? r + b : r);
}

static LogLine & ICACHE_FLASH_ATTR log_line(uint16_t index) {
  int start = log_lines_index - log_lines_count;
  int idx = modulo(start + index, log_lines_count);

  return (log_lines[idx]);
}

static void ICACHE_FLASH_ATTR lognet(const char *str, uint16_t len = 0) {
  static bool first_log_line = true;

  udp->beginPacket(udp_ip, udp_port);

  if (first_log_line) {
    udp->write("\n\n", 2);
    first_log_line = false;
  }

  if (len == 0) len = strlen(str);

  udp->write(str, len);

  udp->endPacket();
  system_count_net_traffic(len);

  // FIXME workaround for bug in ESP UDP implementation
  //       https://github.com/esp8266/Arduino/issues/1009
  system_delay(3);
}

static void ICACHE_FLASH_ATTR logserial(const char *str, uint16_t len = 0) {
  static bool first_log_line = true;

  if (first_log_line) {
    Serial.write("\n\n", 2);
    first_log_line = false;
  }

  if (len == 0) len = strlen(str);

  Serial.write(str, len);
}

static void ICACHE_FLASH_ATTR lognet(const LogLine &line) {
  char buffer[32], col_text[8], col_time[8], buf_time[16];

  int length = snprintf(buffer, sizeof (buffer), "%s[%s]%s ",
    log_color_str(col_time, color_time),
    system_time(buf_time, line.time),
    log_color_str(col_text, line.color)
  );

  lognet(buffer, length);
  lognet(line.text, line.length);
}

static void ICACHE_FLASH_ATTR logserial(const LogLine &line) {
  char buffer[32], col_text[8], col_time[8], buf_time[16];

  int length = snprintf(buffer, sizeof (buffer), "%s[%s]%s ",
    log_color_str(col_time, color_time),
    system_time(buf_time, line.time),
    log_color_str(col_text, line.color)
  );

  logserial(buffer, length);
  logserial(line.text, line.length);
}

static void ICACHE_FLASH_ATTR print(const char *buffer, int length) {
  if (length >= MAX_LINE_LEN) {
    length = strlen(buffer);
  }

  LogLine line(buffer, length);

  if (log_channels & LOG_CHANNEL_SERIAL) {
    logserial(line);
  }

  log_lines[log_lines_index++] = line;
  log_lines_index %= MAX_LOG_LINES;
  if (log_lines_count < MAX_LOG_LINES) {
    log_lines_count++;
  }
}

static const String ICACHE_FLASH_ATTR html_color(uint8_t col) {
  switch (col) {
    case COL_BLACK:   return ("black");
    case COL_RED:     return ("red");
    case COL_GREEN:   return ("green");
    case COL_YELLOW:  return ("yellow");
    case COL_BLUE:    return ("blue");
    case COL_MAGENTA: return ("magenta");
    case COL_CYAN:    return ("cyan");
    case COL_WHITE:   return ("white");
  }

  return ("white");
}

void ICACHE_FLASH_ATTR loginit(void) {
  char col[8];

  if (log_channels & LOG_CHANNEL_SERIAL) {
    Serial.begin(115200);
  }

  if (log_channels & LOG_CHANNEL_NETWORK) {
    if ((strlen(log_server) != 0) && log_port) {
      IPAddress ip;

      udp = new WiFiUDP();
      udp->begin(log_port);

      // FIXME if we cannot resolve the log server name, we give up
      //       but we really should just retry later in log_poll()
      if (WiFi.hostByName(log_server, ip) != 1) {
        logprint(F("LOG:  DNS lookup for '%s' failed\n"), log_server);
      }

      udp_ip = ip;
      udp_port = log_port;
    }
  }

  // reset the terminal color
  lograw(log_color_str(col, COL_DEFAULT));
}

void ICACHE_FLASH_ATTR logdumpraw(String &str) {
  char time[16];

  for (int i=0; i<log_lines_count; i++) {
    LogLine line = log_line(i);

    str += "[";
    str += system_time(time, line.time);
    str += "] ";
    str += line.text;
  }
}

void ICACHE_FLASH_ATTR logdumphtml(String &str) {
  for (int i=0; i<log_lines_count; i++) {
    LogLine line = log_line(i);
    String txt = line.text;
    char time[16];

    txt.replace("\n", "<br />");

    str += "<span style='color:" + html_color(color_time) + "'>[";
    str += system_time(time, line.time);
    str += "] </span>";

    str += "<span style='color:" + html_color(line.color) + "'>";
    str += txt;
    str += "</span>";
  }
}

void ICACHE_FLASH_ATTR logpoll(void) {
  if ((log_channels & LOG_CHANNEL_NETWORK) && udp && net_connected()) {
    for (int i=0; i<log_lines_count; i++) {
      LogLine &line = log_line(i);

      if (!line.sent) {
        lognet(line);
        line.sent = true;
      }
    }
  }

  // read data from UDP socket
  if (udp) {
    int len = udp->parsePacket();

    if (len != 0) {
      char buf[len + 1];

      buf[len] = '\0';
      udp->read(buf, len);

      if (message_cb) message_cb(buf);
    }
  }
}

void ICACHE_FLASH_ATTR logclear(void) {
  //log_lines.clear();
}

void ICACHE_FLASH_ATTR logcolortext(uint8_t col) {
  color_text = col;
}

void ICACHE_FLASH_ATTR logcolortime(uint8_t col) {
  color_time = col;
}

void ICACHE_FLASH_ATTR logend(void) {
  char col[8];

  logprint(F("LOG:  disabling logging on all channels\n"));

  logcolortext(COL_YELLOW);
  logprint(LINE_MEDIUM);
  logcolortext(COL_DEFAULT);
  lograw(log_color_str(col, COL_DEFAULT));
  lograw("\n");

  // give the drivers some time to log the final messages
  logpoll();
  system_delay(1000);

  if (log_channels & LOG_CHANNEL_SERIAL) {
    Serial.end();
  }

  if (log_channels & LOG_CHANNEL_NETWORK) {
    if (udp) {
      udp->stop();
      delete (udp);
      udp = NULL;
    }
  }

  log_channels = LOG_CHANNEL_NONE;

  logclear();
}

void ICACHE_FLASH_ATTR logprogress(const String &prefix, const String &postfix, int value) {
  String str = prefix + String(value) + postfix;
  LogLine line(str.c_str(), str.length());

  if (log_channels & LOG_CHANNEL_SERIAL) {
    logserial(line);
  }
  if ((log_channels & LOG_CHANNEL_NETWORK) && udp && net_connected()) {
    lognet(line);
  }

  lograw("\033[1G");
}

void ICACHE_FLASH_ATTR lograw(const String &text) {
  if (log_channels & LOG_CHANNEL_SERIAL) {
    logserial(text.c_str(), text.length());
  }
  if ((log_channels & LOG_CHANNEL_NETWORK) && udp && net_connected()) {
    lognet(text.c_str(), text.length());
  }
}

void ICACHE_FLASH_ATTR logprint(const __FlashStringHelper *fmt, ...) {
  char buffer[MAX_LINE_LEN];
  va_list args;
  int length;

  if (log_channels == LOG_CHANNEL_NONE) return;

  va_start(args, fmt);
  length = vsnprintf_P(buffer, sizeof (buffer), (const char *)fmt, args);
  va_end(args);

  print(buffer, length);
}

void ICACHE_FLASH_ATTR logprint(const char *fmt, ...) {
  char buffer[MAX_LINE_LEN];
  va_list args;
  int length;

  if (log_channels == LOG_CHANNEL_NONE) return;

  va_start(args, fmt);
  length = vsnprintf(buffer, sizeof (buffer), fmt, args);
  va_end(args);

  print(buffer, length);
}

void ICACHE_FLASH_ATTR logregistermessagecb(void (*cb)(const char *)) {
  // set message callback function
  message_cb = cb;
}

#endif // RELASE

char * ICACHE_FLASH_ATTR log_color_str(char buf[], uint8_t col) {
  sprintf(buf, "\033[0;3%im", col);

  return (buf);
}
