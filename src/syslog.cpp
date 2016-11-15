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

#include "net.h"
#include "log.h"

#include "syslog.h"

static WiFiUDP *udp = NULL;
static IPAddress udp_ip;

static bool active = false;

static bool udp_write(const char *str, uint16_t len) {
  if (!udp) return (false);
  if (!len) return (true);

  if (!net_connected()) return (false);

  udp->beginPacket(udp_ip, DEFAULT_LOG_PORT);
  udp->write(str, len);

  return ((udp->endPacket() == 1));
}

static void udp_begin(void) {
  if (!net_connected()) return;

  if ((strlen(DEFAULT_LOG_SERVER) != 0) && DEFAULT_LOG_PORT) {
    IPAddress ip;

    if (WiFi.hostByName(DEFAULT_LOG_SERVER, ip) == 1) {
      udp_ip = ip;

      udp = new WiFiUDP();
      if (udp->begin(DEFAULT_LOG_PORT)) {
        log_print(F("SYSL: connected to syslog server: %s\r\n"),
          ip.toString().c_str()
        );
      }
    }
  }
}

bool syslog_init(void) {
  if (active) return (false);

  udp_begin();

  active = true;

  return (true);
}

bool syslog_fini(void) {
  if (!active) return (false);

  if (udp) {
    udp->stop();
    delete (udp);
    udp = NULL;
  }

  active = false;

  return (true);
}

void syslog_poll(void) {
  if (!udp) udp_begin();
}

bool syslog_print(const char *str, uint16_t len) {
  return (udp_write(str, len));
}

bool syslog_print(const String &str) {
  syslog_print(str.c_str(), str.length());
}
