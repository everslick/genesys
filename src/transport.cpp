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

#include <ESP8266HTTPClient.h>

#include "config.h"
#include "system.h"
#include "module.h"
#include "mqtt.h"
#include "log.h"
#include "net.h"
#include "rtc.h"
#include "led.h"

#include "transport.h"

#define BUFFER_SIZE 512

struct TRANSPORT_PrivateData {
  // MQTT members
  MQTT *mqtt;
  bool mqtt_is_connected;
  char mqtt_topic[36];

  // HTTP members
  HTTPClient *http;
  bool http_is_connected;
  uint32_t http_ip;

  // time in seconds to wait before connecting again
  uint16_t reconnection_delay;

  // settings from config
  uint8_t protocol;
  char url[64];
  char user[16];
  char pass[32];
  uint32_t interval;

  // terminate module in poll()
  bool shutdown;
};

static TRANSPORT_PrivateData *p = NULL;

// split str into 'proto://auth@host:port/path?query'

static bool parse_url(const String &str,
                            String &proto,
                            String &auth,
                            String &host,
                            String &path,
                            String &query,
                            uint16_t &port) {
  String url = str;
  int index = -1;

  // check for ':' (http: or https:)
  index = url.indexOf(':');
  if (index >= 0) {
    proto = url.substring(0, index);
    url.remove(0, (index + 3)); // remove http:// or https://
  } else {
    proto = "";
  }

  if (proto != F("http")) {
    // unsupported protocol
    return (false);
  }

  // check for '@' (user:pass)
  index = url.indexOf('@');
  if (index >= 0) {
    auth = url.substring(0, index);
    url.remove(0, index + 1); // remove auth part including '@'
  } else {
    auth = "";
  }

  // check for '/' or '?' (host name)
  int ind1 = url.indexOf('/');
  int ind2 = url.indexOf('?');

  if ((ind1 < 0) && (ind2 < 0)) {
    // neither '/' nor '?' was found
    host = url;
    url = "";
  } else {
    if ((ind1 >= 0) && (ind2 >= 0)) {
      index = min(ind1, ind2);
    } else {
      index = max(ind1, ind2);
    }
    host = url.substring(0, index);
    if (index == ind1) url.remove(0, index);     // remove host but not '/'
    if (index == ind2) url.remove(0, index + 1); // remove host including '?'
  }

  if (host.length() == 0) {
    // no hostname specified
    return (false);
  }

  // check for ':' (port)
  index = host.indexOf(':');
  if (index >= 0) {
    String name = host.substring(0, index);
    host.remove(0, index + 1); // remove host including ':'
    port = host.toInt();       // get port number
    host = name;
  } else {
         if (proto == F("ftp"))   port = 21;
    else if (proto == F("http"))  port = 80;
    else if (proto == F("https")) port = 443;
    else                          port = 0;
  }

  if (host.length() == 0) {
    // no hostname specified
    return (false);
  }

  // check for '?' (path)
  index = url.indexOf('?');
  if (index >= 0) {
    path = url.substring(0, index);
    url.remove(0, index + 1); // remove path including '?'

    // what is left is the query string
    query = url;
  } else {
    path = url;
  }

  return (true);
}

static void receive_cb(char *mqtt_topic, byte *payload, unsigned int length) {
  char buf[length + 1];
  char *c = buf;

  for (int i=0; i<length; i++) *c++ = payload[i];
  *c = '\0';

  log_print(F("MQTT: message [%s] %s\r\n"), mqtt_topic, buf);
}

static void publish(const String &t, const String &m) {
  led_flash(LED_YEL);

  p->mqtt->publish(t.c_str(), m.c_str(), true); // true = retained

  if (m.length() + t.length() + 5 + 2 > MQTT_MAX_PACKET_SIZE) {
    log_print(F("MQTT: packet of %i bytes is too big\r\n"), m.length());
  }
}

static void publish_adc_value(void) {
  String m, t = p->mqtt_topic;

  if (!m.reserve(BUFFER_SIZE)) {
    log_print(F("MQTT: failed to allocate memory\r\n"));
  }

  t += F("values");
  m += String(F("{ \"version\":1, ")) +
    F("\"time\":")          + String(0) + F(", ")   +
    F("\"msec\":")          + String(0) + F(", ")   +
    F("\"device_id\":\"")   + String(device_id)             + F("\", ") +
    F("\"device_name\":\"") + String(system_device_name())  + F("\", ");

  m += String(F("\"adc\":"))  + String(0) + F(", ");
  m += String(F("\"temp\":")) + String(rtc_temp())    + F(" }");

  publish(t, m);
}

static void publish_poweron_info(void) {
  String m, t = p->mqtt_topic;

  if (!m.reserve(BUFFER_SIZE)) {
    log_print(F("MQTT: failed to allocate memory\r\n"));
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
  String m, t = p->mqtt_topic;

  if (!m.reserve(BUFFER_SIZE)) {
    log_print(F("MQTT: failed to allocate memory\r\n"));
  }

  t += F("debug");
  m += String(F("{ "))      +
    F("\"device_id\":\"")   + String(device_id)            + F("\", ") +
    F("\"device_name\":\"") + String(system_device_name()) + F("\", ") +
    F("\"uptime\":")        + String(uptime)               + F(", ")   +
    F("\"heap\":")          + String(free, DEC)            + F(", ")   +
    F("\"stack\":")         + String(stack, DEC)           + F(", ")   +
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

      log_print(F("MQTT: disconnected from broker\r\n"));
    } else {
      // try to connect every so often
      if (millis() - ms > p->reconnection_delay * 1000) {
        ms = millis();

        if (p->mqtt->connect(device_id, p->user, p->pass)) {
          String t = p->mqtt_topic;
          t += F("setup");

          p->mqtt_is_connected = true;
          p->mqtt->subscribe(t);

          log_print(F("MQTT: connected to broker (%s)\r\n"), p->url);

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

    publish_adc_value();
    publish_debug_info();
  }
}

bool transport_connected(void) {
  if (!p) return (false);

  return (p->mqtt && p->mqtt_is_connected);
}

int transport_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool transport_init(void) {
  String str;

  if (p) return (false);

  config_init();

  if (bootup && !config->transport_enabled) {
    log_print(F("TRNS: transport disabled in config\r\n"));

    config_fini();

    return (false);
  }

  log_print(F("TRNS: initializing transport\r\n"));

  p = (TRANSPORT_PrivateData *)malloc(sizeof (TRANSPORT_PrivateData));
  memset(p, 0, sizeof (TRANSPORT_PrivateData));

  p->interval = config->transport_interval;

  config_get(F("transport_url"),  str);
  strncpy(p->url,  str.c_str(), sizeof (p->url));
  config_get(F("transport_user"), str);
  strncpy(p->user, str.c_str(), sizeof (p->user));
  config_get(F("transport_pass"), str);
  strncpy(p->pass, str.c_str(), sizeof (p->pass));
 
  config_fini();

  // initially we try to connect fast
  p->reconnection_delay = 5;

  // MQTT protocol
  p->mqtt = new MQTT(p->url);
  p->mqtt->ReceiveCallback(receive_cb);

  // format the MQTT topic string
  snprintf_P(p->mqtt_topic, sizeof (p->mqtt_topic), PSTR("%s/%s/"),
    p->user, device_id
  );

  return (true);
}

bool transport_fini(void) {
  if (!p) return (false);

  log_print(F("TRNS: closing transport\r\n"));

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

void transport_poll(void) {
  if (p && net_connected()) {
    poll_mqtt_connection();

    poll_publish();

    if (p->shutdown) {
      log_print(F("TRNS: disabling transport until next reboot\r\n"));

      transport_fini();
    }
  }
}

MODULE(transport)
