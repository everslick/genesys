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

#include <ESP8266mDNS.h>

#include "system.h"
#include "module.h"
#include "config.h"
#include "net.h"
#include "log.h"

#include "mdns.h"

static MDNSResponder *mdns = NULL;

static bool active = false;

static void mdns_start(void) {
  if (!mdns) return;

  if (mdns->begin(system_device_name().c_str())) {
    log_print(F("MDNS: hostname set to '%s.local'\r\n"),
      system_device_name().c_str()
    );

    mdns->addService(F("http"), F("tcp"), 80);

    active = true;
  } else {
    log_print(F("MDNS: could not start mDNS responder\r\n"));

    delete (mdns);
    mdns = NULL;
  }
}

static void mdns_stop(void) {
  log_print(F("MDNS: stopping mDNS responder\r\n"));

  active = false;
}

int mdns_state(void) {
  if (mdns) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool mdns_init(void) {
  if (mdns) return (false);

  if (bootup) {
    config_init();

    if (!config->mdns_enabled) {
      log_print(F("MDNS: responder disabled in config\r\n"));

      config_fini();

      return (false);
    }

    config_fini();
  }

  log_print(F("MDNS: initializing responder\r\n"));

  mdns = new MDNSResponder();

  return (true);
}

bool mdns_fini(void) {
  if (!mdns) return (false);

  log_print(F("MDNS: disabling mDNS responder\r\n"));

  delete (mdns);
  mdns = NULL;

  active = false;

  return (true);
}

void mdns_poll(void) {
  if (mdns) {
    if (net_connected() && !active) mdns_start();
    if (!net_connected() && active) mdns_stop();
  }
}

MODULE(mdns)
