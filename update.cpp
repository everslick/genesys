#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "websocket.h"
#include "system.h"
#include "log.h"

#include "update.h"

static uint32_t file_size = 0;

static void handle_update_start_cb(void) {
  uint32_t free_space = (system_free_sketch_space() - 0x1000) & 0xFFFFF000;

  log_print(F("OTA:  starting OTA update ...\n"));
  log_print(F("OTA:  available space: %u bytes\n"), free_space);
  //log_print(F("OTA:  filename: %s\n"), filename.c_str());

  websocket_broadcast_message("update");
  system_delay(250);
  websocket_disconnect_clients();

  log_poll();
}

static void handle_update_end_cb(void) {
  log_print(F("OTA:  update successful: %u bytes\n"), file_size);

  system_reboot();
}

static void handle_update_progress_cb(unsigned int progress, unsigned int total) {
  file_size = total;

  int perc = progress / (total / 100);
  log_progress(F("OTA:  received "), "%", perc);

  system_yield();
}

static void handle_update_error_cb(ota_error_t error) {
  const char *str = "UNKNOWN";

  switch (error) {
    case OTA_AUTH_ERROR:    str = "Auth Failed";    break;
    case OTA_BEGIN_ERROR:   str = "Begin Failed";   break;
    case OTA_CONNECT_ERROR: str = "Connect Failed"; break;
    case OTA_RECEIVE_ERROR: str = "Receive Failed"; break;
    case OTA_END_ERROR:     str = "End Failed";     break;
  }

  log_print("OTA:  Error[%u]: %s\n", error, str);
}
 
bool update_init(void) {
  ArduinoOTA.onStart(handle_update_start_cb);
  ArduinoOTA.onEnd(handle_update_end_cb);
  ArduinoOTA.onProgress(handle_update_progress_cb);
  ArduinoOTA.onError(handle_update_error_cb);

  ArduinoOTA.begin();
}

void update_poll(void) {
  ArduinoOTA.handle();
}
