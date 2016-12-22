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

#include "filesystem.h"
#include "telemetry.h"
#include "webserver.h"
#include "websocket.h"
#include "storage.h"
#include "console.h"
#include "at24c32.h"
#include "system.h"
#include "logger.h"
#include "telnet.h"
#include "module.h"
#include "config.h"
#include "update.h"
#include "clock.h"
#include "gpio.h"
#include "mdns.h"
#include "rtc.h"
#include "cli.h"
#include "ntp.h"
#include "net.h"
#include "log.h"
#include "i2c.h"
#include "led.h"

static void button_cb(uint16_t event) {
  static bool reset_in_progress = false;
  static bool short_press = false;

  if (event == BUTTON_EVENT_PRESSED) {
    reset_in_progress = false;
    short_press = true;
  } else if (event == BUTTON_EVENT_RELEASED) {
    if (short_press) {
      console_dump_debug_info();
    } else {
      if (!reset_in_progress) {
        log_print(F("GPIO: reset ABORTED!              "));
        led_on(LED_GRN);
      }
    }
  } else if (event == BUTTON_EVENT_HOLD_1) {
    log_print(F("GPIO: preparing for factory reset ..."));
    reset_in_progress = false;
    led_pulse(LED_GRN, 50, 50);
  } else if (event == BUTTON_EVENT_HOLD_2) {
    log_progress("GPIO: factory reset in ", " seconds", 5 - event);
  } else if (event == BUTTON_EVENT_HOLD_4) {
    log_print(F("GPIO: initiating factory reset ...          "));
    reset_in_progress = true;
    led_on(LED_GRN);
    led_on(LED_RED);
    config_reset();
    system_reboot();
  }
}

bool main_init(void) {
  gpio_register_button_cb(button_cb);

  console_init();
  system_init();
  logger_init();
  cli_init();
  i2c_init();
  rtc_init();
  gpio_init();
  led_init();
  net_init();
  ntp_init();
  webserver_init();
  websocket_init();
  telemetry_init();
  storage_init();
  update_init();
  mdns_init();
  telnet_init();

  led_on(LED_GRN);

  config_fini();
  bootup = false;

  log_print(F("MAIN: starting main loop ..."));

  return (true);
}

bool main_fini(void) {
  config_fini();
  ntp_fini();
  rtc_fini();
  mdns_fini();
  update_fini();
  storage_fini();
  telemetry_fini();
  websocket_fini();
  webserver_fini();
  fs_fini();
  cli_fini();
  led_fini();
  gpio_fini();
  telnet_fini();
  logger_fini();
  system_fini();
  //console_fini();
  //net_fini();

  return (true);
}

void main_loop(void) {
  config_poll();
  rtc_poll();
  websocket_poll();
  telemetry_poll();
  webserver_poll();
  update_poll();
  gpio_poll();
  ntp_poll();
  net_poll();
  mdns_poll();
  storage_poll();
  telnet_poll();
  console_poll();
  led_poll();
  system_poll();
  logger_poll();
  fs_poll();
}
