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
#include "config.h"
#include "log.h"

#include "mdns.h"

bool mdns_init(void) {
  const char *name = config->mdns_name;

  if (!config->mdns_enabled) {
    log_print(F("MDNS: responder disabled in config\n"));

    return (false);
  }

  if (!strlen(name)) name = client_id;

  if (MDNS.begin(name)) {
    log_print(F("MDNS: hostname set to '%s.local'\n"), name);

    MDNS.addService("http", "tcp", 80);
  } else {
    log_print(F("MDNS: could not start mDNS responder\n"));
  }

  return (true);
}
