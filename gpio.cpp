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

#include "log.h"
#include "gpio.h"

#define BUTTON_PRESSED  LOW
#define BUTTON_RELEASED HIGH

#define BUTTON_DEBOUNCE_TIME 20

static void (*button_cb)(uint16_t) = NULL;

static uint8_t  button_state      = BUTTON_RELEASED;
static uint8_t  button_last_state = BUTTON_RELEASED;
static uint32_t button_last_time  = 0;
static uint32_t button_hold_state = BUTTON_RELEASED;
static uint32_t button_hold_start = 0;

static uint16_t led_toggle_period[16];
static uint32_t led_last_toggle_time[16];

static bool event_sent[BUTTON_EVENTS];

static bool gpio_is_led(uint8_t led) {
  if (led == GPIO_LED0) return (true);
  if (led == GPIO_LED1) return (true);

  return (false);
}

void gpio_register_button_cb(void (*cb)(uint16_t)) {
  // set button callback function
  button_cb = cb;
}

void gpio_init(void) {
  pinMode(GPIO_BUTTON, INPUT);

  for (int led=0; led<16; led++) {
    if (gpio_is_led(led)) {
      pinMode(led, OUTPUT);
      gpio_led_off(led);
    }
  }

  pinMode(GPIO_RELAIS, OUTPUT);
  gpio_relais_off();
}

void gpio_poll(void) {
  int state = digitalRead(GPIO_BUTTON);

  if (state != button_last_state) {
    button_last_time = millis();
  }

  if ((millis() - button_last_time) > BUTTON_DEBOUNCE_TIME) {
    if (state != button_state) {
      button_state = state;

      if (button_state == BUTTON_PRESSED) {
        button_hold_state = BUTTON_PRESSED;
        button_hold_start = millis();

        for (int ev=0; ev<BUTTON_EVENTS; ev++) {
          event_sent[ev] = false;
        }

        if (button_cb) button_cb(BUTTON_EVENT_PRESSED);
      } else {
        button_hold_state = BUTTON_RELEASED;

        if (button_cb) button_cb(BUTTON_EVENT_RELEASED);
      }
    }
  }

  button_last_state = state;

  for (int led=0; led<16; led++) {
    if (gpio_is_led(led) && (led_toggle_period[led] != 0)) {
      if ((millis() - led_last_toggle_time[led]) > led_toggle_period[led] / 2) {
        gpio_led_toggle(led);
      }
    }
  }

  if (button_hold_state == BUTTON_PRESSED) {
    int hold = millis() - button_hold_start;

    for (int i=0; i<10; i++) {
      if ((hold > i * 1000 + 1000) && (!event_sent[i + BUTTON_EVENT_HOLD_1])) {
        if (button_cb) button_cb((ButtonEvent_t)(i + BUTTON_EVENT_HOLD_1));
        event_sent[i + BUTTON_EVENT_HOLD_1] = true;
      }
    }
  }
}

void gpio_led_on(uint8_t led) {
  if (gpio_is_led(led)) {
    digitalWrite(led, HIGH);
    led_toggle_period[led] = 0;
  }
}

void gpio_led_off(uint8_t led) {
  if (gpio_is_led(led)) {
    digitalWrite(led, LOW);
    led_toggle_period[led] = 0;
  }
}

void gpio_led_toggle(uint8_t led) {
  if (gpio_is_led(led)) {
    digitalWrite(led, !digitalRead(led));
    led_last_toggle_time[led] = millis();
  }
}

void gpio_led_blink(uint8_t led, uint16_t ms) {
  if (gpio_is_led(led)) {
    gpio_led_on(led);
    led_toggle_period[led] = ms;
    led_last_toggle_time[led] = millis();
  }
}

void gpio_relais_on(void) {
  digitalWrite(GPIO_RELAIS, HIGH);
  log_print(F("GPIO: relais on pin %i is on\n"), GPIO_RELAIS);
}

void gpio_relais_off(void) {
  digitalWrite(GPIO_RELAIS, LOW);
  log_print(F("GPIO: relais on pin %i is off\n"), GPIO_RELAIS);
}

void gpio_relais_toggle(void) {
  digitalWrite(GPIO_RELAIS, !digitalRead(GPIO_RELAIS));
  log_print(F("GPIO: relais on pin %i toggles\n"), GPIO_RELAIS);
}
