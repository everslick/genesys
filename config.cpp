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

#include <EEPROM.h>
#include <IPAddress.h>

#include "log.h"

#include "config.h"

#define EEPROM_SIZE 4096 // SPI_FLASH_SEC_SIZE

#define MAGIC_STR "GENESYS"
#define MAGIC_LEN 7

#define DATA_OFFSET 10

#define CONFIG_VERSION 1

struct Config *config = NULL;

static void ICACHE_FLASH_ATTR eeprom_write(const void *buf, int addr, int size) {
  char *data = (char *)buf;

  for (int i=0; i<size; i++) {
    EEPROM.write(addr + i, data[i]);
  }
}

static void ICACHE_FLASH_ATTR eeprom_read(void *buf, int addr, int size) {
  char *data = (char *)buf;

  for (int i=0; i<size; i++) {
    data[i] = EEPROM.read(addr + i);
  }
}

static void ICACHE_FLASH_ATTR eeprom_format(void) {
  for (int i=0; i<EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }

  eeprom_write(MAGIC_STR, 0, MAGIC_LEN);
}

static void ICACHE_FLASH_ATTR eeprom_commit(void) {
  EEPROM.commit();
}

void ICACHE_FLASH_ATTR config_reset(void) {
  IPAddress ip;

  log_print(F("CONF: formatting EEPROM\n"));

  EEPROM.begin(sizeof (Config));

  eeprom_format();

  config->version             = CONFIG_VERSION;

  // user
  strncpy(config->user_name   , DEFAULT_USER_NAME,   63);
  strncpy(config->user_pass   , DEFAULT_USER_PASS,   63);

  // WiFi
  strncpy(config->wifi_ssid   , DEFAULT_WIFI_SSID,   31);
  strncpy(config->wifi_pass   , DEFAULT_WIFI_PASS,   63);

  // IP
  config->ip_static           = DEFAULT_IP_STATIC;
  ip.fromString(DEFAULT_IP_ADDR);
  config->ip_addr             = ip;
  ip.fromString(DEFAULT_IP_NETMASK);
  config->ip_netmask          = ip;
  ip.fromString(DEFAULT_IP_GATEWAY);
  config->ip_gateway          = ip;
  ip.fromString(DEFAULT_IP_DNS1);
  config->ip_dns1             = ip;
  ip.fromString(DEFAULT_IP_DNS2);
  config->ip_dns2             = ip;

  // AP
  config->ap_enabled          = DEFAULT_AP_ENABLED;
  ip.fromString(DEFAULT_AP_ADDR);
  config->ap_addr             = ip;

  // mDNS
  config->mdns_enabled        = DEFAULT_MDNS_ENABLED;
  strncpy(config->mdns_name   , DEFAULT_MDNS_NAME,   63);

  // NTP
  config->ntp_enabled         = DEFAULT_NTP_ENABLED;
  strncpy(config->ntp_server  , DEFAULT_NTP_SERVER,  63);
  config->ntp_interval        = DEFAULT_NTP_INTERVAL;

  // MQTT
  config->mqtt_enabled        = DEFAULT_MQTT_ENABLED;
  strncpy(config->mqtt_server , DEFAULT_MQTT_SERVER, 63);
  strncpy(config->mqtt_user   , DEFAULT_MQTT_USER,   63);
  strncpy(config->mqtt_pass   , DEFAULT_MQTT_PASS,   63);
  config->mqtt_interval       = DEFAULT_MQTT_INTERVAL;

  eeprom_write(config, DATA_OFFSET, sizeof (Config));

  EEPROM.end();
}

void ICACHE_FLASH_ATTR config_init(void) {
  char magic[MAGIC_LEN + 1];

  log_print(F("CONF: reading config from EEPROM\n"));

  if (sizeof (Config) > EEPROM_SIZE) {
    log_print(F("CONF: EEPROM too small\n"));
  }

  if (!(config = (struct Config *)malloc(sizeof (Config)))) {
    log_print(F("CONF: could not allocate memory\n"));
  }

  EEPROM.begin(sizeof (Config));
  eeprom_read(magic, 0, MAGIC_LEN);
  magic[MAGIC_LEN] = '\0';
  EEPROM.end();

  if (strcmp(MAGIC_STR, magic)) {
    log_print(F("CONF: EEPROM not initialized\n"));
    config_reset();
  } else {
    EEPROM.begin(sizeof (Config));
    eeprom_read(config, DATA_OFFSET, sizeof (Config));
    EEPROM.end();

    if (config->version != CONFIG_VERSION) {
      log_print(F("CONF: firmware has new config version\n"));
      config_reset();
    }
  }
}

void ICACHE_FLASH_ATTR config_write(void) {
  log_print(F("CONF: writing config to EEPROM\n"));

  if (sizeof (Config) > EEPROM_SIZE) {
    log_print(F("CONF: EEPROM too small\n"));
  }

  EEPROM.begin(EEPROM_SIZE);
  eeprom_write(config, DATA_OFFSET, sizeof (Config));
  EEPROM.end();
}
