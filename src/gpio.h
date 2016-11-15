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

#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

// RTC pins
#define GPIO_SCL     2 // UART1 TX
#define GPIO_SDA    14

// ADE pins
#define GPIO_IRQ     5 

// SPI pins
#define GPIO_MISO   12
#define GPIO_MOSI   13
#define GPIO_SCLK   14
#define GPIO_SS     15

// USR pins
#define GPIO_BUTTON  0
#define GPIO_LED1    4
#define GPIO_LED2   16
#define GPIO_LED3    1 // UART0 TX
#define GPIO_LED4    3 // UART0 RX
#define GPIO_RELAIS 16

#define GPIO_LEDS    2

enum ButtonEvent_t {
  BUTTON_EVENT_RELEASED,
  BUTTON_EVENT_PRESSED,
  BUTTON_EVENT_HOLD_1,
  BUTTON_EVENT_HOLD_2,
  BUTTON_EVENT_HOLD_3,
  BUTTON_EVENT_HOLD_4,
  BUTTON_EVENT_HOLD_5,
  BUTTON_EVENT_HOLD_6,
  BUTTON_EVENT_HOLD_7,
  BUTTON_EVENT_HOLD_8,
  BUTTON_EVENT_HOLD_9,
  BUTTON_EVENT_HOLD,
  BUTTON_EVENTS
};

void gpio_register_button_cb(void (*)(uint16_t));

int gpio_state(void);
bool gpio_init(void);
bool gpio_fini(void);
void gpio_poll(void);

void gpio_high(uint8_t pin);
void gpio_low(uint8_t pin);
void gpio_toggle(uint8_t pin);

void gpio_led_on(uint8_t pin);
void gpio_led_off(uint8_t pin);
void gpio_led_toggle(uint8_t pin);
void gpio_led_pulse(uint8_t pin, uint16_t on, uint16_t off);
void gpio_led_flash(uint8_t pin, uint16_t ms = 100);

void gpio_relais_on(void);
void gpio_relais_off(void);
void gpio_relais_toggle(void);
void gpio_relais_state(bool &state);

#endif // _GPIO_H_
