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

#ifndef _NET_H_
#define _NET_H_

#include <ESP8266WiFi.h>
#include <Arduino.h>

int net_state(void);
bool net_init(void);
bool net_fini(void);
void net_poll(void);

bool net_connected(void);
bool net_enabled(void);

bool net_ping(const char *dest, int count = 3);

int net_scan_wifi(void);
const String &net_list_wifi(void);

void net_sleep(uint32_t us = 0);
void net_wakeup(void);

String net_hostname(void);
String net_gateway(void);
String net_netmask(void);
String net_dns(void);
String net_mac(void);
String net_ip(void);
String net_ssid(void);

int net_rssi(void);

String net_ap_gateway(void);
String net_ap_netmask(void);
String net_ap_mac(void);
String net_ap_ip(void);

int net_ap_clients(void);

#endif // _NET_H_
