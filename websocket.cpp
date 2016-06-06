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

#include <ESPAsyncWebServer.h>

#include <limits.h>

#include "system.h"
#include "buffer.h"
#include "clock.h"
#include "log.h"

#include "websocket.h"

extern AsyncWebServer *http;

static AsyncWebSocket *websocket = NULL;

static void send_time_data(AsyncWebSocketClient *client) {
#ifndef RELEASE
  char time[16], uptime[24];
  Buffer data;

  system_uptime(uptime);
  system_time(time);

  data += "{\"type\":\"time\",";

  data += "\"uptime\":\"";
  data += uptime;
  data += "\",";

  data += "\"utc\":\"";
  data += time;
  data += "\"}";

  client->text((char *)data.data(), data.size());
  system_count_net_traffic(data.size());
#endif
}

static void send_adc_data(AsyncWebSocketClient *client) {
#ifndef RELEASE
  Buffer data;

  data += "{\"type\":\"adc\",\"value\":";
  data += String(analogRead(17));
  data += "}";

  client->text((char *)data.data(), data.size());
  system_count_net_traffic(data.size());
#endif
}

static void send_load_data(AsyncWebSocketClient *client) {
#ifndef RELEASE
  String cpu_data, mem_data, net_data;
  Buffer data(512);

  for (int i=0; i<system_load_history_entries(); i++) {
    SysLoad load = system_load_history(i);

    cpu_data += String(load.cpu);
    mem_data += String(load.mem);
    net_data += String(load.net);

    if (i != system_load_history_entries() - 1) {
      cpu_data += ','; mem_data += ','; net_data += ',';
    }
  }
 
  data += "{\"type\":\"load\", ";

  data += "\"cpu\":{\"values\":[";
  data += cpu_data;
  data += "]},";

  data += "\"mem\":{\"values\":[";
  data += mem_data;
  data += "]},";

  data += "\"net\":{\"values\":[";
  data += net_data;
  data += "]}}";

  client->text((char *)data.data(), data.size());
  system_count_net_traffic(data.size());
#endif
}

static void send_log_data(AsyncWebSocketClient *client) {
#ifndef RELEASE
  String log;

  log.reserve(3*1024);

  log += "{\"type\":\"log\",\"text\":\"";
  log_dump_html(log);
  log += "\"}";

  client->text(log);
  system_count_net_traffic(log.length());
#endif
}

static void ws_event(AsyncWebSocket *server,
                     AsyncWebSocketClient *client,
                     AwsEventType type,
                     void *arg,
                     uint8_t *data,
                     size_t len){

  //IPAddress ip = websocket->remoteIP(client);

  switch (type) {
    case WS_EVT_DISCONNECT:
      //log_print(F("WS:   client %i disconnected\n"), client->id());
    break;

    case WS_EVT_CONNECT:
      //log_print(F("WS:   client %i connected, url: %s\n"),
      //  client->id(), server->url()
      //);
    break;

    case WS_EVT_ERROR:
      log_print(F("WS:   [%s][%u] error(%u): %s\n"),
        server->url(), client->id(), *((uint16_t *)arg), (char *)data
      );
    break;

    case WS_EVT_PONG:
      //pong message was received (in response to a ping request maybe)
      log_print("WS:   [%s][%u] pong[%u]: %s\n",
        server->url(), client->id(), len, (len) ? (char *)data : ""
      );
    break;

    case WS_EVT_DATA:
      if (!strncmp((const char *)data, "time", len)) {
        send_time_data(client);
      }
      if (!strncmp((const char *)data, "load", len)) {
        send_load_data(client);
      }
      if (!strncmp((const char *)data, "log", len)) {
        send_log_data(client);
      }
      if (!strncmp((const char *)data, "adc", len)) {
        send_adc_data(client);
      }
    break;
  }
}

void websocket_broadcast_message(const char *msg) {
  Buffer data;

  data += "{\"type\":\"broadcast\",\"value\":\"";
  data += msg;
  data += "\"}";

  websocket->textAll((char *)data.data(), data.size());
  system_count_net_traffic(data.size());
}

bool websocket_init(void) {
  if (websocket) return (false);

  websocket = new AsyncWebSocket("/ws"); // access at ws://[esp ip]/ws
  websocket->onEvent(ws_event);
  http->addHandler(websocket);

  return (true);
}

void websocket_poll(void) {
}

void websocket_disconnect_clients(void) {
  if (websocket) {
    log_print(F("WS:   disconnected all clients\n"));

    // Disable client connections    
    websocket->enable(false);

    // Close them
    websocket->closeAll();
  }
}
