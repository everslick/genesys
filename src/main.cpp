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
#include "transport.h"
#include "webserver.h"
#include "websocket.h"
#include "storage.h"
#include "console.h"
#include "at24c32.h"
#include "system.h"
#include "syslog.h"
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
        log_print(F("GPIO: reset ABORTED!              \r\n"));
        led_on(LED_GRN);
      }
    }
  } else if (event == BUTTON_EVENT_HOLD_1) {
    log_print(F("GPIO: preparing for ADE reset ...\r\n"));
    led_pulse(LED_GRN, 100, 100);
    short_press = false;
  } else if ((event == BUTTON_EVENT_HOLD_2) || (event == BUTTON_EVENT_HOLD_3)) {
    log_progress("GPIO: ADE reset in ", " seconds", 5 - event);
  } else if (event == BUTTON_EVENT_HOLD_4) {
    log_print(F("GPIO: initiating ADE reset ...          \r\n"));
    reset_in_progress = true;
    led_on(LED_GRN);
    //ade_zero();
  } else if (event == BUTTON_EVENT_HOLD_5) {
    log_print(F("GPIO: preparing for factory reset ...\r\n"));
    reset_in_progress = false;
    led_pulse(LED_GRN, 50, 50);
  } else if ((event == BUTTON_EVENT_HOLD_6) || (event == BUTTON_EVENT_HOLD_7)) {
    log_progress("GPIO: factory reset in ", " seconds", 9 - event);
  } else if (event == BUTTON_EVENT_HOLD_8) {
    log_print(F("GPIO: initiating factory reset ...          \r\n"));
    reset_in_progress = true;
    led_on(LED_GRN);
    led_on(LED_RED);
    config_reset();
    system_reboot();
  }
}

bool main_init(void) {
  gpio_register_button_cb(button_cb);

  cli_init();
  console_init();
  i2c_init();
  log_init();
  config_init();
  system_init();
  rtc_init();
  gpio_init();
  led_init();
  net_init();
  ntp_init();
  syslog_init();
  webserver_init();
  websocket_init();
  transport_init();
  storage_init();
  update_init();
  mdns_init();
  telnet_init();

  led_on(LED_GRN);

  config_fini();
  bootup = false;

  log_print(F("MAIN: starting main loop ...\r\n"));

  return (true);
}

bool main_fini(void) {
  config_fini();
  ntp_fini();
  rtc_fini();
  mdns_fini();
  update_fini();
  storage_fini();
  transport_fini();
  websocket_fini();
  webserver_fini();
  fs_fini();
  cli_fini();
  telnet_fini();
  syslog_fini();
  net_fini();
  led_fini();
  gpio_fini();
  log_fini();
  console_fini();
  system_fini();

  return (true);
}

void main_loop(void) {
  rtc_poll();
  websocket_poll();
  transport_poll();
  webserver_poll();
  update_poll();
  system_poll();
  gpio_poll();
  ntp_poll();
  net_poll();
  mdns_poll();
  storage_poll();
  telnet_poll();
  log_poll();
  syslog_poll();
  console_poll();
  led_poll();
}
