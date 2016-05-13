#include <ESP8266mDNS.h>

#include "system.h"
#include "config.h"
#include "log.h"

#include "mdns.h"

bool ICACHE_FLASH_ATTR mdns_init(void) {
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
