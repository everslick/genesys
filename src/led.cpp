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
#include "gpio.h"
#include "net.h"

#include "led.h"

static void red_led_poll(void) {
  static bool led = false;

  if (!led && fs_full()) {
    led_pulse(LED_RED, ERROR_LED_BLINK_PERIOD);
    led = true;
  }

  if (led && !fs_full()) {
    led_off(LED_RED);
    led = false;
  }
}

static void grn_led_poll(void) {
  static uint32_t ms = millis();
  static bool led = false;

  uint16_t duration = led ? GRN_LED_ON_MS : GRN_LED_OFF_MS;

  if ((millis() - ms) > duration) {
    ms = millis();

    if (led) {
      led_off(LED_GRN);
      led = false;
    } else {
      led_on(LED_GRN);
      led = true;
    }
  }
}

static void yel_led_poll(void) {
  static bool telemetry_is_enabled   = telemetry_enabled();
  static bool       net_is_enabled   = net_enabled();
  static bool telemetry_is_connected = true;
  static bool       net_is_connected = true;

  // WiFi

  if (net_is_enabled != net_enabled()) {
    if (net_is_enabled && !net_enabled()) {
      net_is_enabled = false;

      led_off(LED_YEL);

      return;
    }

    if (!net_is_enabled && net_enabled()) {
      net_is_enabled = true;

      led_pulse(LED_YEL, NET_LED_BLINK_PERIOD);

      return;
    }
  }

  if (!net_is_enabled) return;

  if (!net_is_connected && net_connected()) {
    net_is_connected = true;

    led_off(LED_YEL);

    return;
  }

  if (net_is_connected && !net_connected()) {
    net_is_connected = false;

    led_pulse(LED_YEL, NET_LED_BLINK_PERIOD);

    return;
  }

  // MQTT

  if (telemetry_is_enabled != telemetry_enabled()) {
    if (telemetry_is_enabled && !telemetry_enabled()) {
      telemetry_is_enabled = false;

      led_off(LED_YEL);

      return;
    }

    if (!telemetry_is_enabled && telemetry_enabled()) {
      telemetry_is_enabled = true;

      led_pulse(LED_YEL, TELEMETRY_LED_BLINK_PERIOD);

      return;
    }
  }

  if (!telemetry_is_enabled) return;

  if (!telemetry_is_connected && telemetry_connected()) {
    telemetry_is_connected = true;

    led_off(LED_YEL);

    return;
  }

  if (telemetry_is_connected && !telemetry_connected()) {
    telemetry_is_connected = false;

    led_pulse(LED_YEL, TELEMETRY_LED_BLINK_PERIOD);

    return;
  }
}

bool led_init(void) {
  led_off(LED_GRN);
  led_off(LED_YEL);
  led_off(LED_RED);

  return (true);
}

bool led_fini(void) {
  led_off(LED_GRN);
  led_off(LED_YEL);
  led_off(LED_RED);

  return (true);
}

void led_poll(void) {
  grn_led_poll();
  yel_led_poll();
  red_led_poll();
}

void led_on(int led) {
  gpio_led_on(led);
}

void led_off(int led) {
  gpio_led_off(led);
}

void led_toggle(int led) {
  gpio_led_toggle(led);
}

void led_pulse(int led, uint16_t on, uint16_t off) {
  if (off == 0) off = on;

  gpio_led_pulse(led, on, off);
}

void led_flash(int led, uint16_t ms) {
  gpio_led_flash(led, ms);
}
