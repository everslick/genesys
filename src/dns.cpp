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

#include <ESP8266WiFi.h>
//#include <DNSServer.h>

#include "config.h"
#include "log.h"

//static DNSServer *dns = NULL;

bool dns_init(void) {
  IPAddress ap_addr(config->ap_addr);

  //if (dns) return (false);

  // start DNS only if the ESP has no valid WiFi config
  // it will reply with it's own AP address on all DNS
  // requests (captive portal concept)

  if (config->wifi_ssid[0] == '\0') {
    //dns = new DNSServer();

    log_print(F("DNS:  starting captive portal DNS for AP\n"));

    //dns->start(53, "*", ap_addr);
  }

  return (true);
}

void dns_poll(void) {
  //if (dns) dns->processNextRequest();
}
