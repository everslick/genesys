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

#include "transport.h"
#include "gpio.h"
#include "net.h"

#include "led.h"

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
  static bool transport_is_connected = true;
  static bool       net_is_connected = true;
  static bool       net_is_enabled   = true;

  if (net_is_enabled && !net_enabled()) {
    net_is_enabled = false;

    led_off(LED_YEL);

    return;
  }

  if (!net_is_enabled && net_enabled()) {
    net_is_enabled = true;

    led_pulse(LED_YEL, NET_LED_BLINK_PERIOD, NET_LED_BLINK_PERIOD);

    return;
  }

  if (!net_is_enabled) return;

  if (transport_is_connected && !transport_connected()) {
    transport_is_connected = false;

    led_pulse(LED_YEL, TRANSPORT_LED_BLINK_PERIOD, TRANSPORT_LED_BLINK_PERIOD);

    return;
  }

  if (!transport_is_connected && transport_connected()) {
    transport_is_connected = true;

    led_off(LED_YEL);

    return;
  }

  if (net_is_connected && !net_connected()) {
    net_is_connected = false;

    led_pulse(LED_YEL, NET_LED_BLINK_PERIOD, NET_LED_BLINK_PERIOD);

    return;
  }

  if (!net_is_connected && net_connected()) {
    net_is_connected = true;

    led_off(LED_YEL);

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
  gpio_led_pulse(led, on, off);
}

void led_flash(int led, uint16_t ms) {
  gpio_led_flash(led, ms);
}
