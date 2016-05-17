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

struct Config {
  char     magic[10];        // magic string
  uint8_t  version;          // config version -> increment if struct changes

  // user management
  char     user_name[64];
  char     user_pass[64];

  // WiFi
  char     wifi_ssid[32];
  char     wifi_pass[64];

  // IP configuration
  uint8_t  ip_static;        // 0..DHCP, 1..disable dynamic host configuration
  uint32_t ip_addr;          // static IP address
  uint32_t ip_netmask;       // subnet mask
  uint32_t ip_gateway;       // default gateway
  uint32_t ip_dns1;          // primary domain name server
  uint32_t ip_dns2;          // secondary DNS

  // access point
  uint8_t  ap_enabled;       // enable private wifi network
  uint32_t ap_addr;          // access point ip address

  // mDNS
  uint8_t  mdns_enabled;     // start mDNS responder
  char     mdns_name[64];    // mDNS hostname

  // time server
  uint8_t  ntp_enabled;      // get time from NTP server
  char     ntp_server[64];   // NTP server name/ip
  uint32_t ntp_interval;     // NTP synchronization interval in minutes

  // MQTT
  uint8_t  mqtt_enabled;     // start MQTT client
  char     mqtt_server[64];  // mqtt broker name/ip
  char     mqtt_user[64];    // mqtt account name
  char     mqtt_pass[64];    // mqtt account password
  uint32_t mqtt_interval;    // mqtt publish interval in milliseconds

  uint32_t dummy[32];
};

void config_init(void);
void config_reset(void);
void config_write(void);

extern struct Config *config;

#endif // _CONFIG_H_
