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

#include "system.h"
#include "module.h"
#include "config.h"
#include "log.h"

#include "gpio.h"

#define BUTTON_PRESSED  LOW
#define BUTTON_RELEASED HIGH
#define BUTTON_DEBOUNCE_TIME 20

static void (*button_cb)(uint16_t) = NULL;

enum {
  LED_STATE_OFF,   // permanently off
  LED_STATE_ON,    // permanently on
  LED_STATE_PULSE, // permanently pulsing
  LED_STATE_FLASH  // one shot pulse
};

struct GPIO_PrivateData {
  uint8_t  button_state      = BUTTON_RELEASED;
  uint8_t  button_last_state = BUTTON_RELEASED;
  uint32_t button_last_time  = 0;
  uint32_t button_hold_state = BUTTON_RELEASED;
  uint32_t button_hold_start = 0;

  bool event_sent[BUTTON_EVENTS];

  uint32_t led_last_toggle_time[GPIO_LEDS];
  uint16_t led_on_time[GPIO_LEDS];
  uint16_t led_off_time[GPIO_LEDS];
  uint8_t  led_state[GPIO_LEDS];
};

static GPIO_PrivateData *p = NULL;

static uint8_t gpio_led_pin(uint8_t led) {
  if (led == 0) return (GPIO_LED1);
  if (led == 1) return (GPIO_LED2);
  if (led == 2) return (GPIO_LED3);

  return (GPIO_LED3);
}

void gpio_register_button_cb(void (*cb)(uint16_t)) {
  // set button callback function
  button_cb = cb;
}

int gpio_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool gpio_init(void) {
  if (p) return (false);

  config_init();

  if (bootup) {
    if (!config->gpio_enabled) {
      log_print(F("GPIO: leds and buttons disabled in config"));

      config_fini();

      return (false);
    }
  }

  config_fini();

  log_print(F("GPIO: activating leds and buttons"));

  p = (GPIO_PrivateData *)malloc(sizeof (GPIO_PrivateData));
  memset(p, 0, sizeof (GPIO_PrivateData));

  pinMode(GPIO_BUTTON, INPUT);
  pinMode(GPIO_RELAIS, OUTPUT);

  p->button_state      = BUTTON_RELEASED;
  p->button_last_state = BUTTON_RELEASED;
  p->button_last_time  = 0;
  p->button_hold_state = BUTTON_RELEASED;
  p->button_hold_start = 0;

  for (int i=0; i<BUTTON_EVENTS; i++) {
    p->event_sent[i] = BUTTON_RELEASED;
  }

  for (int led=0; led<GPIO_LEDS; led++) {
    pinMode(gpio_led_pin(led), OUTPUT);
    digitalWrite(gpio_led_pin(led), HIGH); // inverted = led off
    p->led_state[led] = LED_STATE_OFF;
  }

  digitalWrite(GPIO_RELAIS, LOW);

  return (true);
}

bool gpio_fini(void) {
  if (!p) return (false);

  log_print(F("GPIO: deactivating leds and buttons"));

  for (int led=0; led<GPIO_LEDS; led++) {
    digitalWrite(gpio_led_pin(led), HIGH); // inverted = led off
    pinMode(gpio_led_pin(led), INPUT);
  }

  digitalWrite(GPIO_RELAIS, LOW);
  pinMode(GPIO_RELAIS, INPUT);

  // free private data
  free(p);
  p = NULL;

  return (true);
}

void gpio_poll(void) {
  if (!p) return;

  int state = digitalRead(GPIO_BUTTON);

  if (state != p->button_last_state) {
    p->button_last_time = millis();
  }

  if ((millis() - p->button_last_time) > BUTTON_DEBOUNCE_TIME) {
    if (state != p->button_state) {
      p->button_state = state;

      if (p->button_state == BUTTON_PRESSED) {
        p->button_hold_state = BUTTON_PRESSED;
        p->button_hold_start = millis();

        for (int ev=0; ev<BUTTON_EVENTS; ev++) {
          p->event_sent[ev] = false;
        }

        if (button_cb) button_cb(BUTTON_EVENT_PRESSED);
      } else {
        p->button_hold_state = BUTTON_RELEASED;

        if (button_cb) button_cb(BUTTON_EVENT_RELEASED);
      }
    }
  }

  p->button_last_state = state;

  if (p->button_hold_state == BUTTON_PRESSED) {
    int hold = millis() - p->button_hold_start;

    for (int i=0; i<10; i++) {
      if ((hold > i * 1000 + 1000) && (!p->event_sent[i + BUTTON_EVENT_HOLD_1])) {
        if (button_cb) button_cb((ButtonEvent_t)(i + BUTTON_EVENT_HOLD_1));
        p->event_sent[i + BUTTON_EVENT_HOLD_1] = true;
      }
    }
  }

  for (int led=0; led<GPIO_LEDS; led++) {
    bool state = !digitalRead(gpio_led_pin(led)); // inverted

    if (p->led_state[led] == LED_STATE_PULSE) {
      if (state) {
        if ((millis() - p->led_last_toggle_time[led]) > p->led_on_time[led]) {
          gpio_led_toggle(led);
        }
      } else {
        if ((millis() - p->led_last_toggle_time[led]) > p->led_off_time[led]) {
          gpio_led_toggle(led);
        }
      }
    } else if (p->led_state[led] == LED_STATE_FLASH) {
      if ((millis() - p->led_last_toggle_time[led]) > p->led_on_time[led]) {
        p->led_state[led] = LED_STATE_OFF;
        gpio_led_toggle(led);
      }
    }
  }
}

void gpio_high(uint8_t pin) {
  if (p) digitalWrite(pin, HIGH);
}

void gpio_low(uint8_t pin) {
  if (p) digitalWrite(pin, LOW);
}

void gpio_toggle(uint8_t pin) {
  if (p) digitalWrite(pin, !digitalRead(pin));
}

void gpio_led_on(uint8_t led) {
  if (!p || led >= GPIO_LEDS) return;

  digitalWrite(gpio_led_pin(led), LOW); // inverted

  p->led_state[led] = LED_STATE_ON;
}

void gpio_led_off(uint8_t led) {
  if (!p || led >= GPIO_LEDS) return;

  digitalWrite(gpio_led_pin(led), HIGH); // inverted

  p->led_state[led] = LED_STATE_OFF;
}

void gpio_led_toggle(uint8_t led) {
  if (!p || led >= GPIO_LEDS) return;

  digitalWrite(gpio_led_pin(led), !digitalRead(gpio_led_pin(led)));

  p->led_last_toggle_time[led] = millis();
}

void gpio_led_pulse(uint8_t led, uint16_t on, uint16_t off) {
  if (!p || led >= GPIO_LEDS) return;

  p->led_state[led] = LED_STATE_PULSE;
  p->led_on_time[led] = on;
  p->led_off_time[led] = off;
  p->led_last_toggle_time[led] = millis();
}

void gpio_led_flash(uint8_t led, uint16_t ms) {
  if (!p || led >= GPIO_LEDS) return;

  p->led_state[led] = LED_STATE_FLASH;
  p->led_on_time[led] = ms;
  digitalWrite(gpio_led_pin(led), LOW); // inverted
  p->led_last_toggle_time[led] = millis();
}

void gpio_relais_on(void) {
  if (!p) return;

  if (!digitalRead(GPIO_RELAIS)) {
    digitalWrite(GPIO_RELAIS, HIGH);
    log_print(F("GPIO: relais on pin %i is on"), GPIO_RELAIS);
  }
}

void gpio_relais_off(void) {
  if (!p) return;

  if (digitalRead(GPIO_RELAIS)) {
    digitalWrite(GPIO_RELAIS, LOW);
    log_print(F("GPIO: relais on pin %i is off"), GPIO_RELAIS);
  }
}

void gpio_relais_toggle(void) {
  if (!p) return;

  digitalWrite(GPIO_RELAIS, !digitalRead(GPIO_RELAIS));
  log_print(F("GPIO: relais on pin %i is %s"),
    GPIO_RELAIS, (digitalRead(GPIO_RELAIS)) ? "on" : "off"
  );
}

void gpio_relais_state(bool &state) {
  state = digitalRead(GPIO_RELAIS);
}

MODULE(gpio)
