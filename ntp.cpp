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

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "system.h"
#include "config.h"
#include "clock.h"
#include "log.h"
#include "net.h"

#include "ntp.h"

#define NTP_LOCAL_PORT  2390
#define NTP_REMOTE_PORT  123
#define NTP_PACKET_SIZE   48

int ntp_gettime(struct timespec *tp) {
  byte msg[NTP_PACKET_SIZE];
  int waiting = 1000; // ms
  IPAddress server;
  WiFiUDP udp;

  if (!tp) return (-1);

  udp.begin(NTP_LOCAL_PORT);

  if (!WiFi.hostByName(config->ntp_server, server)) {
    log_print(F("NTP:  failed to resolve hostname '%s'\n"), config->ntp_server);
    return (-1);
  }

  // initialize NTP request packet
  memset(msg, 0, NTP_PACKET_SIZE);
  msg[0] = 0b00100011; // 2 bit LI = 0, 3 bit version = 4, 2 bit mode = 3

  log_print(F("NTP:  sending request to %i.%i.%i.%i\n"),
    server[0], server[1], server[2], server[3]
  );

  udp.beginPacket(server, NTP_REMOTE_PORT);
  udp.write(msg, NTP_PACKET_SIZE);
  udp.endPacket();

  int start = millis();

  system_yield();

  // wait until a reply is available
  while ((waiting > 0) && (udp.parsePacket() == 0)) {
    waiting -= 3;
    system_delay(3);
  }

  // waiting for response timed out
  if (waiting <= 0) {
    log_print(F("NTP:  waiting for server response timed out\n"));
    return (-1);
  }

  // measure time delay between sent and received message
  int delay = (millis() - start) / 2;
  log_print(F("NTP:  round trip offset is %ims\n"), delay);

  // read received data into buffer
  udp.read(msg, NTP_PACKET_SIZE);

  // parse datagram
  uint32_t sec = (msg[40]<<24) | (msg[41]<<16) | (msg[42]<<8) | msg[43];
  uint32_t ms  = ((msg[44]<<8) | msg[45]) / 65536.0 * 1e3;

  sec -= 2208988800UL; // convert NTP time to UTC

  //
  // TODO if MSB is set the time base is the year 2036 and not 1900
  //

  struct timespec offset;

  offset.tv_sec  = delay / 1e3;
  offset.tv_nsec = (delay % 1000) * 1e6;

  tp->tv_sec  = sec;
  tp->tv_nsec = ms * 1e6;

  *tp = clock_addtime(&offset, tp);

  return (0);
}

int ntp_settime(void) {
  char time_buf[16], diff_buf[16] = { '\0' };
  struct timespec now, ntp, diff;

  if (ntp_gettime(&ntp) < 0) return (-1);

  clock_gettime(CLOCK_REALTIME, &now);
  diff = clock_subtime(&now, &ntp);

  int32_t dt = diff.tv_sec * 1e3 + diff.tv_nsec / 1e6;

  if (abs(diff.tv_sec) < 10) {
    snprintf(diff_buf, sizeof (diff_buf), " (diff=%ims)", dt);
  }

  system_time(time_buf, ntp.tv_sec);
  log_print(F("NTP:  time set to %s.%i%s\n"),
    time_buf, (int)(ntp.tv_nsec / 1e6), diff_buf
  );
  clock_settime(CLOCK_REALTIME, &ntp);

  return (0);
}

bool ntp_init(void) {
  return (true);
}

bool ntp_poll(void) {
  uint32_t interval = config->ntp_interval * 1000 * 60;
  static bool sync_pending = true;
  static uint32_t ms = 0;

  if (!config->ntp_enabled) return (false);

  if (net_connected()) {
    if (sync_pending) interval = 10 * 1000;

    if ((millis() - ms) > interval) {
      sync_pending = (ntp_settime() < 0);

      ms = millis();
    }
  }

  return (true);
}
