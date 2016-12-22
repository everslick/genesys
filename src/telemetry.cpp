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
#include "config.h"
#include "system.h"
#include "module.h"
#include "clock.h"
#include "mqtt.h"
#include "log.h"
#include "net.h"
#include "led.h"
#include "rtc.h"

#include "telemetry.h"

#define BUFFER_SIZE 512

#define SERVER_FINGERPRINT \
  F("26 96 1C 2A 51 07 FD 15 80 96 93 AE F7 32 CE B9 0D 01 55 C4")

struct TELEMETRY_PrivateData {
  // MQTT members
  MQTT *mqtt;
  bool mqtt_is_connected;
  char mqtt_topic[36];

  // time in seconds to wait before connecting again
  uint16_t reconnection_delay;

  // settings from config
  char url[64];
  char user[17];
  char pass[33];
  uint32_t interval;

  // ADE values
  uint32_t measurements;

  // terminate module in poll()
  bool shutdown;
};

static TELEMETRY_PrivateData *p = NULL;

static void receive_cb(char *mqtt_topic, byte *payload, unsigned int length) {
  char buf[length + 1];
  char *c = buf;

  for (int i=0; i<length; i++) *c++ = payload[i];
  *c = '\0';

  log_print(F("MQTT: message [%s] %s"), mqtt_topic, buf);
}

static void publish(const String &t, const String &m) {
  led_flash(LED_YEL);

  p->mqtt->publish(t.c_str(), m.c_str(), true); // true = retained

  if (m.length() + t.length() + 5 + 2 > MQTT_MAX_PACKET_SIZE) {
    log_print(F("MQTT: packet of %i bytes is too big"), m.length());
  }
}

static void publish_protocol_mqtt(void) {
  String m, t = p->mqtt_topic;
  struct timespec tm;

  if (!m.reserve(BUFFER_SIZE)) {
    log_print(F("MQTT: failed to allocate memory"));
  }

  clock_gettime(CLOCK_REALTIME, &tm);

  t += F("values");
  m += F("{ \"version\":1, \"time\":");
  m += String(tm.tv_sec) + F(", \"msec\":");
  m += String(tm.tv_nsec / 1000000) + F(", ");
  m += F("\"device_id\":\"");
  m += String(device_id) + F("\", \"device_name\":\"");
  m += String(system_device_name()) + F("\", ");
  m += String(F("\"adc\":")) + String(analogRead(17)) + F(", ");
  m += String(F("\"temp\":")) + String(rtc_temp())    + F(" }");

  publish(t, m);
}

static void publish_poweron_info(void) {
  String m, t = p->mqtt_topic;

  if (!m.reserve(BUFFER_SIZE)) {
    log_print(F("MQTT: failed to allocate memory"));
  }

  t += F("poweron");
  m += String(F("{ "))      +
    F("\"device_id\":\"")   + String(device_id)            + F("\", ") +
    F("\"device_name\":\"") + String(system_device_name()) + F("\", ") +
    F("\"device\":\"")      + String(system_hw_device())   + F("\", ") +
    F("\"hw_version\":\"")  + String(system_hw_version())  + F("\", ") +
    F("\"sw_version\":\"")  + String(FIRMWARE)             + F("\" }");
 
  publish(t, m);
}

static void publish_debug_info(void) {
  if ((!p->mqtt) || (!p->mqtt_is_connected)) return;

  uint32_t stack = system_free_stack();
  uint32_t free = system_free_heap();
  uint32_t uptime = millis() / 1000;
  int total, used, unused = -1;
  String m, t = p->mqtt_topic;

  fs_usage(total, used, unused);

  if (!m.reserve(BUFFER_SIZE)) {
    log_print(F("MQTT: failed to allocate memory"));
  }

  t += F("debug");
  m += String(F("{ "))      +
    F("\"device_id\":\"")   + String(device_id)            + F("\", ") +
    F("\"device_name\":\"") + String(system_device_name()) + F("\", ") +
    F("\"uptime\":")        + String(uptime)               + F(", ")   +
    F("\"heap\":")          + String(free, DEC)            + F(", ")   +
    F("\"stack\":")         + String(stack, DEC)           + F(", ")   +
    F("\"fs\":")            + String(unused, DEC)          + F(", ")   +
    F("\"rssi\":")          + String(net_rssi())           + F(" }");

  publish(t, m);
}

static void poll_mqtt_connection(void) {
  static uint32_t ms = millis();

  if (p->mqtt && (!p->mqtt->loop())) {
    // client is not connected
    if (p->mqtt_is_connected) {
      // but we havn't sent the event yet
      p->mqtt_is_connected = false;
      p->reconnection_delay = 15;

      log_print(F("MQTT: disconnected from broker"));
    } else {
      // try to connect every so often
      if (millis() - ms > p->reconnection_delay * 1000) {
        bool connected = false;
        ms = millis();

        connected = p->mqtt->connect(device_id, p->user, p->pass);

        if (connected) {  
          String t = p->mqtt_topic;
          t += F("setup");

          p->mqtt_is_connected = true;
          p->mqtt->subscribe(t);

          log_print(F("MQTT: connected to broker (%s)"), p->url);

          publish_poweron_info();
        }
      }
    }
  }
}

static void poll_publish(void) {
  static uint32_t ms = millis();

  if (millis() - ms > p->interval * 1000) {
    ms = millis();

    publish_protocol_mqtt();
    publish_debug_info();
  }
}

bool telemetry_connected(void) {
  if (!p) return (false);

  return (p->mqtt && p->mqtt_is_connected);
}

int telemetry_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool telemetry_init(void) {
  String str;

  if (p) return (false);

  config_init();

  if (bootup && !config->telemetry_enabled) {
    log_print(F("TELE: telemetry disabled in config"));

    config_fini();

    return (false);
  }

  log_print(F("TELE: initializing telemetry (MQTT)"));

  p = (TELEMETRY_PrivateData *)malloc(sizeof (TELEMETRY_PrivateData));
  memset(p, 0, sizeof (TELEMETRY_PrivateData));

  p->interval = config->telemetry_interval;

  config_get(F("telemetry_url"),  str);
  strncpy(p->url,  str.c_str(), sizeof (p->url));
  config_get(F("telemetry_user"), str);
  strncpy(p->user, str.c_str(), sizeof (p->user));
  config_get(F("telemetry_pass"), str);
  strncpy(p->pass, str.c_str(), sizeof (p->pass));
 
  config_fini();

  // initially we try to connect fast
  p->reconnection_delay = 5;

  p->mqtt = new MQTT(p->url, MQTT_PORT, SERVER_FINGERPRINT);
  p->mqtt->ReceiveCallback(receive_cb);

  // format the MQTT topic string
  snprintf_P(p->mqtt_topic, sizeof (p->mqtt_topic), PSTR("%s/%s/"),
    p->user, device_id
  );

  return (true);
}

bool telemetry_fini(void) {
  if (!p) return (false);

  log_print(F("TELE: closing telemetry"));

  if (p->mqtt) {
    // MQTT based protocols

    String t = p->mqtt_topic;
    t += F("setup");

    p->mqtt->unsubscribe(t);

    if (p->mqtt->connected()) {
      p->mqtt->disconnect();
    }

    delete (p->mqtt);
  }

  // free private data
  free(p);
  p = NULL;

  return (true);
}

void telemetry_poll(void) {
  if (p && net_connected()) {
    poll_mqtt_connection();

    poll_publish();

    if (p->shutdown) {
      log_print(F("TELE: disabling telemetry until next reboot"));

      telemetry_fini();
    }
  }
}

MODULE(telemetry)
