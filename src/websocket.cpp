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

#include <WebSocketsServer.h>

#include <limits.h>

#include "logger.h"
#include "system.h"
#include "module.h"
#include "config.h"
#include "clock.h"
#include "util.h"
#include "gpio.h"
#include "log.h"
#include "rtc.h"

#include "websocket.h"

//#define LOG_CLIENT_CONNECTS
//#define LOG_BUFFER_USAGE

// install arduinoWebsockets into Arduino/libraries:
// git clone git@github.com:Links2004/arduinoWebSockets.git

enum {
  CLIENT_REQUEST_NONE,
  CLIENT_REQUEST_INIT,
  CLIENT_REQUEST_FINI,
  CLIENT_REQUEST_REBOOT,
  CLIENT_REQUEST_STATE,
  CLIENT_REQUEST_RELAIS,
  CLIENT_REQUEST_TEMP,
  CLIENT_REQUEST_TIME,
  CLIENT_REQUEST_LOAD,
  CLIENT_REQUEST_LOG,
  CLIENT_REQUEST_ADC
};

struct WS_PrivateData {
  int client_request[WEBSOCKETS_SERVER_CLIENT_MAX];

  WebSocketsServer *websocket = NULL;

  uint32_t packet_reserved;
  char packet_purpose[16];

  uint8_t module;
};

static WS_PrivateData *p = NULL;

// global send buffer
static String data;

static void packet_prepare(uint32_t reserve, const char *purpose = NULL) {
  strncpy_P(p->packet_purpose, purpose, sizeof (p->packet_purpose));
  p->packet_reserved = reserve;

  if (!data.reserve(reserve)) {
    log_print(F("WS:   could not allocate %s buffer"), p->packet_purpose);
  }

  data = F("              "); // 14 bytes reserved for header
}

static void packet_check(void) {
  if (data.length() >= p->packet_reserved) {
    log_print(F("WS:   %s buffer too small (size=%i, need=%i)"),
      p->packet_purpose, p->packet_reserved, data.length()
    );
  }

#ifdef LOG_BUFFER_USAGE
  log_print(F("WS:   %s buffer = %i bytes"), p->packet_purpose, data.length());
#endif

  p->packet_purpose[0] = '\0';
  p->packet_reserved = 0;

  data = String();
}

static void packet_send(int client) {
  int len = data.length() - WEBSOCKETS_MAX_HEADER_SIZE;

  p->websocket->sendTXT(client, (char *)data.c_str(), len, true);

  packet_check();
}

static void packet_broadcast(void) {
  int len = data.length() - WEBSOCKETS_MAX_HEADER_SIZE;

  p->websocket->broadcastTXT((char *)data.c_str(), len, true);

  packet_check();
}

static void send_time_data(int client) {
  char time[16], uptime[24];
  struct timespec tm;

  packet_prepare(150, PSTR("TIME"));

  clock_gettime(CLOCK_REALTIME, &tm);

  data += F("{\"type\":\"time\",");

  // localtime in milli seconds
  data += F("\"localtime\":");
  data += system_localtime() * 1000.0 + tm.tv_nsec / 1000000;
  data += F(",");

  // uptime as string
  data += F("\"uptime\":\"");
  data += system_uptime(uptime);
  data += F("\",");

  // UTC as string
  data += F("\"utc\":\"");
  data += system_time(time);
  data += F("\"}");

  packet_send(client);
}

static void send_adc_data(int client) {
  packet_prepare(100, PSTR("ADC"));

  data += F("{\"type\":\"adc\",\"value\":");
  data += String(analogRead(17));
  data += F("}");

  packet_send(client);
}

static void send_relais_data(int client) {
  bool state;

  packet_prepare(100, PSTR("RELAIS"));

  gpio_relais_state(state);

  data += F("{\"type\":\"relais\",\"value\":");
  data += String(state);
  data += F("}");

  packet_send(client);
}

static void send_load_data(int client) {
#ifdef ALPHA
  String cpu_data, mem_data, net_data;

  packet_prepare(450, PSTR("LOAD"));

  for (int i=0; i<system_load_history_entries(); i++) {
    SysLoad load = system_load_history(i);

    cpu_data += String(load.cpu);
    mem_data += String(load.mem);
    net_data += String(load.net);

    if (i != system_load_history_entries() - 1) {
      cpu_data += ','; mem_data += ','; net_data += ',';
    }
  }
 
  data += F("{\"type\":\"load\", ");

  data += F("\"cpu\":{\"values\":[");
  data += cpu_data;
  data += F("],\"loops\":");
  data += system_main_loops();
  data += F("},");

  data += F("\"mem\":{\"values\":[");
  data += mem_data;
  data += F("],\"free\":");
  data += system_mem_free();
  data += F("},");

  data += F("\"net\":{\"values\":[");
  data += net_data;
  data += F("],\"xfer\":");
  data += system_net_xfer();
  data += F("}}");

  packet_send(client);
#endif
}

static void send_module_data(int client) {
  packet_prepare(300, PSTR("MODULE"));

  data += F("{\"type\":\"module\",\"state\":[");
  for (int i=0; i<module_count(); i++) {
    int state;

    module_call_state(i, state);

    data += F("\"");
    data += module_state_str(state);
    data += F("\"");

    if (i != module_count() - 1) data += F(",");
  }
  data += F("]}");

  packet_send(client);
}

static void send_temp_data(int client) {
  float temp = rtc_temp();

  packet_prepare(100, PSTR("TEMP"));

  data += F("{\"type\":\"temp\",");

  data += F("\"value\":\"");
  data += float2str(temp);
  data += F("\"}");

  packet_send(client);
}

static void send_log_data(int client) {
  packet_prepare(4000, PSTR("LOG"));

  data += F("{\"type\":\"log\",\"text\":\"");
  logger_dump_html(data, -1);
  data += F("\"}");

  packet_send(client);
}

static void ws_event(uint8_t client, WStype_t type, uint8_t *data, size_t len) {
  if (!p) return;

  IPAddress ip = p->websocket->remoteIP(client);
  bool unhandled_client_request = true;

  if (type == WStype_CONNECTED) {
#ifdef LOG_CLIENT_CONNECTS
    log_print(F("WS:   client %i connected from %d.%d.%d.%d url: %s"),
      client, ip[0], ip[1], ip[2], ip[3], data
    );
#endif
    unhandled_client_request = false;
  } else if (type == WStype_DISCONNECTED) {
#ifdef LOG_CLIENT_CONNECTS
    log_print(F("WS:   client %i disconnected"), client);
#endif
    p->client_request[client] = CLIENT_REQUEST_NONE;
    unhandled_client_request = false;
  } else if (type == WStype_TEXT) {
    if (!strncmp_P((const char *)data, PSTR("reboot"), len)) {
      p->client_request[client] = CLIENT_REQUEST_REBOOT;
      unhandled_client_request = false;
    } else if (!strncmp_P((const char *)data, PSTR("state"), len)) {
      p->client_request[client] = CLIENT_REQUEST_STATE;
      unhandled_client_request = false;
    } else if (!strncmp_P((const char *)data, PSTR("temp"), len)) {
      p->client_request[client] = CLIENT_REQUEST_TEMP;
      unhandled_client_request = false;
    } else if (!strncmp_P((const char *)data, PSTR("time"), len)) {
      p->client_request[client] = CLIENT_REQUEST_TIME;
      unhandled_client_request = false;
    } else if (!strncmp_P((const char *)data, PSTR("load"), len)) {
      p->client_request[client] = CLIENT_REQUEST_LOAD;
      unhandled_client_request = false;
    } else if (!strncmp_P((const char *)data, PSTR("log"), len)) {
      p->client_request[client] = CLIENT_REQUEST_LOG;
      unhandled_client_request = false;
    } else if (!strncmp_P((const char *)data, PSTR("adc"), len)) {
      p->client_request[client] = CLIENT_REQUEST_ADC;
      unhandled_client_request = false;
    } else if ((len >= 3) && (!strncmp_P((const char *)data, PSTR("sync"), 4))) {
      String tm((const char *)&data[5]);     // strip 'sync ' from tm
      String ms((const char *)&data[len-3]); // get milliseconds
      struct timespec tv;

      tm[tm.length()-3] = '\0'; // strip milliseconds from tm

      tv.tv_sec  = tm.toInt();
      tv.tv_nsec = ms.toInt() * 1000000;

      clock_settime(CLOCK_REALTIME, &tv);
      if (rtc_set(&tv) != 0) {
        log_print(F("WS:   could not set RTC"));
      }

      unhandled_client_request = false;
    } else if ((len >= 6) && (!strncmp_P((const char *)data, PSTR("relais"), 6))) {
      char c = data[len - 1];

           if (c == '1') gpio_relais_on();
      else if (c == '0') gpio_relais_off();
      else if (c == '!') gpio_relais_toggle();
      else p->client_request[client] = CLIENT_REQUEST_RELAIS;

      unhandled_client_request = false;
    } else if ((len >= 6) && (!strncmp_P((const char *)data, PSTR("module"), 6))) {
      String act((const char *)&data[7]);  // strip 'module ' from data
      act = act.substring(0, 4);           // cut after 4th char
      String mod((const char *)&data[12]); // get module index

      p->module = mod.toInt();

      if (act == F("init")) p->client_request[client] = CLIENT_REQUEST_INIT;
      if (act == F("fini")) p->client_request[client] = CLIENT_REQUEST_FINI;

      unhandled_client_request = false;
    }
  }

  if (unhandled_client_request) {
    log_print(F("WS:   unhandled client request: %s"), data);
  }
}

void websocket_broadcast_message(const String &msg) {
  if (!p) return;

  packet_prepare(100, PSTR("BROADCAST"));

  data += F("{\"type\":\"broadcast\",\"value\":\"");
  data += msg;
  data += F("\"}");

  packet_broadcast();
}

int websocket_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool websocket_init(void) {
  if (p) return (false);

  config_init();

  if (bootup) {
    if (!config->websocket_enabled) {
      log_print(F("WS:   websockets disabled in config"));

      config_fini();

      return (false);
    }
  }

  config_fini();

  log_print(F("WS:   initializing websockets"));

  p = (WS_PrivateData *)malloc(sizeof (WS_PrivateData));
  memset(p, 0, sizeof (WS_PrivateData));

  for (int i=0; i<WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    p->client_request[i] = CLIENT_REQUEST_NONE;
  }

  p->websocket = new WebSocketsServer(81, "", F("genesys"));
  p->websocket->onEvent(ws_event);
  p->websocket->begin();

  return (true);
}

bool websocket_fini(void) {
  if (!p) return (false);

  log_print(F("WS:   closing websockets"));

  delete (p->websocket);

  // free private data
  free(p);
  p = NULL;

  return (true);
}

void websocket_poll(void) {
  bool ret;

  if (!p) return;

  p->websocket->loop();

  for (int i=0; i<WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    if (!p) return; // maybe module_call_fini() was called on us

    int &req = p->client_request[i];

         if (req == CLIENT_REQUEST_NONE)   continue;
    else if (req == CLIENT_REQUEST_REBOOT) system_reboot();
    else if (req == CLIENT_REQUEST_STATE)  send_module_data(i);
    else if (req == CLIENT_REQUEST_RELAIS) send_relais_data(i);
    else if (req == CLIENT_REQUEST_TEMP)   send_temp_data(i);
    else if (req == CLIENT_REQUEST_TIME)   send_time_data(i);
    else if (req == CLIENT_REQUEST_LOAD)   send_load_data(i);
    else if (req == CLIENT_REQUEST_LOG)    send_log_data(i);
    else if (req == CLIENT_REQUEST_ADC)    send_adc_data(i);
    else if (req == CLIENT_REQUEST_INIT)   module_call_init(p->module, ret);
    else if (req == CLIENT_REQUEST_FINI)   module_call_fini(p->module, ret);

    if (p) req = CLIENT_REQUEST_NONE;
  }
}

MODULE(websocket)
