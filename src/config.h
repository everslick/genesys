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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <Arduino.h>

struct Config {
  char     magic[8];           // magic string
  uint8_t  version;            // config version -> increment if struct changes

  // user management
  char     user_name[17];      // username for web interface
  char     user_pass[33];      // password for web interfac

  // alias
  char     device_name[17];    // human readable device name

  // WiFi
  uint8_t  wifi_enabled;       // enable wifi network
  char     wifi_ssid[33];      // wlan SSID
  char     wifi_pass[33];      // wlan password
  uint8_t  wifi_power;         // output power in dBm (0 - 21)
  uint32_t wifi_watchdog;      // wifi watchdog time in seconds (0 = disabled)

  // IP configuration
  uint8_t  ip_static;          // 0..DHCP, 1..disable dynamic host configuration
  uint32_t ip_addr;            // static IP address
  uint32_t ip_netmask;         // subnet mask
  uint32_t ip_gateway;         // default gateway
  uint32_t ip_dns1;            // primary domain name server
  uint32_t ip_dns2;            // secondary DNS

  // access point
  uint8_t  ap_enabled;         // enable private wifi network
  uint32_t ap_addr;            // access point ip address

  // time server
  uint8_t  ntp_enabled;        // get time from NTP server
  char     ntp_server[33];     // NTP server name/ip
  uint32_t ntp_interval;       // NTP synchronization interval in minutes

  // transport
  uint8_t  transport_enabled;  // start network transport
  char     transport_url[65];  // mqtt broker/emon server URL
  char     transport_user[17]; // mqtt account user name
  char     transport_pass[33]; // mqtt account password
  uint32_t transport_interval; // publish interval in seconds

  // http update
  uint8_t  update_enabled;     // poll server for updates
  char     update_url[65];     // URL of firmware file
  uint32_t update_interval;    // poll interval in hours

  // local file storage
  uint8_t  storage_enabled;    // local storage of CVS files enabled
  uint32_t storage_interval;   // poll interval in seconds
  uint32_t storage_mask;       // bitmask of things to store

  // general switches
  uint8_t  mdns_enabled;       // start mDNS responder
  uint8_t  webserver_enabled;  // start http service
  uint8_t  websocket_enabled;  // start websocket server
  uint8_t  telnet_enabled;     // start start telnet server
  uint8_t  gpio_enabled;       // enable leds and buttons
  uint8_t  rtc_enabled;        // enable RTC module

  // CPU speed
  uint8_t  cpu_turbo;          // enable 160MHz clock

};

bool config_set(const String &name, const String &value);
bool config_get(const String &name, String &value);
bool config_clr(const String &name);

int config_state(void);
bool config_init(void);
bool config_fini(void);
void config_reset(void);
void config_write(void);
void config_import(String &str);
void config_export(String &str);

extern struct Config *config;

#endif // _CONFIG_H_
