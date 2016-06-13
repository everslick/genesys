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
#include "user_interface.h"
#if LWIP_STATS
#include <lwip/stats.h>
#endif
}

#include "system.h"
#include "config.h"
#include "log.h"
#include "net.h"

static List<NetAccessPoint> ap_list;

static void (*event_cb)(uint16_t) = NULL;

static void default_event_handler(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED: // 5
      log_print(F("WIFI: client connected to AP\n"));
    break;

    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED: // 6
      log_print(F("WIFI: client disconnected from AP\n"));
    break;

    case WIFI_EVENT_STAMODE_DHCP_TIMEOUT: // 4
      log_print(F("WIFI: DHCP timeout\n"));
    break;

    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE: // 2
      log_print(F("WIFI: auth mode changed\n"));
    break;

    case WIFI_EVENT_STAMODE_DISCONNECTED: // 1
      log_print(F("WIFI: STA disconnected from AP\n"));
    break;

    case WIFI_EVENT_STAMODE_GOT_IP: // 3
      log_print(F("WIFI: STA connected, local IP: %s\n"), net_ip().c_str());
    break;

    case WIFI_EVENT_STAMODE_CONNECTED: // 0
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED: // 7
      // ignore
    break;

    default:
      log_print(F("WIFI: unknown event: %i\n"), event);
    break;
  }

  if (event_cb) {
    event_cb(event);
  }
}

void net_register_event_cb(void (*cb)(uint16_t)) {
  // set event callback function
  event_cb = cb;
}

bool net_init(void) {
  // delete old config, disable STA and AP
  WiFi.disconnect(true);       // true = disable STA mode
  WiFi.softAPdisconnect(true); // true = disable AP mode

  // set DHCP hostname
  WiFi.hostname(client_id);

  // enable auto reconnect
  WiFi.setAutoReconnect(true);

  // disable auto connect
  WiFi.setAutoConnect(false);

  // register default event handler
  WiFi.onEvent(default_event_handler);

  if (config->ip_static) {
    IPAddress addr(config->ip_addr);
    IPAddress netmask(config->ip_netmask);
    IPAddress gateway(config->ip_gateway);
    IPAddress dns1(config->ip_dns1);
    IPAddress dns2(config->ip_dns2);

    log_print(F("WIFI: using static IP configuration\n"));
    WiFi.config(addr, gateway, netmask, dns1, dns2);
  } else {
    log_print(F("WIFI: using DHCP\n"));
  }

  // start wifi (enabling STA mode)
  WiFi.begin(config->wifi_ssid, config->wifi_pass);

  log_print(F("WIFI: waiting for STA to connect ...\n"));

  // start local AP
  if (config->ap_enabled) {
    IPAddress addr(config->ap_addr);
    IPAddress netmask(255, 255, 255, 0);
    IPAddress gateway = addr;

    if (WiFi.softAPConfig(addr, gateway, netmask)) {
      WiFi.softAP(client_id); // no password

      IPAddress ip = WiFi.softAPIP();

      log_print(F("WIFI: AP started, local IP: %s\n"), ip.toString().c_str());
    } else {
      log_print(F("WIFI: could not start AP\n"));

      return (false);
    }
  } else {
    log_print(F("WIFI: AP disabled in config\n"));
  }

  return (true);
}

int net_scan_wifi(void) {
  int n = WiFi.scanNetworks();

  ap_list.clear();

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
    NetAccessPoint ap;

    if (indices[i] == -1) continue;

    ap.ssid = WiFi.SSID(indices[i]);
    ap.rssi = WiFi.RSSI(indices[i]) + 100;
    ap.encrypted = (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE);

    ap_list.push_back(ap);
  }

  return (ap_list.size());
}

List<NetAccessPoint> &net_list_wifi(void) {
  return (ap_list);
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

int net_xmit(void) {
#if LWIP_STATS
  return (lwip_stats.ip.xmit);
#else
  return (0);
#endif
}

bool net_connected(void) {
  return (WiFi.status() == WL_CONNECTED);
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

void net_poll(void) {
  static bool already_scanned = false;
  static uint32_t ms = 0;

  if (!already_scanned) {
    if ((millis() - ms) > 5000) {
      log_print(F("WIFI: scanning for accesspoints ...\n"));

      int n = net_scan_wifi();

      if (n < 0) {
        log_print(F("WIFI: error while scanning for accesspoints, retrying\n"));
      } else {
        if (n == 0) {
          log_print(F("WIFI: no accesspoints found\n"));
        } else {
          log_print(F("WIFI: found %i unique SSID%s\n"),
            ap_list.size(), (ap_list.size() > 1) ? "s" : ""
          );
        }

        already_scanned = true;
        ms = millis();
      }
    }
  }
}
