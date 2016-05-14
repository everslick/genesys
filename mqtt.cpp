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
#include <MQTT.h>

#include "config.h"
#include "system.h"
#include "log.h"
#include "net.h"

#include "mqtt.h"

static void (*event_cb)(uint16_t) = NULL;

static bool mqtt_is_connected = false;

static MQTT *mqtt = NULL;

static String topic;

static void ICACHE_FLASH_ATTR mqtt_connected_cb(void) {
  String t = topic + "setup";

  mqtt_is_connected = true;
  mqtt->subscribe(t);

  if (event_cb) {
    event_cb(MQTT_EVENT_CONNECTED);
  }
}

static void ICACHE_FLASH_ATTR mqtt_disconnected_cb(void) {
  mqtt_is_connected = false;

  if (event_cb) {
    event_cb(MQTT_EVENT_DISCONNECTED);
  }
}

static void ICACHE_FLASH_ATTR mqtt_published_cb(void) {
}

static void ICACHE_FLASH_ATTR mqtt_data_cb(String &topic, String &data) {
  log_print(F("MQTT: data %s:%s\n"), topic.c_str(), data.c_str());
}

static void ICACHE_FLASH_ATTR publish_debug_info(void) {
#ifndef RELEASE
  uint32_t stack = system_free_stack();
  uint32_t free = system_free_heap();
  uint32_t uptime = millis() / 1000;
  String t = topic + "debug";
  String msg;

  msg = String("{ ") +
    "\"uptime\":"  + String(uptime)                  + ", " +
    "\"heap\":"    + String(free, DEC)               + ", " +
    "\"stack\":"   + String(stack, DEC)              + ", " +
    "\"ssid\":\""  + net_ssid()                      + "\", " +
    "\"rssi\":"    + String(net_rssi())              + " }";

  mqtt->publish(t, msg, 1, 1); // publish + QOS = 1, retain = true
  system_count_net_traffic(msg.length());
#endif
}

static void ICACHE_FLASH_ATTR publish_poweron_info(void) {
  String t = topic + "poweron";

  String msg = String("{ ") +
    "\"client_id\":\"" + String(client_id)           + "\", " +
    "\"version\":\""   + String(FIRMWARE)            + "\" }";
 
  mqtt->publish(t, msg, 1, 1); // publish + QOS = 1, retain = true
  system_count_net_traffic(msg.length());
}

static void ICACHE_FLASH_ATTR publish_adc_value(void) {
  static bool first_call = true;
  String t = topic + "adc";
  String msg;

  if (!msg.reserve(256)) {
    log_print(F("failed to allocate memory\n"));
  }

  msg = "{ \"value\":" + String(analogRead(17)) + " }";

  if (first_call) {
    // first time around we get messy msgs
    // thus we just send a poweron message

    publish_poweron_info();

    first_call = false;
  } else {
    mqtt->publish(t, msg, 1, 1); // publish + QOS = 1, retain = true
  }

  system_count_net_traffic(msg.length());
}

void ICACHE_FLASH_ATTR mqtt_register_event_cb(void (*cb)(uint16_t)) {
  // set event callback function
  event_cb = cb;
}

bool ICACHE_FLASH_ATTR mqtt_init(void) {
  if (!config->mqtt_enabled) {
    log_print(F("MQTT: mqtt disabled in config"));
    return (false);
  }

  if (mqtt) return (false);

  mqtt = new MQTT(client_id, config->mqtt_server, 1883);

  topic = "/" + String(config->mqtt_user) + "/" + String(client_id) + "/";

  // Setup MQTT callbacks
  mqtt->onConnected(mqtt_connected_cb);
  mqtt->onDisconnected(mqtt_disconnected_cb);
  mqtt->onPublished(mqtt_published_cb);
  mqtt->onData(mqtt_data_cb);

  mqtt->setUserPwd(config->mqtt_user, config->mqtt_pass);

  mqtt->connect();

  return (true);
}

bool ICACHE_FLASH_ATTR mqtt_poll(void) {
  static uint32_t ms = 0;

  if (!config->mqtt_enabled) return (false);

  if (!mqtt) return (false);

  if (mqtt_is_connected) {
    if ((millis() - ms) / 1000 > config->mqtt_interval) {
      if (event_cb) {
        event_cb(MQTT_EVENT_PUBLISH);
      }

      publish_adc_value();
      publish_debug_info();

      if (event_cb) {
        event_cb(MQTT_EVENT_PUBLISHED);
      }

      ms = millis();
    }
  }

  return (true);
}
