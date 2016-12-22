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

// this file is generated during make
#include "buildinfo.h"

#include "websocket.h"
#include "datetime.h"
#include "config.h"
#include "clock.h"
#include "xxtea.h"
#include "main.h"
#include "load.h"
#include "i18n.h"
#include "util.h"
#include "net.h"
#include "log.h"

#include "system.h"

#define LOAD_HISTORY_LENGTH      30
#define LOAD_HISTORY_INTERVAL   500
#define NET_TRAFFIC_FULL      30000
#define NO_LOAD_IDLE_COUNT       40

extern "C" cont_t g_cont;

char device_id[14];
char device_name[16];

bool bootup = true;

uint32_t mem_free, idle_count, traffic_count;
uint32_t last_mem_free, last_idle_count, last_traffic_count;

#ifdef ALPHA
static uint8_t cpu_load, mem_usage, net_traffic;

static SysLoad load_history[LOAD_HISTORY_LENGTH];
static uint16_t load_history_index = 0;
static uint16_t load_history_count = 0;
#endif

static uint32_t no_load_mem_free = 0;
static uint16_t reboot_pending   = 0;

static bool dst = false;

static void out_of_memory(void) {
  log_print(F("SYS:  out of memory"));
}

static bool dst_is_active(void) {
  int time = clock_time();
  DateTime now(time);

  // calculate DST start and end
  uint32_t dst_start = DateTime::DST(now.Year(), I18N_DST_START);
  uint32_t dst_end   = DateTime::DST(now.Year(), I18N_DST_END);

  // check if we are in daylight saving time
  bool ret = ((time >= dst_start) && (time < dst_end));

  if (ret) log_print(F("DST is active"));

  return (ret);
}

static void reboot_poll(void) {
  if (reboot_pending) {
    if (reboot_pending == 1) {
      ESP.restart();
    } else if (reboot_pending == 9000) {
      main_fini();
    } else if (reboot_pending == 4000) {
      log_print(F("SYS:  rebooting ..."));
    }

    reboot_pending--;
  }
}

static void time_poll(void) {
  static int last_time_hour = -1;
  static uint32_t ms = millis();

  if ((millis() - ms) > 500) {
    int hour = (clock_time() % 86400) / 3600; // 86400 secs per day

    ms = millis();

    if (hour != last_time_hour) {
      dst = dst_is_active();

      // remember current hour for next round
      last_time_hour = hour;
    }
  }
}

static void first_poll(void) {
  static bool already_polled = false;

  if (!already_polled) {
    already_polled = true;

    log_print(F("SYS:  CPU is running on %iMhz"),
      (system_turbo_get()) ? 160 : 80
    );
    log_print(F("SYS:  %i bytes of total ram, %i bytes free"),
      no_load_mem_free, system_free_heap()
    );
  }
}

static void load_poll(void) {
#ifdef ALPHA
  static uint32_t ms = millis();
  SysLoad load;

  if ((millis() - ms) > LOAD_HISTORY_INTERVAL) {
    int real_millis_past = millis() - ms;
    float real_idle_count = (float)idle_count / real_millis_past;

    ms = millis();

    // sample system load once every 500ms
    load.cpu = cpu_load =
      100 - (real_idle_count / NO_LOAD_IDLE_COUNT) * 100;
    load.mem = mem_usage =
      (((float)(no_load_mem_free - mem_free)) / no_load_mem_free) * 100;
    load.net = net_traffic =
      ((float)traffic_count / NET_TRAFFIC_FULL) * 100;

    // store a history of LOAD_HISTORY_LENGTH samples
    load_history[load_history_index++] = load;
    load_history_index %= LOAD_HISTORY_LENGTH;
    if (load_history_count < LOAD_HISTORY_LENGTH) {
      load_history_count++;
    }

    last_mem_free      = mem_free;
    last_idle_count    = idle_count;
    last_traffic_count = traffic_count;

    mem_free      = 100000;
    idle_count    = 0;
    traffic_count = 0;
  }

  idle_count++;
#endif
}

bool system_init(void) {
  no_load_mem_free = system_free_heap();

  String id = net_mac();

  id.replace(":", "");

  dst = dst_is_active();

  snprintf(device_id, sizeof (device_id), "%s", id.c_str());
  xxtea_init(device_id, 0xdeadbeef);

  config_init();

  strcpy(device_name, config->device_name);
  system_turbo_set(config->cpu_turbo);

  register_out_of_memory_cb(out_of_memory);

  last_mem_free      = mem_free      = 100000;
  last_idle_count    = idle_count    = 0;
  last_traffic_count = traffic_count = 0;

  return (true);
}

bool system_fini(void) {
  return (true);
}

void system_poll(void) {
  first_poll();
  time_poll();
  load_poll();
  reboot_poll();
}

void system_yield(void) {
#ifdef ALPHA
  //idle_count++;
#endif

  yield();
}

static uint8_t get_hw_id(void) {
  uint16_t voltage = analogRead(17);

  if (voltage < 200) return (0);
  if (voltage < 400) return (1);
  if (voltage < 600) return (2);
  if (voltage < 800) return (3);

  return (4);
}

const String system_device_name(void) {
  const char *name = device_name;
  char buf[32];

  if (!strlen(name)) {
    snprintf_P(buf, sizeof (buf), PSTR("ESP8266-%06x"), ESP.getChipId());
    name = buf;
  }

  return (name);
}

const String system_hw_device(void) {
  if (get_hw_id() == 0) return (F("ESP8266"));

  return (F("UNKNOWN"));
}

const String system_hw_version(void) {
  int version = (analogRead(17) - (get_hw_id() * 200)) / 40;

  if (version == 0) return (String(F("1.0")));
  if (version == 1) return (String(F("1.1")));

  return (F("0.0"));
}

const String system_fw_version(void) {
  return (F(FIRMWARE));
}

const String system_fw_build(void) {
#ifdef BETA
  return (F("(beta)"));
#endif
#ifdef ALPHA
  return (F("(alpha)"));
#endif

  return (F("(release)"));
}

uint8_t system_cpu_load(void) {
#ifdef ALPHA
  return (cpu_load);
#else
  return (0);
#endif
}

bool system_turbo_get(void) {
  return (system_get_cpu_freq() == 160);
}

bool system_turbo_set(bool on) {
  return (system_update_cpu_freq((on) ? 160 : 80));
}

uint8_t system_mem_usage(void) {
#ifdef ALPHA
  return (mem_usage);
#else
  return (0);
#endif
}

uint8_t system_net_traffic(void) {
#ifdef ALPHA
  return (net_traffic);
#else
  return (0);
#endif
}

SysLoad &system_load_history(uint16_t index) {
#ifdef ALPHA
  int start = load_history_index - load_history_count;
  int idx = modulo(start + index, load_history_count);

  return (load_history[idx]);
#else
  static SysLoad x;

  return (x);
#endif
}

uint16_t system_load_history_entries(void) {
#ifdef ALPHA
  return (load_history_count);
#else
  return (0);
#endif
}

uint32_t system_net_xfer(void) {
  return (last_traffic_count * 2); // bytes per second
}

uint32_t system_main_loops(void) {
  return (last_idle_count * 2); // main loops per second
}

uint32_t system_mem_free(void) {
  return (last_mem_free);
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
    snprintf_P(d, sizeof (d), PSTR("%i day%s, "), days, (days > 1) ? "s" : "");
  }
  sprintf_P(buf, PSTR("%s%02i:%02i:%02i"), d, hours, minutes, seconds);

  return (buf);
}

char *system_time(char buf[], time_t time) {
  if (time == INT_MAX) time = clock_time();

  int   hours = (time % 86400) / 3600; // 86400 equals secs per day
  int minutes = (time % 3600)  /   60; //  3600 equals secs per minute
  int seconds =  time % 60;

  sprintf_P(buf, PSTR("%02i:%02i:%02i"), hours, minutes, seconds);

  return (buf);
}

time_t system_utc(void) {
  return (clock_time());
}

time_t system_localtime(void) {
  int time = clock_time();

  // add time zone offset
  time += I18N_TZ_OFFSET * 60;

  // add daylight saving offset
  time += (dst) ? 3600 : 0;

  return (time);
}

void system_device_info(String &str) {
  str += F("DEVICE\r\n\r\n");
  str += F("    Hardware: ");
  str += String(system_hw_device())         + F("\r\n");
  str += F("          ID: ");
  str += String(device_id)                  + F("\r\n");
  str += F("        Name: ");
  str += String(device_name)                + F("\r\n");
}

void system_version_info(String &str) {
  String sdk = ESP.getSdkVersion();

  sdk.replace("(", "-00-");
  sdk.replace(")", "");

  str += F("VERSION\r\n\r\n");
  str += F("    Hardware: ");
  str += system_hw_version()                     + F("\r\n");
  str += F("    Firmware: ");
  str += system_fw_version()                     + F(" ");
  str += system_fw_build()                       + F("\r\n");
  str += F("  Bootloader: ");
  str += String(ESP.getBootVersion())            + F("\r\n");
  str += F("      Source: ");
  str += String(_BuildInfo.src_version)          + F("\r\n");
  str += F("        Core: ");
  str += String(_BuildInfo.env_version)          + F("\r\n");
  str += F("         SDK: ");
  str += sdk                                     + F("\r\n");
}

void system_build_info(String &str) {
  str += F("BUILD\r\n\r\n");
  str += F("        Date: ");
  str += String(_BuildInfo.date)            + F("\r\n");
  str += F("        Time: ");
  str += String(_BuildInfo.time)            + F("\r\n");
}

void system_sys_info(String &str) {
  char uptime[24];

  String stack_guard = system_stack_corrupt() ? F("CORRUPTED") : F("OK");

  system_uptime(uptime);

  str += F("SYSTEM\r\n\r\n");
  str += F("      Uptime: ");
  str += String(uptime)                     + F("\r\n");
  str += F("       Reset: ");
  str += ESP.getResetReason()               + F("\r\n");
  str += F(" Sketch Size: ");
  str += String(system_sketch_size())       + F("\r\n");
  str += F("  Free Flash: ");
  str += String(system_free_sketch_space()) + F("\r\n");
  str += F("   Free Heap: ");
  str += String(system_free_heap())         + F("\r\n");
  str += F("  Free Stack: ");
  str += String(system_free_stack())        + F("\r\n");
  str += F(" Stack Guard: ");
  str += stack_guard                        + F("\r\n");
  str += F("   CPU Clock: ");
  str += String(ESP.getCpuFreqMHz())        + F("\r\n");
}

void system_flash_info(String &str) {
  String flash_mode = F("UNKNOWN");
  int mode = ESP.getFlashChipMode();
  char flash_id[16];

  snprintf_P(flash_id, sizeof (flash_id), PSTR("%08X"), ESP.getFlashChipId());

       if (mode == FM_QIO)  flash_mode = F("QIO");
  else if (mode == FM_QOUT) flash_mode = F("QOUT");
  else if (mode == FM_DIO)  flash_mode = F("DIO");
  else if (mode == FM_DOUT) flash_mode = F("DOUT");

  str += F("FLASH\r\n\r\n");
  str += F("     Chip ID: ");
  str += String(flash_id)                   + F("\r\n");
  str += F("   Real Size: ");
  str += String(ESP.getFlashChipRealSize()) + F("\r\n");
  str += F("        Size: ");
  str += String(ESP.getFlashChipSize())     + F("\r\n");
  str += F("       Speed: ");
  str += String(ESP.getFlashChipSpeed())    + F("\r\n");
  str += F("        Mode: ");
  str += flash_mode                         + F("\r\n");
}

void system_net_info(String &str) {
  str += F("NETWORK\r\n\r\n");
  str += F("  IP Address: ");
  str += net_ip()                           + F("\r\n");
  str += F("  Default GW: ");
  str += net_gateway()                      + F("\r\n");
  str += F("         DNS: ");
  str += net_dns()                          + F("\r\n");
  str += F("     Netmask: ");
  str += net_netmask()                      + F("\r\n");
  str += F(" MAC Address: ");
  str += net_mac()                          + F("\r\n");
  str += F("        RSSI: ");
  str += String(net_rssi())                 + F("\r\n");
}

void system_ap_info(String &str) {
  str += F("SOFT AP\r\n\r\n");
  str += F("  IP Address: ");
  str += net_ap_ip()                        + F("\r\n");
  str += F("     Gateway: ");
  str += net_ap_gateway()                   + F("\r\n");
  str += F("     Netmask: ");
  str += net_ap_netmask()                   + F("\r\n");
  str += F(" MAC Address: ");
  str += net_ap_mac()                       + F("\r\n");
}

void system_wifi_info(String &str) {
  const String &wifi_list_str = net_list_wifi();
  String ssid, rssi, crypt;
  int n = 0, parse = 0;

  if (wifi_list_str.length() != 0) {
    str += F("WIFI NETWORKS\r\n\r\n");
  }

  for (char c : wifi_list_str) {
    if (c == '\t') {
      parse = 1;
    } else if (c == '\b') {
      parse = 2;
    } else if (c == '\r') {
      parse = 0;

      str += F("          ");
      str += (n<10) ? F(" ") : F("");
      str += String(n++) + F(": ");
      str += ssid  + F(" ");
      str += rssi  + F("% ");
      str += crypt + F("\r\n");

      ssid = rssi = crypt = "";
    } else {
      if (parse == 0) {
        ssid += c;
      } else if (parse == 1) {
        rssi += c;
      } else {
        crypt += c;
      }
    }
  }
}

void system_reboot(void) {
  websocket_broadcast_message(F("reboot"));

  log_print(F("SYS:  shutting down ..."));

  reboot_pending = 10000;
}
