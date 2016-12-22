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

#include "system.h"
#include "module.h"
#include "config.h"
#include "clock.h"
#include "log.h"
#include "net.h"
#include "rtc.h"

#include "ntp.h"

#define NTP_LOCAL_PORT  2390
#define NTP_REMOTE_PORT  123
#define NTP_PACKET_SIZE   48

struct NTP_PrivateData {
  // settings from config
  uint32_t interval;
  char     server[64];

  // terminate module in poll()
  bool shutdown;
};

static NTP_PrivateData *p = NULL;

int ntp_gettime(struct timespec *tp) {
  byte msg[NTP_PACKET_SIZE];
  IPAddress ip;
  WiFiUDP udp;

  if (!p || !tp) return (-1);

  udp.begin(NTP_LOCAL_PORT);

  if (!WiFi.hostByName(p->server, ip)) {
    log_print(F("NTP:  failed to resolve hostname '%s'"), p->server);
    p->shutdown = true;
    return (-1);
  }

  // initialize NTP request packet
  memset(msg, 0, NTP_PACKET_SIZE);
  msg[0] = 0b00100011; // 2 bit LI = 0, 3 bit version = 4, 2 bit mode = 3

  udp.beginPacket(ip, NTP_REMOTE_PORT);
  udp.write(msg, NTP_PACKET_SIZE);
  udp.endPacket();

  // wait until a reply is available
  bool response = false;
  int start = millis();
  while ((millis() - start < 1000) && (!response)) {
    response = (udp.parsePacket() != 0);
    system_yield();
  }
  int delay = (millis() - start) / 2;

  // waiting for response timed out
  if (!response) {
    log_print(F("NTP:  waiting for server response timed out"));
    return (-1);
  }

  // read received data into buffer
  udp.read(msg, NTP_PACKET_SIZE);

  // parse datagram
  uint32_t sec = (msg[40]<<24) | (msg[41]<<16) | (msg[42]<<8) | msg[43];
  uint32_t ms  = ((msg[44]<<8) | msg[45]) / 65536.0 * 1e3;

  sec -= 2208988800UL; // convert NTP time to UTC

  //
  // TODO if MSB is set then the base year is 2036 and not 1900
  //

  struct timespec offset;

  offset.tv_sec  = delay / 1000;
  offset.tv_nsec = (delay % 1000) * 1000000;

  tp->tv_sec  = sec;
  tp->tv_nsec = ms * 1000000;

  *tp = clock_addtime(&offset, tp);

  return (0);
}

int ntp_settime(void) {
  char time_buf[16], diff_buf[16] = { '\0' };
  struct timespec now, ntp, diff;

  if (ntp_gettime(&ntp) < 0) return (-1);

  clock_gettime(CLOCK_REALTIME, &now);
  diff = clock_subtime(&now, &ntp);

  int32_t dt = diff.tv_sec * 1000 + diff.tv_nsec / 1000000;

  if (abs(diff.tv_sec) < 10) {
    snprintf_P(diff_buf, sizeof (diff_buf), PSTR(" (diff=%ims)"), dt);
  }

  system_time(time_buf, ntp.tv_sec);
  log_print(F("NTP:  system time set to %s.%i%s"),
    time_buf, (int)(ntp.tv_nsec / 1000000), diff_buf
  );
  clock_settime(CLOCK_REALTIME, &ntp);

  return (0);
}

int ntp_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool ntp_init(void) {
  if (p) return (false);

  config_init();

  if (bootup && !config->ntp_enabled) {
    log_print(F("NTP:  net time synchronization disabled in config"));

    config_fini();

    return (false);
  }

  log_print(F("NTP:  initializing NTP service"));

  p = (NTP_PrivateData *)malloc(sizeof (NTP_PrivateData));
  memset(p, 0, sizeof (NTP_PrivateData));

  p->interval = config->ntp_interval;
  strcpy(p->server, config->ntp_server);

  config_fini();

  return (true);
}

bool ntp_fini(void) {
  if (!p) return (false);

  log_print(F("NTP:  disabling NTP service"));

  // free private p->data
  free(p);
  p = NULL;

  return (true);
}

void ntp_poll(void) {
  if (p && net_connected()) {
    uint32_t interval = p->interval * 1000 * 60;
    static bool sync_pending = true;
    static uint32_t ms = millis();

    if (sync_pending) interval = 10 * 1000;

    if ((millis() - ms) > interval) {
      ms = millis();

      if (ntp_settime() < 0) {
        // NTP failed, retry in 10 seconds
        sync_pending = true;
      } else {
        struct timespec now;

        clock_gettime(CLOCK_REALTIME, &now);
        rtc_set(&now);

        // NTP succeeded, move on
        sync_pending = false;
      }
    }

    if (p->shutdown) {
      log_print(F("NTP:  disabling NTP until next reboot"));

      ntp_fini();
    }
  }
}

MODULE(ntp)
