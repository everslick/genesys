#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiUdp.h>

#include "websocket.h"
#include "system.h"
#include "config.h"
#include "net.h"
#include "log.h"

#include "update.h"

static uint32_t file_size = 0;

static void handle_update_start_cb(void) {
  uint32_t free_space = (system_free_sketch_space() - 0x1000) & 0xFFFFF000;

  log_print(F("OTA:  starting update ...\n"));
  log_print(F("OTA:  available space: %u bytes\n"), free_space);

  websocket_broadcast_message("update");

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
    case OTA_AUTH_ERROR:    str = "auth failed";    break;
    case OTA_BEGIN_ERROR:   str = "begin failed";   break;
    case OTA_CONNECT_ERROR: str = "connect failed"; break;
    case OTA_RECEIVE_ERROR: str = "receive failed"; break;
    case OTA_END_ERROR:     str = "end failed";     break;
  }

  log_print(F("OTA:  error[%u]: %s\n"), error, str);
}
 
static int check_for_update(void) {
  HTTPUpdateResult result;

  result = ESPhttpUpdate.update(config->update_url, FIRMWARE);

  switch (result) {
    case HTTP_UPDATE_FAILED:
      log_print("UPD:  error (%i): %s\n",
        ESPhttpUpdate.getLastError(),
        ESPhttpUpdate.getLastErrorString().c_str()
      );

      // uncomment if failed updates should be retried next minute
      // return (-1);
    break;

    case HTTP_UPDATE_NO_UPDATES:
      log_print("UPD:  no update available\n");
    break;

    case HTTP_UPDATE_OK:
      log_print("UPD:  update successful\n");
    break;
  }

  return (0);
}

bool update_init(void) {
  ArduinoOTA.onStart(handle_update_start_cb);
  ArduinoOTA.onEnd(handle_update_end_cb);
  ArduinoOTA.onProgress(handle_update_progress_cb);
  ArduinoOTA.onError(handle_update_error_cb);

  ArduinoOTA.begin();
}

void update_poll(void) {
  uint32_t interval = config->update_interval * 1000 * 60 * 60;
  static bool poll_pending = true;
  static uint32_t ms = 0;

  if (config->update_enabled && net_connected()) {
    if (poll_pending) interval = 60 * 1000;

    if ((millis() - ms) > interval) {
      poll_pending = (check_for_update() < 0);

      ms = millis();
    }
  }

  ArduinoOTA.handle();
}
