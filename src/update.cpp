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
#include <ESP8266httpUpdate.h>

#include "system.h"
#include "module.h"
#include "config.h"
#include "net.h"
#include "log.h"

#include "update.h"

struct UPD_PrivateData {
  ESP8266HTTPUpdate *upd;

  // settings from config
  uint32_t update_interval;
  char     update_url[64];
};

static UPD_PrivateData *p = NULL;

static int check_for_update(void) {
  HTTPUpdateResult result;

  result = p->upd->update(p->update_url, FIRMWARE);

  if (result == HTTP_UPDATE_FAILED) {
      log_print(F("UPD:  %s\r\n"),
        p->upd->getLastErrorString().c_str()
      );

      // uncomment if failed updates should be retried next minute
      // return (-1);
  } else if (result == HTTP_UPDATE_NO_UPDATES) {
    log_print(F("UPD:  no update available\r\n"));
  } else if (result == HTTP_UPDATE_OK) {
    log_print(F("UPD:  update successful\r\n"));
  }

  return (0);
}

int update_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool update_init(void) {
  if (p) return (false);

  config_init();

  if (bootup && !config->update_enabled) {
    log_print(F("UPD:  http update disabled in config\r\n"));

    config_fini();

    return (false);
  }

  log_print(F("UPD:  initializing http update\r\n"));

  p = (UPD_PrivateData *)malloc(sizeof (UPD_PrivateData));
  memset(p, 0, sizeof (UPD_PrivateData));

  p->update_interval = config->update_interval;
  strcpy(p->update_url, config->update_url);

  p->upd = new ESP8266HTTPUpdate();

  config_fini();

  return (true);
}

bool update_fini(void) {
  if (!p) return (false);

  log_print(F("UPD:  disabling http update\r\n"));

  delete (p->upd);

  // free private p->data
  free(p);
  p = NULL;

  return (true);
}

void update_poll(void) {
  if (p && net_connected()) {
    uint32_t interval = p->update_interval * 1000 * 60 * 60;
    static bool poll_pending = true;
    static uint32_t ms = millis();

    if (poll_pending) interval = 60 * 1000;

    if ((millis() - ms) > interval) {
      ms = millis();

      poll_pending = (check_for_update() < 0);
    }
  }
}

MODULE(update)
