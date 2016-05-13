#include <WebSocketsServer.h>

#include <limits.h>

#include "system.h"
#include "buffer.h"
#include "clock.h"
#include "log.h"

#include "websocket.h"

// install arduinoWebsockets into Arduino/libraries:
// git clone git@github.com:Links2004/arduinoWebSockets.git

enum {
  CLIENT_REQUEST_NONE,
  CLIENT_REQUEST_TIME,
  CLIENT_REQUEST_LOAD,
  CLIENT_REQUEST_LOG,
  CLIENT_REQUEST_ADC
};

static int client_request[WEBSOCKETS_SERVER_CLIENT_MAX];

static WebSocketsServer *websocket = NULL;

static void ICACHE_FLASH_ATTR send_time_data(int client) {
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

  websocket->sendTXT(client, (char *)data.data(), data.size());
  system_count_net_traffic(data.size());

  system_yield();
#endif
}

static void ICACHE_FLASH_ATTR send_adc_data(int client) {
#ifndef RELEASE
  Buffer data;

  data += "{\"type\":\"adc\",\"value\":";
  data += String(analogRead(17));
  data += "}";

  websocket->sendTXT(client, (char *)data.data(), data.size());
  system_count_net_traffic(data.size());

  system_yield();
#endif
}

static void ICACHE_FLASH_ATTR send_load_data(int client) {
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
 
  data += "              "; // 14 bytes reserved for header
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

  int len = data.size() - WEBSOCKETS_MAX_HEADER_SIZE;
  websocket->sendTXT(client, (char *)data.data(), len, true);
  system_count_net_traffic(data.size());

  system_yield();
#endif
}

static void ICACHE_FLASH_ATTR send_log_data(int client) {
#ifndef RELEASE
  String log;

  log.reserve(3*1024);

  log += "              "; // 14 bytes reserved for header
  log += "{\"type\":\"log\",\"text\":\"";
  log_dump_html(log);
  log += "\"}";

  int len = log.length() - WEBSOCKETS_MAX_HEADER_SIZE;
  websocket->sendTXT(client, (char *)log.c_str(), len, true);
  system_count_net_traffic(log.length());

  system_yield();
#endif
}

static void ICACHE_FLASH_ATTR ws_event(uint8_t client, WStype_t type, uint8_t *data, size_t len) {
  IPAddress ip = websocket->remoteIP(client);

  switch (type) {
    case WStype_DISCONNECTED:
      log_print(F("WS:   client %i disconnected\n"), client);
      client_request[client] = CLIENT_REQUEST_NONE;
    break;

    case WStype_CONNECTED:
      log_print(F("WS:   client %i connected from %d.%d.%d.%d url: %s\n"),
        client, ip[0], ip[1], ip[2], ip[3], data
      );
    break;

    case WStype_TEXT:
      if (!strcmp((const char *)data, "time")) {
        client_request[client] = CLIENT_REQUEST_TIME;
      }

      if (!strcmp((const char *)data, "load")) {
        client_request[client] = CLIENT_REQUEST_LOAD;
      }

      if (!strcmp((const char *)data, "log")) {
        client_request[client] = CLIENT_REQUEST_LOG;
      }

      if (!strcmp((const char *)data, "adc")) {
        client_request[client] = CLIENT_REQUEST_ADC;
      }
    break;
  }
}

void ICACHE_FLASH_ATTR websocket_broadcast_message(const char *msg) {
  Buffer data;

  data += "{\"type\":\"broadcast\",\"value\":\"";
  data += msg;
  data += "\"}";

  websocket->broadcastTXT((char *)data.data(), data.size());
  system_count_net_traffic(data.size());

  system_yield();
}

bool ICACHE_FLASH_ATTR websocket_init(void) {
  if (websocket) return (false);

  for (int i=0; i<WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    client_request[i] = CLIENT_REQUEST_NONE;
  }

  websocket = new WebSocketsServer(81, "", "genesys");
  websocket->onEvent(ws_event);
  websocket->begin();

  return (true);
}

void ICACHE_FLASH_ATTR websocket_poll(void) {
  if (websocket) {
    websocket->loop();

    for (int i=0; i<WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
      switch (client_request[i]) {
        case CLIENT_REQUEST_TIME:  send_time_data(i);  break;
        case CLIENT_REQUEST_LOAD:  send_load_data(i);  break;
        case CLIENT_REQUEST_LOG:   send_log_data(i);   break;
        case CLIENT_REQUEST_ADC:   send_adc_data(i);   break;
      }

      client_request[i] = CLIENT_REQUEST_NONE;
    }
  }
}

void ICACHE_FLASH_ATTR websocket_disconnect_clients(void) {
  if (websocket) {
    websocket->disconnect();
    log_print(F("WS:   disconnected all clients\n"));
  }
}
