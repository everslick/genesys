#include <DNSServer.h>

#include "config.h"
#include "log.h"

static DNSServer *dns = NULL;

bool dns_init(void) {
  IPAddress ap_addr(config->ap_addr);

  if (dns) return (false);

  // start DNS only if the ESP has no valid WiFi config
  // it will reply with it's own AP address on all DNS
  // requests (captive portal concept)

  if (config->wifi_ssid[0] == '\0') {
    dns = new DNSServer();

    log_print(F("DNS:  starting captive portal DNS for AP\n"));

    dns->start(53, "*", ap_addr);
  }

  return (true);
}

void dns_poll(void) {
  if (dns) dns->processNextRequest();
}
