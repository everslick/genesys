#ifndef _NET_H_
#define _NET_H_

#include <ESP8266WiFi.h>

#include "list.h"

typedef struct NetAccessPoint {
  String ssid;
  int rssi;
  bool encrypted;
} NetAccessPoint;

void net_register_event_cb(void (*)(uint16_t));

bool net_init(void);
bool net_connected(void);

void net_scan_wifi(List<NetAccessPoint> &list);

String net_hostname(void);
String net_gateway(void);
String net_netmask(void);
String net_dns(void);
String net_mac(void);
String net_ip(void);
String net_ssid(void);

int net_rssi(void);
int net_xmit(void);

String net_ap_gateway(void);
String net_ap_netmask(void);
String net_ap_mac(void);
String net_ap_ip(void);

#endif // _NET_H_
