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
#include <ESP8266WiFiAP.h>

extern "C" {
#include <user_interface.h>
#include <ping.h>
}

#include "system.h"
#include "module.h"
#include "config.h"
#include "log.h"

#include "net.h"

#define FAKE_WIFI_NETWORKS 0

static String wifi_list;

static bool wifi_is_connected = false;
static bool wifi_is_enabled   = false;

static int  ping_count, ping_lost, ping_received, ping_avg_time;
static bool ping_finished; 

static uint32_t watchdog_timeout = 0;
static bool     watchdog_enabled = false;

static bool scan_wifi(void) {
  log_print(F("WIFI: scanning for accesspoints ...\r\n"));

  int n = net_scan_wifi();

  if (n < 0) {
    log_print(F("WIFI: error while scanning for accesspoints\r\n"));

    return (false);
  } else {
    if (n == 0) {
      log_print(F("WIFI: no accesspoints found\r\n"));
    } else {
      log_print(F("WIFI: found %i unique SSID%s\r\n"), n, (n>1)?"s":"");
    }
  }

  return (true);
}

static void default_event_handler(WiFiEvent_t event) {
  if (event == WIFI_EVENT_SOFTAPMODE_STACONNECTED) {
    log_print(F("WIFI: client connected to soft AP\r\n"));

    if (wifi_is_enabled && !wifi_is_connected) {
      log_print(F("WIFI: disabling STA mode\r\n"));

      WiFi.enableSTA(false);
    }
  } else if (event == WIFI_EVENT_SOFTAPMODE_STADISCONNECTED) {
    log_print(F("WIFI: client disconnected from soft AP\r\n"));

    if (wifi_is_enabled && !wifi_is_connected && (net_ap_clients() == 0)) {
      log_print(F("WIFI: reenabling STA mode\r\n"));

      WiFi.enableSTA(true);
    }
  } else if (event == WIFI_EVENT_STAMODE_AUTHMODE_CHANGE) {
    log_print(F("WIFI: STA auth mode changed\r\n"));
  } else if (event == WIFI_EVENT_STAMODE_DHCP_TIMEOUT) {
    log_print(F("WIFI: DHCP timout\r\n"));
  } else if (event == WIFI_EVENT_STAMODE_GOT_IP) {
    if (!wifi_is_connected) {
      // this event fires even in static IP configuration
      log_print(F("WIFI: STA connected, local IP: %s\r\n"), net_ip().c_str());
      wifi_is_connected = true;

      // activate watchdog
      watchdog_enabled = (watchdog_timeout != 0);
    }
  } else if (event == WIFI_EVENT_STAMODE_DISCONNECTED) {
    if (wifi_is_connected) {
      log_print(F("WIFI: STA disconnected from AP\r\n"), net_ip().c_str());
      wifi_is_connected = false;
    }
  }
}

static void poll_wifi(void) {
  static bool already_scanned = false;
  static uint32_t ms = millis();

  if (!already_scanned && wifi_is_connected) {
    if ((millis() - ms) > 1000 * 3) {
      ms = millis();

      already_scanned = scan_wifi();
    }
  }
}

static void poll_watchdog(void) {
  static uint32_t ms = millis();
  static uint8_t watchdog = 0;

  // the watchdog MUST NOT reboot the device if
  // wifi is not enabled or not configured yet or
  // never was connected since last reboot

  if (watchdog_enabled && ((millis() - ms) > 1000 * 6)) {
    ms = millis();

    if (!wifi_is_connected) {
      if (++watchdog < watchdog_timeout * 10) { // timeout in minutes ( * 10 )
        if (watchdog == 1) {
          log_print(F("WIFI: arming network watchdog (reboot in %i min.)\r\n"),
            watchdog_timeout
          );
        }
      } else {
        log_print(F("WIFI: still not connected, triggering reboot ...\r\n"));
        system_reboot();
      }
    } else {
      if (watchdog) {
        log_print(F("WIFI: network is back, disarming watchdog\r\n"));
        watchdog = 0;
      }

      // TODO ping the default gateway from time to time
      //      to see, if we are still online
    }
  }
}

static void ping_cb(void *opt, void *resp) {
  ping_resp *response = (struct ping_resp *)resp;

  if (response->ping_err == -1)
    ping_lost++;
  else {
    ping_received++;
    ping_avg_time += response->resp_time;
  }

  log_print(F("PING: reply %i/%i [%ims]\r\n"),
    ping_received + ping_lost, ping_count, response->resp_time
  );

  ping_finished = (ping_received + ping_lost == ping_count);
  if (ping_finished) {
    ping_avg_time = ping_avg_time / ping_count;

    log_print(F("PING: avg response time %i ms\r\n"), ping_avg_time);

    response->seqno = 0;
  }
}

int net_state(void) {
  if (wifi_is_enabled) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool net_init(void) {
  String ssid, pass;
  bool ret = true;

  if (wifi_is_enabled) return (false);

  // DON'T store WiFi config in SDK flash area
  WiFi.persistent(false);

  // delete old config, disable STA and AP
  WiFi.disconnect(true);       // true = disable STA mode
  WiFi.softAPdisconnect(true); // true = disable AP mode

  // set DHCP hostname
  WiFi.hostname(system_device_name().c_str());

  // enable auto reconnect
  WiFi.setAutoReconnect(true);

  // enable auto connect
  //WiFi.setAutoConnect(true);

  // register default event handler
  WiFi.onEvent(default_event_handler);

  // make sure EEPROM is mounted
  config_init();

  // store watchdog timeout
  watchdog_timeout = config->wifi_watchdog;

  // start local AP
  if (bootup && !config->ap_enabled) {
    log_print(F("WIFI: AP disabled in config\r\n"));
  } else {
    IPAddress addr(config->ap_addr);
    IPAddress netmask(255, 255, 255, 0);
    IPAddress gateway = addr;

    if (WiFi.softAPConfig(addr, gateway, netmask)) {
      WiFi.softAP(system_device_name().c_str()); // no password

      IPAddress ip = WiFi.softAPIP();

      log_print(F("WIFI: AP started, local IP: %s\r\n"), ip.toString().c_str());
    } else {
      log_print(F("WIFI: could not start AP\r\n"));
    }
  }

  // start wifi (enabling STA mode)
  if (bootup && !config->wifi_enabled) {
    log_print(F("WIFI: STA is disabled in config\r\n"));

    ret = false;
  } else {
    if (config->wifi_ssid[0] == '\0') {
      log_print(F("WIFI: STA is not configured\r\n"));

      ret = false;
    } else {
      wifi_is_enabled = true;

      if (config->ip_static) {
        IPAddress addr(config->ip_addr);
        IPAddress netmask(config->ip_netmask);
        IPAddress gateway(config->ip_gateway);
        IPAddress dns1(config->ip_dns1);
        IPAddress dns2(config->ip_dns2);

        log_print(F("WIFI: using static IP configuration\r\n"));

        WiFi.config(addr, gateway, netmask, dns1, dns2);
      } else {
        log_print(F("WIFI: using DHCP\r\n"));
      }

      config_get(F("wifi_ssid"), ssid);
      config_get(F("wifi_pass"), pass);

      WiFi.begin(ssid.c_str(), pass.c_str());

      log_print(F("WIFI: waiting for STA to connect (%s) ...\r\n"),
        config->wifi_ssid
      );

      WiFi.setOutputPower(config->wifi_power);
    }
  }

  config_fini();

  return (ret);
}

bool net_fini(void) {
  if (!wifi_is_enabled) return (false);

  WiFi.disconnect(true);       // true = disable STA mode
  WiFi.softAPdisconnect(true); // true = disable AP mode

  wifi_is_enabled = false;

  return (true);
}

void net_poll(void) {
  poll_watchdog();
  poll_wifi();
}

bool net_ping(const char *dest, int count) {
  static ping_option options;
  IPAddress addr;

  if (!wifi_is_connected) {
    log_print(F("PING: no WiFi connection\r\n"));

    return (false);
  }
  if (!WiFi.hostByName(dest, addr)) {
    log_print(F("PING: unknown host '%s'\r\n"), dest);

    return (false);
  }

  ping_count    = count;
  ping_lost     = 0;
  ping_received = 0;
  ping_avg_time = 0;
  ping_finished = false;

  memset(&options, 0, sizeof (struct ping_option));
  options.recv_function = ping_cb;
  options.count         = count;
  options.ip            = addr;
  options.coarse_time   = 1;

  if (!ping_start(&options)) {
    return (false);
  }

  return (true);
}

void net_sleep(uint32_t us) {
  WiFi.forceSleepBegin(us);
}

void net_wakeup(void) {
  WiFi.forceSleepWake();
}

int net_scan_wifi(void) {
  const char *fmt = PSTR("{ \"ssid\":\"%s\", \"rssi\":%i, \"crypt\":%i }");
  int unique = 0, n = WiFi.scanNetworks();
  char buf[128];

  wifi_list = "";

  if (n <= 0) return (n);

  int indices[n];

  // prepare a list of indices
  for (int i=0; i<n; i++) {
    indices[i] = i;
  }

  // sort networks by RSSI
  for (int i=0; i<n; i++) {
    for (int j=i+1; j<n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        indices[i] ^= indices[j] ^= indices[i] ^= indices[j];
      }
    }
  }

  // mark duplicates
  for (int i=0; i<n; i++) {
    if (indices[i] == -1) continue;

    String ssid = WiFi.SSID(indices[i]);

    for (int j=i+1; j<n; j++) {
      if (ssid == WiFi.SSID(indices[j])){
        indices[j] = -1; // set duplicate AP to index -1
      }
    }
  }
  
  // store unique APs in list, strongest first
  for (int i=0; i<n; i++) {
    // skip removed indices
    if (indices[i] == -1) continue;

    if (unique > 0) wifi_list += F(",\r\n");

    snprintf_P(buf, sizeof (buf), fmt,
      WiFi.SSID(indices[i]).c_str(),
      WiFi.RSSI(indices[i]) + 100,
      WiFi.encryptionType(indices[i])
    );
    wifi_list += buf;

    unique++;
  }

  // append a few fake networks for testing
  for (int i=0; i<FAKE_WIFI_NETWORKS; i++) {
    if (unique > 0) wifi_list += F(",\r\n");

    snprintf_P(buf, sizeof (buf), fmt,
      (String(F("FAKE_WIFI_")) + (i + 1)).c_str(), FAKE_WIFI_NETWORKS - i, 7
    );
    wifi_list += buf;

    unique++;
  }

  WiFi.scanDelete();

  return (unique);
}

const String &net_list_wifi(void) {
  return (wifi_list);
}

String net_hostname(void) {
  return (WiFi.hostname());
}

String net_gateway(void) {
  return (WiFi.gatewayIP().toString());
}

String net_dns(void) {
  return (WiFi.dnsIP().toString());
}

String net_netmask(void) {
  return (WiFi.subnetMask().toString());
}

String net_ip(void) {
  return (WiFi.localIP().toString());
}

String net_mac(void) {
  return (WiFi.macAddress());
}

String net_ssid(void) {
  return (WiFi.SSID());
}

int net_rssi(void) {
  return (WiFi.RSSI() + 100);
}

bool net_enabled(void) {
  return (wifi_is_enabled);
}

bool net_connected(void) {
  return (wifi_is_connected);
}

String net_ap_gateway(void) {
  struct ip_info info;

  wifi_get_ip_info(SOFTAP_IF, &info);
  return (IPAddress(info.gw.addr).toString());
}

String net_ap_netmask(void) {
  struct ip_info info;

  wifi_get_ip_info(SOFTAP_IF, &info);
  return (IPAddress(info.netmask.addr).toString());
}

String net_ap_mac(void) {
  return (WiFi.softAPmacAddress());
}

String net_ap_ip(void) {
  return (WiFi.softAPIP().toString());
}

int net_ap_clients(void) {
  return (WiFi.softAPgetStationNum());
}

MODULE(net)
