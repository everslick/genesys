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

#include <Arduino.h>

extern "C" {
#include <user_interface.h>
#include <cont.h>
}

#include "net.h"
#include "log.h"
#include "load.h"
#include "clock.h"
#include "websocket.h"

#include "system.h"

#define HEARTBEAT_INTERVAL      120
#define LOAD_HISTORY_LENGTH      60
#define LOAD_HISTORY_INTERVAL   500
#define NET_TRAFFIC_FULL      50000

#define YIELD_COUNT_SCALER 5 // reduce cpu load by factor

extern "C" cont_t g_cont;

extern "C" {
  void esp_yield();
}

char client_id[16];

#ifndef RELEASE
uint32_t mem_free, yield_count, traffic_count;

static uint8_t cpu_load, mem_usage, net_traffic;

static SysLoad load_history[LOAD_HISTORY_LENGTH];
static uint16_t load_history_index = 0;
static uint16_t load_history_count = 0;

static uint32_t idle_yield_count, idle_mem_free;
#endif

static uint16_t reboot_pending = 0;

static void out_of_memory(void) {
  log_print(F("SYS:  out of memory\n"));
}

static int modulo(int a, int b) {
  int r = a % b;

  return ((r < 0) ? r + b : r);
}

bool system_init(void) {
  snprintf(client_id, sizeof (client_id), "esp-%x", system_get_chip_id());

#ifndef RELEASE
  register_out_of_memory_cb(out_of_memory);

  log_color_text(COL_YELLOW);
  log_print(LINE_MEDIUM);
  log_print(F("            *** GENESYS  %s ***\n"), FIRMWARE);
  log_print(LINE_MEDIUM);
  log_color_text(COL_DEFAULT);

  log_print(F("SYS:  calibrating yield counter ...\n"));
  uint32_t start = millis();
  for (int i=0; i<50000; i++) yield();
  uint32_t count = 50000 / (millis() - start);
  log_print(F("SYS:  %i yield calls per ms\n"), count);

  idle_yield_count = count * LOAD_HISTORY_INTERVAL;
  idle_mem_free = system_free_heap();

  log_print(F("SYS:  watermark for free heap is %i bytes\n"), idle_mem_free);
#endif

  return (true);
}

bool system_poll(void) {
#ifndef RLEASE
  static uint32_t ms = 0;
  char uptime[24];

  if ((millis() - ms) > 1000 * 60 * HEARTBEAT_INTERVAL) {
    log_print(F("SYS:  uptime = %s\n"), system_uptime(uptime));
    ms = millis();
  }
#endif

  if (reboot_pending) {
    if (reboot_pending == 1) {
      log_print(F("SYS:  rebooting ...\n"));
      log_end();

      system_restart();
      esp_yield();
    } else {
      reboot_pending--;
    }
  }

  return (true);
}

void system_delay(uint32_t ms) {
#ifndef RELEASE
  int ticks = (idle_yield_count / YIELD_COUNT_SCALER) / LOAD_HISTORY_INTERVAL;

  yield_count += ticks * ms;
#endif

  delay(ms);
}

void system_yield(void) {
#ifndef RELEASE
  yield_count++;
#endif

  yield();
}

void system_idle(void) {
#ifndef RELEASE
  static uint32_t ms = 0;
  SysLoad load;

  if ((millis() - ms) > LOAD_HISTORY_INTERVAL) {
    // sample system load once every 500ms
    load.cpu = cpu_load =
      100 - ((float)yield_count / (idle_yield_count/YIELD_COUNT_SCALER)) * 100;
    load.mem = mem_usage =
      (((float)(idle_mem_free - mem_free)) / idle_mem_free) * 100;
    load.net = net_traffic =
      ((float)traffic_count / NET_TRAFFIC_FULL) * 100;

    // store a history of LOAD_HISTORY_LENGTH samples
    load_history[load_history_index++] = load;
    load_history_index %= LOAD_HISTORY_LENGTH;
    if (load_history_count < LOAD_HISTORY_LENGTH) {
      load_history_count++;
    }

    mem_free = 100000;
    yield_count = 0;
    traffic_count = 0;

    ms = millis();
  }
#endif

  system_yield();
}

uint8_t system_cpu_load(void) {
#ifndef RELEASE
  return (cpu_load);
#else
  return (0);
#endif
}

uint8_t system_mem_usage(void) {
#ifndef RELEASE
  return (mem_usage);
#else
  return (0);
#endif
}

uint8_t system_net_traffic(void) {
#ifndef RELEASE
  return (net_traffic);
#else
  return (0);
#endif
}

void system_count_net_traffic(uint32_t bytes) {
#ifndef RELEASE
  traffic_count += bytes;
#endif
}

SysLoad &system_load_history(uint16_t index) {
#ifndef RELEASE
  int start = load_history_index - load_history_count;
  int idx = modulo(start + index, load_history_count);

  return (load_history[idx]);
#else
  static SysLoad x;

  return (x);
#endif
}

uint16_t system_load_history_entries(void) {
#ifndef RELEASE
  return (load_history_count);
#else
  return (0);
#endif
}

uint32_t system_free_heap(void) {
  return (system_get_free_heap_size());
}

uint32_t system_free_stack(void) {
  return (cont_get_free_stack(&g_cont));
}

bool system_stack_corrupt(void) {
  return (cont_check(&g_cont));
}

uint32_t system_sketch_size(void) {
  return (ESP.getSketchSize());
}

uint32_t system_free_sketch_space(void) {
  return (ESP.getFreeSketchSpace());
}

char *system_uptime(char buf[]) {
  time_t time = millis() / 1e3;
  int    days = (time / 86400);
  int   hours = (time % 86400) / 3600; // 86400 equals secs per day
  int minutes = (time % 3600)  /   60; //  3600 equals secs per minute
  int seconds =  time % 60;
  char d[12] = { '\0' };

  if (days != 0) {
    snprintf(d, sizeof (d), "%i day%s, ", days, (days > 1) ? "s" : "");
  }
  sprintf(buf, "%s%02i:%02i:%02i", d, hours, minutes, seconds);

  return (buf);
}

char *system_time(char buf[], time_t time) {
  if (time == INT_MAX) time = clock_time();

  int   hours = (time % 86400) / 3600; // 86400 equals secs per day
  int minutes = (time % 3600)  /   60; //  3600 equals secs per minute
  int seconds =  time % 60;

  sprintf(buf, "%02i:%02i:%02i", hours, minutes, seconds);

  return (buf);
}

void system_device_info(String &str) {
#ifndef RELEASE
  str += "genesys version: " + String(FIRMWARE)                     + '\n';
  str += "    sketch size: " + String(system_sketch_size())         + '\n';
  str += "     free flash: " + String(system_free_sketch_space())   + '\n';
  str += "      client id: " + String(client_id)                    + '\n';
  str += "      ADC value: " + String(analogRead(17))               + '\n';
#endif
}

void system_sys_info(String &str) {
#ifndef RELEASE
  char uptime[24];

  String stack_guard = system_stack_corrupt() ? "CORRUPTED" : "OK";

  system_uptime(uptime);

  str += "         uptime: " + String(uptime)                       + '\n';
  str += "      free heap: " + String(system_free_heap())           + '\n';
  str += "     free stack: " + String(system_free_stack())          + '\n';
  str += "    stack guard: " + stack_guard                          + '\n';
  str += "       CPU load: " + String(system_cpu_load())            + '\n';
  str += "      mem usage: " + String(system_mem_usage())           + '\n';
  str += "    net traffic: " + String(system_net_traffic())         + '\n';
#endif
}

void system_net_info(String &str) {
#ifndef RELEASE
  str += "             ip: " +  net_ip()                            + '\n';
  str += "        gateway: " +  net_gateway()                       + '\n';
  str += "            dns: " +  net_dns()                           + '\n';
  str += "        netmask: " +  net_netmask()                       + '\n';
  str += "            mac: " +  net_mac()                           + '\n';
  str += "           RSSI: " +  String(net_rssi())                  + '\n';
  str += "           XMIT: " +  String(net_xmit())                  + '\n';
#endif
}

void system_ap_info(String &str) {
#ifndef RELEASE
  str += "          AP ip: " +  net_ap_ip()                         + '\n';
  str += "     AP gateway: " +  net_ap_gateway()                    + '\n';
  str += "     AP netmask: " +  net_ap_netmask()                    + '\n';
  str += "         AP mac: " +  net_ap_mac()                        + '\n';
#endif
}

void system_wifi_info(String &str) {
#ifndef RELEASE
  List<NetAccessPoint> &ap = net_list_wifi();
  List<NetAccessPoint>::iterator it;
  int n = 1;

  for (it=ap.begin(); it!=ap.end(); it++) {
    String enc = (*it).encrypted ? "yes" : "no";

    str += "           AP";
    str += (n<10) ? "0" : "";
    str += String(n++)       + ':';
    str += " ssid="          + (*it).ssid                           + ',';
    str += " rssi="          + String((*it).rssi)                   + ',';
    str += " encrypted="     + enc                                  + '\n';
  }
#endif
}

void system_reboot(void) {
  websocket_broadcast_message("reboot");

  reboot_pending = 10000;
}
