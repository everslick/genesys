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

#ifdef DEBUG
#include <gdbstub.h>
#endif

#include "websocket.h"
#include "system.h"
#include "config.h"
#include "update.h"
#include "clock.h"
#include "http.h"
#include "mdns.h"
#include "gpio.h"
#include "mqtt.h"
#include "ntp.h"
#include "dns.h"
#include "net.h"
#include "log.h"

static void dump_debug_info(void) {
#ifndef RELEASE
  char col[8];
  String str;

  if (!str.reserve(2048)) {
    log_print(F("LOG:  failed to allocate memory\n"));
  }

  str = "";
  str += "\n";
  str += log_color_str(col, COL_RED);
  log_dump_raw(str);
  log_raw(str);

  str = "";
  str += "\n";
  str += log_color_str(col, COL_BLUE);
  system_device_info(str);
  log_raw(str);

  str = "";
  str += "\n";
  str += log_color_str(col, COL_GREEN);
  system_sys_info(str);
  log_raw(str);

  str = "";
  str += "\n";
  str += log_color_str(col, COL_MAGENTA);
  system_net_info(str);
  log_raw(str);

  str = "";
  str += "\n";
  str += log_color_str(col, COL_RED);
  system_ap_info(str);
  log_raw(str);

  str = "";
  str += "\n";
  str += log_color_str(col, COL_YELLOW);
  system_wifi_info(str);
  log_raw(str);

  str = "";
  str += "\n";
  str += log_color_str(col, COL_DEFAULT);
  log_raw(str);
#endif
}

static void wifi_cb(uint16_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      gpio_led_blink(GPIO_LED0, 150);

      mdns_init();
      mqtt_init();
    break;

    case WIFI_EVENT_STAMODE_DISCONNECTED:
      gpio_led_blink(GPIO_LED0, 75);
    break;
  }
}

static void button_cb(uint16_t event) {
  static bool reset_in_progress = false;
  static bool short_press = false;

  switch (event) {
    case BUTTON_EVENT_PRESSED:
      reset_in_progress = false;
      short_press = true;
    break;

    case BUTTON_EVENT_RELEASED:
      if (short_press) {
        dump_debug_info();
      } else {
        if (!reset_in_progress) {
          log_print(F("GPIO: reset ABORTED!              \n"));
        }
      }
    break;

    case BUTTON_EVENT_HOLD_1:
      log_print(F("GPIO: preparing for factory reset ...\n"));
      short_press = false;
    case BUTTON_EVENT_HOLD_2:
    case BUTTON_EVENT_HOLD_3:
    case BUTTON_EVENT_HOLD_4:
    case BUTTON_EVENT_HOLD_5:
      log_progress("GPIO: factory reset in ", " seconds", 7 - event);
    break;

    case BUTTON_EVENT_HOLD_6:
      log_print(F("GPIO: initiating factory reset ...          \n"));

      reset_in_progress = true;
      config_reset();
      system_reboot();
    break;
  }
}

static void mqtt_cb(uint16_t event) {
  switch (event) {
    case MQTT_EVENT_CONNECTED:
      log_print(F("MQTT: connected to broker: %s\n"), config->mqtt_server);
      gpio_led_on(GPIO_LED0); // GPIO 2
    break;

    case MQTT_EVENT_DISCONNECTED:
      log_print(F("MQTT: disconnected from broker\n"));
      gpio_led_blink(GPIO_LED0); // GPIO 2
    break;

    case MQTT_EVENT_PUBLISH:
    case MQTT_EVENT_PUBLISHED:
      gpio_led_toggle(GPIO_LED0);
    break;
  }
}

static void log_cb(const char *msg) {
#ifndef RELEASE
  String str, cmd = msg;
  char col[8];

  cmd.replace("\n", "");
  cmd.replace("\r", "");

  if (cmd == "dump") {
    dump_debug_info();
  } else if (cmd == "ntp") {
    ntp_settime();
  } else if (cmd == "reboot") {
    system_reboot();
  } else if (cmd == "scan") {
    net_scan_wifi();
  } else if (cmd == "reset") {
    config_reset();
    system_reboot();
  } else if (cmd == "help") {
    str += log_color_str(col, COL_GREEN);
    str += "\navailable commands are:\n";
    str += "\tntp    ... update time from ntp server\n";
    str += "\tdump   ... dump debug info\n";
    str += "\tscan   ... scan WiFi for available accesspoints\n";
    str += "\treboot ... reboot device\n";
    str += "\treset  ... perform factory reset\n";
    str += "\thelp   ... print this info\n\n";
    str += log_color_str(col, COL_DEFAULT);

    log_raw(str);
  } else {
    str += log_color_str(col, COL_RED);
    str += "\nreceived unknown command '" + cmd + "'";
    str += "\ntry 'help' to get a list of available commands\n\n";
    str += log_color_str(col, COL_DEFAULT);

    log_raw(str);
  }
#endif
}

void setup(void) {
#ifdef DEBUG
  gdbstub_init();
#endif

  net_register_event_cb(wifi_cb);
  gpio_register_button_cb(button_cb);
  mqtt_register_event_cb(mqtt_cb);
  log_register_message_cb(log_cb);

  log_init();
  system_init();
  gpio_init();
  config_init();
  net_init();
  dns_init();
  http_init();
  websocket_init();
  update_init();

  gpio_led_on(GPIO_LED1);
}

void loop(void) {
  log_poll();
  system_poll();
  gpio_poll();
  http_poll();
  websocket_poll();
  update_poll();
  mqtt_poll();
  dns_poll();
  ntp_poll();

  system_idle();
}
