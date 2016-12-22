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
#include <DNSServer.h>

extern "C" {
#include <user_interface.h>
#include <ping.h>

#include <lwip/err.h>
#include <lwip/dns.h>
}

extern "C" void esp_schedule();
extern "C" void esp_yield();

#include "system.h"
#include "module.h"
#include "config.h"
#include "log.h"

#include "net.h"

static String wifi_list;

static bool wifi_is_connected = false;
static bool wifi_is_enabled   = false;

static uint16_t ping_count = 0, ping_time = 0;

static bool    watchdog_enabled    = false;
static uint8_t watchdog_timeout    = 0;
static uint8_t watchdog_lost_pings = 0;

static DNSServer *dns = NULL;

static bool scan_wifi(void) {
  log_print(F("WIFI: scanning for accesspoints ..."));

  int n = net_scan_wifi();

  if (n < 0) {
    log_print(F("WIFI: error while scanning for accesspoints"));

    return (false);
  } else {
    if (n == 0) {
      log_print(F("WIFI: no accesspoints found"));
    } else {
      log_print(F("WIFI: found %i unique SSID%s"), n, (n>1)?"s":"");
    }
  }

  return (true);
}

static void default_event_handler(WiFiEvent_t event) {
  if (event == WIFI_EVENT_SOFTAPMODE_STACONNECTED) {
    log_print(F("WIFI: client connected to soft AP"));

    if (wifi_is_enabled && !wifi_is_connected) {
      log_print(F("WIFI: disabling STA mode"));

      WiFi.enableSTA(false);
    }
  } else if (event == WIFI_EVENT_SOFTAPMODE_STADISCONNECTED) {
    log_print(F("WIFI: client disconnected from soft AP"));

    if (wifi_is_enabled && !wifi_is_connected && (net_ap_clients() == 0)) {
      log_print(F("WIFI: reenabling STA mode"));

      WiFi.enableSTA(true);
      WiFi.begin();
    }
  } else if (event == WIFI_EVENT_STAMODE_AUTHMODE_CHANGE) {
    log_print(F("WIFI: STA auth mode changed"));
  } else if (event == WIFI_EVENT_STAMODE_DHCP_TIMEOUT) {
    log_print(F("WIFI: DHCP timout"));
  } else if (event == WIFI_EVENT_STAMODE_GOT_IP) {
    if (!wifi_is_connected) {
      // this event fires even in static IP configuration
      log_print(F("WIFI: STA connected, local IP: %s"), net_ip().c_str());
      wifi_is_connected = true;

      // activate watchdog
      watchdog_enabled = (watchdog_timeout != 0);
    }
  } else if (event == WIFI_EVENT_STAMODE_DISCONNECTED) {
    if (wifi_is_connected) {
      log_print(F("WIFI: STA disconnected from AP"), net_ip().c_str());
      wifi_is_connected = false;
    }
  }
}

static void watchdog_ping_sent_cb(void *opt, void *resp) {
  free(opt);
}

static void watchdog_ping_recv_cb(void *opt, void *resp) {
  ping_resp *response = (struct ping_resp *)resp;

  if (response->ping_err == -1) {
    watchdog_lost_pings++;

    if (watchdog_lost_pings == 3) {
      log_print(F("WIFI: already %i lost pings, arming network watchdog"),
        watchdog_lost_pings
      );
    }
    if (watchdog_lost_pings == 5) {
      log_print(F("WIFI: %i lost pings, triggering reboot ..."),
        watchdog_lost_pings
      );
      system_reboot();
    }
  } else {
    if (watchdog_lost_pings >= 3) {
      log_print(F("WIFI: ping received, disarming watchdog"));
    }

    watchdog_lost_pings = 0;
  }
}

static void poll_watchdog_ping(void) {
  static uint32_t ms = millis();
  ping_option *option;

  // ping the default gateway every 10 seconds
  // to see, if we are still online

  // this watchdog MUST NOT reboot the device if
  // wifi is not connected

  if (wifi_is_connected && watchdog_enabled && ((millis() - ms) > 1000 * 10)) {
    option = (struct ping_option *)calloc(1, sizeof (struct ping_option));
    ms = millis();

    option->count         = 1;
    option->ip            = WiFi.gatewayIP();
    option->coarse_time   = 1;
    option->recv_function = watchdog_ping_recv_cb;
    option->sent_function = watchdog_ping_sent_cb;

    ping_start(option);
  }
}

static void poll_watchdog_sta(void) {
  static uint32_t ms = millis();
  static uint8_t watchdog = 0;

  // watch out for STA disconnects and reboot if
  // connection cannot be reestablished after
  // X minutes

  // this watchdog MUST NOT reboot the device if
  // wifi is not enabled or never was connected
  // since last reboot

  if (wifi_is_enabled && watchdog_enabled && ((millis() - ms) > 1000 * 6)) {
    ms = millis();

    if (!wifi_is_connected) {
      if (++watchdog < watchdog_timeout * 10) { // timeout in minutes ( * 10 )
        if (watchdog == 1) {
          log_print(F("WIFI: arming network watchdog (reboot in %i min.)"),
            watchdog_timeout
          );
        }
      } else {
        log_print(F("WIFI: still not connected, triggering reboot ..."));
        system_reboot();
      }
    } else {
      if (watchdog) {
        log_print(F("WIFI: network is back, disarming watchdog"));
        watchdog = 0;
      }
    }
  }
}

static void poll_dns(void) {
  if (dns) dns->processNextRequest();
}

static void ping_sent_cb(void *opt, void *resp) {
  ping_option *option = (struct ping_option *)opt;
  ping_resp *response = (struct ping_resp *)resp;

  ping_time /= ping_count;

  log_print(F("PING: %i packets received, avg response time %i ms"),
    ping_count, ping_time
  );

  free(opt);
}

static void ping_recv_cb(void *opt, void *resp) {
  ping_option *option = (struct ping_option *)opt;
  ping_resp *response = (struct ping_resp *)resp;

  if (response->ping_err != -1) {
    ping_time += response->resp_time;
    ping_count++;

    log_print(F("PING: reply %i/%i [%ims]"),
      ping_count, option->count, response->resp_time
    );
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

  // store WiFi config in SDK flash area
  WiFi.persistent(true);

  // delete old config, disable STA and AP
  WiFi.disconnect(true);       // true = disable STA mode
  WiFi.softAPdisconnect(true); // true = disable AP mode

  // set DHCP hostname
  WiFi.hostname(system_device_name().c_str());

  // enable auto reconnect
  WiFi.setAutoReconnect(true);

  // enable auto connect
  WiFi.setAutoConnect(true);

  // register default event handler
  WiFi.onEvent(default_event_handler);

  // make sure EEPROM is mounted
  config_init();

  // store watchdog timeout
  watchdog_timeout = config->wifi_watchdog;

  // get a list of available SSIDs
  scan_wifi();

  // start wifi (enabling STA mode)
  if (bootup && !config->wifi_enabled) {
    log_print(F("WIFI: STA is disabled in config"));

    ret = false;
  } else {
    if (config->wifi_ssid[0] == '\0') {
      log_print(F("WIFI: STA is not configured"));

      ret = false;
    } else {
      wifi_is_enabled = true;

      if (config->ip_static) {
        IPAddress addr(config->ip_addr);
        IPAddress netmask(config->ip_netmask);
        IPAddress gateway(config->ip_gateway);
        IPAddress dns1(config->ip_dns1);
        IPAddress dns2(config->ip_dns2);

        log_print(F("WIFI: using static IP configuration"));

        WiFi.config(addr, gateway, netmask, dns1, dns2);
      } else {
        log_print(F("WIFI: using DHCP"));
      }

      config_get(F("wifi_ssid"), ssid);
      config_get(F("wifi_pass"), pass);

      WiFi.begin(ssid.c_str(), pass.c_str());

      log_print(F("WIFI: waiting for STA to connect (%s) ..."),
        config->wifi_ssid
      );

      WiFi.setOutputPower(config->wifi_power);
    }
  }

  // start local AP
  if (bootup && !config->ap_enabled) {
    log_print(F("WIFI: AP disabled in config"));
  } else {
    IPAddress addr(config->ap_addr);
    IPAddress netmask(255, 255, 255, 0);
    IPAddress gateway = addr;

    if (WiFi.softAPConfig(addr, gateway, netmask)) {
      WiFi.softAP(system_device_name().c_str()); // no password

      IPAddress ip = WiFi.softAPIP();

      log_print(F("WIFI: AP started, local IP: %s"), ip.toString().c_str());
    } else {
      log_print(F("WIFI: could not start AP"));
    }
  }

  if (!dns && !wifi_is_enabled) {
    IPAddress ap_addr(config->ap_addr);

    dns = new DNSServer();

    log_print(F("WIFI: starting captive portal DNS on AP"));

    dns->start(53, "*", ap_addr);
  }

  config_fini();

  return (ret);
}

bool net_fini(void) {
  if (!wifi_is_enabled) return (false);

  log_print(F("WIFI: disabling captive portal DNS"));
  delete (dns);
  dns = NULL;

  log_print(F("WIFI: disabling soft AP"));
  WiFi.softAPdisconnect(true); // true = disable AP mode

  log_print(F("WIFI: disconnecting STA from AP"));
  WiFi.disconnect(true);       // true = disable STA mode

  wifi_is_enabled   = false;
  wifi_is_connected = false;

  watchdog_lost_pings = 0;
  watchdog_timeout    = 0;
  watchdog_enabled    = false;

  return (true);
}

void net_poll(void) {
  poll_watchdog_sta();
  poll_watchdog_ping();
  poll_dns();
}

bool net_ping(const char *dest, int count) {
  ping_option *option;
  IPAddress addr;

  if (!wifi_is_connected) {
    log_print(F("PING: no WiFi connection"));

    return (false);
  }
  if (!WiFi.hostByName(dest, addr)) {
    log_print(F("PING: unknown host '%s'"), dest);

    return (false);
  }

  ping_count = 0;
  ping_time  = 0;

  option = (struct ping_option *)calloc(1, sizeof (struct ping_option));

  option->count         = count;
  option->ip            = addr;
  option->coarse_time   = 1;
  option->recv_function = ping_recv_cb;
  option->sent_function = ping_sent_cb;

  if (!ping_start(option)) {
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
  int unique = 0, n = WiFi.scanNetworks();

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

    wifi_list += WiFi.SSID(indices[i]);
    wifi_list += '\t';
    wifi_list += (int)WiFi.RSSI(indices[i]) + 100;
    wifi_list += '\b';
    wifi_list += (int)WiFi.encryptionType(indices[i]);
    wifi_list += '\r';

    unique++;
  }

  WiFi.scanDelete();

  return (unique);
}

const String &net_list_wifi(void) {
  return (wifi_list);
}

static void wifi_dns_cb(const char *name, ip_addr_t *ipaddr, void *arg) {
  if (ipaddr) {
    (* reinterpret_cast<IPAddress *>(arg)) = ipaddr->addr;
  }

  esp_schedule(); // resume the hostByName function
}

int net_gethostbyname(const String &name, IPAddress &ip) {
  ip = static_cast<uint32_t>(0);
  ip_addr_t addr;

  // host name is an IP address already
  if (ip.fromString(name)) return (1);

  err_t err = dns_gethostbyname(name.c_str(), &addr, &wifi_dns_cb, &ip);

  if (err == ERR_OK) {
    ip = addr.addr;
  } else if (err == ERR_INPROGRESS) {
    esp_yield();
    // will return here when wifi_dns_cb fires

    if (ip != 0) err = ERR_OK;
  }

  return ((err == ERR_OK) ? 1 : 0);
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
