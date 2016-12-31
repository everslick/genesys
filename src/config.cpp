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

#include "system.h"
#include "module.h"
#include "xxtea.h"
#include "log.h"

#include "config.h"

#define CONFIG_EEPROM_SIZE 4096 // SPI_FLASH_SEC_SIZE

#define CONFIG_MAGIC "GENESYS"

#define CONFIG_VERSION 2

enum { STR, INT8, INT32, BOOL, IP, PASS };

struct Config *config = NULL;

static EEPROMClass *eeprom = NULL;

static uint8_t ref = 0;

static bool config_is_uninitialized = false;
static bool config_has_new_version  = false;

static void *ptr(const String &name, int &type, int &min, int &max) {
  if (!config) config_init();

  if (name == F("user_name"))          {
    type = STR;            max = 16;   return (&config->user_name);
  }
  if (name == F("user_pass"))          {
    type = PASS;  min = 0; max = 32;   return (&config->user_pass);
  }

  if (name == F("device_name"))        {
    type = STR;            max = 16;   return (&config->device_name);
  }

  if (name == F("wifi_enabled"))       {
    type = BOOL;                       return (&config->wifi_enabled);
  }
  if (name == F("wifi_ssid"))          {
    type = STR;            max = 32;   return (&config->wifi_ssid);
  }
  if (name == F("wifi_pass"))          {
    type = PASS;  min = 8; max = 32;   return (&config->wifi_pass);
  }
  if (name == F("wifi_power"))         {
    type = INT8;  min = 0; max = 21;   return (&config->wifi_power);
  }
  if (name == F("wifi_watchdog"))      {
    type = INT32; min = 0; max = 60;   return (&config->wifi_watchdog);
  }

  if (name == F("ip_static"))          {
    type = BOOL;                       return (&config->ip_static);
  }
  if (name == F("ip_addr"))            {
    type = IP;                         return (&config->ip_addr);
  }
  if (name == F("ip_netmask"))         {
    type = IP;                         return (&config->ip_netmask);
  }
  if (name == F("ip_gateway"))         {
    type = IP;                         return (&config->ip_gateway);
  }
  if (name == F("ip_dns1"))            {
    type = IP;                         return (&config->ip_dns1);
  }
  if (name == F("ip_dns2"))            {
    type = IP;                         return (&config->ip_dns2);
  }

  if (name == F("ap_enabled"))         {
    type = BOOL;                       return (&config->ap_enabled);
  }
  if (name == F("ap_addr"))            {
    type = IP;                         return (&config->ap_addr);
  }

  if (name == F("ntp_enabled"))        {
    type = BOOL;                       return (&config->ntp_enabled);
  }
  if (name == F("ntp_interval"))       {
    type = INT32; min = 1; max = 1440; return (&config->ntp_interval);
  }
  if (name == F("ntp_server"))         {
    type = STR;            max = 32;   return (&config->ntp_server);
  }

  if (name == F("telemetry_enabled"))  {
    type = BOOL;                       return (&config->telemetry_enabled);
  }
  if (name == F("telemetry_url"))      {
    type = STR;            max = 64;   return (&config->telemetry_url);
  }
  if (name == F("telemetry_user"))     {
    type = STR;            max = 16;   return (&config->telemetry_user);
  }
  if (name == F("telemetry_pass"))     {
    type = PASS;  min = 0; max = 32;   return (&config->telemetry_pass);
  }
  if (name == F("telemetry_interval")) {
    type = INT32; min = 1; max = 3600; return (&config->telemetry_interval);
  }

  if (name == F("update_enabled"))     {
    type = BOOL;                       return (&config->update_enabled);
  }
  if (name == F("update_url"))         {
    type = STR;            max = 64;   return (&config->update_url);
  }
  if (name == F("update_interval"))    {
    type = INT32; min = 1; max = 240;  return (&config->update_interval);
  }

  if (name == F("storage_enabled"))    {
    type = BOOL;                       return (&config->storage_enabled);
  }
  if (name == F("storage_interval"))   {
    type = INT32; min = 1; max = 60;   return (&config->storage_interval);
  }
  if (name == F("storage_mask"))       {
    type = INT32; min = 0; max = 2048; return (&config->storage_mask);
  }

  if (name == F("logger_enabled"))     {
    type = BOOL;                       return (&config->logger_enabled);
  }
  if (name == F("logger_channels"))    {
    type = INT8; min = 0; max = 7;     return (&config->logger_channels);
  }
  if (name == F("logger_host"))        {
    type = IP;                         return (&config->logger_host);
  }
  if (name == F("logger_port"))        {
    type = INT32;                      return (&config->logger_port);
  }

  if (name == F("cpu_turbo"))          {
    type = BOOL;                       return (&config->cpu_turbo);
  }

  if (name == F("mdns_enabled"))       {
    type = BOOL;                       return (&config->mdns_enabled);
  }
  if (name == F("webserver_enabled"))  {
    type = BOOL;                       return (&config->webserver_enabled);
  }
  if (name == F("websocket_enabled"))  {
    type = BOOL;                       return (&config->websocket_enabled);
  }
  if (name == F("telnet_enabled"))     {
    type = BOOL;                       return (&config->telnet_enabled);
  }
  if (name == F("gpio_enabled"))       {
    type = BOOL;                       return (&config->gpio_enabled);
  }
  if (name == F("rtc_enabled"))        {
    type = BOOL;                       return (&config->rtc_enabled);
  }
  if (name == F("ade_enabled"))        {
    type = BOOL;                       return (&config->ade_enabled);
  }

  return (NULL);
}

static void parse_line(const String &line) {
  int idx = line.indexOf('=');
  String key, val;

  if (idx >= 0) {
    key = line.substring(0, idx);
    val = line.substring(idx + 1);

    if (val == "") {
      config_clr(key);
    } else {
      config_set(key, val);
    }
  }
}

static void append_line(const String &key, String &str) {
  String val;

  if (config_get(key, val)) {
    str += key + F("=");
    str += val + F("\r\n");
  }
}

static bool write_str(char *conf, const String &value, int max) {
  if (value.length() > max) return (false);

  memset(conf, 0, max);
  strcpy(conf, value.c_str());

  return (true);
}

static bool write_pass(char *conf, const String &value, int min, int max) {
  char buf[max];

  if (value.length() == 0)   return (true);
  if (value.length() > max)  return (false);
  if (value.length() < min)  return (false);

  memset(buf, 0, max);
  memcpy(buf, value.c_str(), value.length());

  if (!xxtea_encrypt(buf, max)) return (false);

  memcpy(conf, buf, max);
  conf[max] = '\0';

  return (true);
}

static bool write_ip(uint32_t *conf, const String &value) {
  IPAddress ip;

  if (value.length() > 0) {
    if (!ip.fromString(value)) return (false);
  }

  *conf = ip;

  return (true);
}

static bool write_int32(uint32_t *conf, const String &value, int min, int max) {
  int val = value.toInt();

  if ((val < min) || (val > max)) {
    return (false);
  }

  *conf = val;

  return (true);
}

static bool write_int8(uint8_t *conf, const String &value, int min, int max) {
  int val = value.toInt();

  if ((val < min) || (val > max)) {
    return (false);
  }

  *conf = val;

  return (true);
}

static bool write_bool(uint8_t *conf, const String &value) {
  int val = value.toInt();

  if ((val != 0) && (val != 1)) {
    return (false);
  }

  *conf = val;

  return (true);
}

static bool read_str(char *conf, String &value) {
  value = conf;

  return (true);
}

static bool read_pass(char *conf, String &value, int max) {
  char buf[max];

  memcpy(buf, conf, max);
  buf[max] = '\0';

  if (!xxtea_decrypt(buf, max)) return (false);

  value = buf;

  return (true);
}

static bool read_ip(uint32_t *conf, String &value) {
  IPAddress ip(*conf);

  value = ip.toString();

  return (true);
}

static bool read_int32(uint32_t *conf, String &value) {
  value = *conf;

  return (true);
}

static bool read_int8(uint8_t *conf, String &value) {
  value = *conf;

  return (true);
}

static bool read_bool(uint8_t *conf, String &value) {
  value = *conf;

  return (true);
}

bool config_set(const String &name, const String &value) {
  int type, min, max;
  void *p;

  if ((p = ptr(name, type, min, max))) {
    if (type == STR)   return (write_str(      (char *)p, value,      max));
    if (type == PASS)  return (write_pass(     (char *)p, value, min, max));
    if (type == INT8)  return (write_int8(  (uint8_t *)p, value, min, max));
    if (type == INT32) return (write_int32((uint32_t *)p, value, min, max));
    if (type == BOOL)  return (write_bool(  (uint8_t *)p, value          ));
    if (type == IP)    return (write_ip(   (uint32_t *)p, value          ));
  }

  return (false);
}

bool config_get(const String &name, String &value) {
  int type, min, max;
  void *p;

  if ((p = ptr(name, type, min, max))) {
    if (type == STR)   return (read_str(      (char *)p, value));
    if (type == PASS)  return (read_pass(     (char *)p, value, max));
    if (type == INT8)  return (read_int8(  (uint8_t *)p, value));
    if (type == INT32) return (read_int32((uint32_t *)p, value));
    if (type == BOOL)  return (read_bool(  (uint8_t *)p, value));
    if (type == IP)    return (read_ip(   (uint32_t *)p, value));
  }

  return (false);
}

bool config_clr(const String &name) {
  int type, min, max;
  void *p;

  if ((p = ptr(name, type, min, max))) {
    if (type == PASS) {
      memset(p, 0, max);
      // FIXME clearing a password crashes here!
      //xxtea_encrypt((char *)p, max);
      return (true);
    } else if (type == STR) {
      memset(p, 0, max);
      return (true);
    } else if ((type == INT32) || (type == IP)) {
      *(uint32_t *)p = 0;
      return (true);
    } else if (type == INT8) {
      *(uint8_t *)p = 0;
      return (true);
    } else if (type == BOOL) {
      *(bool *)p = false;
      return (true);
    }
  }

  return (false);
}

void config_import(String &str) {
  String line;

  for (char c : str) {
    if (c == '\r') parse_line(line); else if (c != '\n') line += c;
  }
}

void config_export(String &str) {
  append_line(F("user_name"),          str);
  append_line(F("user_pass"),          str);
  append_line(F("device_name"),        str);
  append_line(F("wifi_enabled"),       str);
  append_line(F("wifi_ssid"),          str);
  append_line(F("wifi_pass"),          str);
  append_line(F("wifi_power"),         str);
  append_line(F("wifi_watchdog"),      str);
  append_line(F("ip_static"),          str);
  append_line(F("ip_addr"),            str);
  append_line(F("ip_netmask"),         str);
  append_line(F("ip_gateway"),         str);
  append_line(F("ip_dns1"),            str);
  append_line(F("ip_dns2"),            str);
  append_line(F("ap_enabled"),         str);
  append_line(F("ap_addr"),            str);
  append_line(F("ntp_enabled"),        str);
  append_line(F("ntp_interval"),       str);
  append_line(F("ntp_server"),         str);
  append_line(F("telemetry_enabled"),  str);
  append_line(F("telemetry_url"),      str);
  append_line(F("telemetry_user"),     str);
  append_line(F("telemetry_pass"),     str);
  append_line(F("telemetry_interval"), str);
  append_line(F("update_enabled"),     str);
  append_line(F("update_url"),         str);
  append_line(F("update_interval"),    str);
  append_line(F("storage_enabled"),    str);
  append_line(F("storage_interval"),   str);
  append_line(F("storage_mask"),       str);
  append_line(F("logger_enabled"),     str);
  append_line(F("logger_channels"),    str);
  append_line(F("logger_host"),        str);
  append_line(F("logger_port"),        str);
  append_line(F("mdns_enabled"),       str);
  append_line(F("webserver_enabled"),  str);
  append_line(F("websocket_enabled"),  str);
  append_line(F("telnet_enabled"),     str);
  append_line(F("gpio_enabled"),       str);
  append_line(F("rtc_enabled"),        str);
  append_line(F("ade_enabled"),        str);
  append_line(F("cpu_turbo"),          str);
}

void config_reset(void) {
  bool unload_eeprom = false;
  uint8_t ct_a, ct_b, ct_c;
  IPAddress ip;

  if (!config) {
    config_init();
    unload_eeprom = true;
  }

  // clear EEPROM
  for (int i=0; i<CONFIG_EEPROM_SIZE; i++) {
    eeprom->write(i, 0);
  }

  // set magic string and config version number
  write_str(config->magic           , F(CONFIG_MAGIC),           8);
  config->version                   = CONFIG_VERSION;

  // user
  write_str(config->user_name       , F(DEFAULT_USER_NAME),     16);
  write_pass(config->user_pass      , F(DEFAULT_USER_PASS),  0, 32);

  // alias
  write_str(config->device_name     , F(DEFAULT_DEVICE_NAME),   16);

  // WiFi
  config->wifi_enabled              = DEFAULT_WIFI_ENABLED;
#ifdef ALPHA
  write_str(config->wifi_ssid       , F(DEFAULT_WIFI_SSID),     32);
  write_pass(config->wifi_pass      , F(DEFAULT_WIFI_PASS),  8, 32);
#endif
  config->wifi_power                = 21;

  // IP
  config->ip_static                 = DEFAULT_IP_STATIC;
  write_ip(&config->ip_addr         , DEFAULT_IP_ADDR);
  write_ip(&config->ip_netmask      , DEFAULT_IP_NETMASK);
  write_ip(&config->ip_gateway      , DEFAULT_IP_GATEWAY);
  write_ip(&config->ip_dns1         , DEFAULT_IP_DNS1);
  write_ip(&config->ip_dns2         , DEFAULT_IP_DNS2);

  // AP
  config->ap_enabled                = DEFAULT_AP_ENABLED;
  write_ip(&config->ap_addr         , DEFAULT_AP_ADDR);

  // NTP
  config->ntp_enabled               = DEFAULT_NTP_ENABLED;
  write_str(config->ntp_server      , F(DEFAULT_NTP_SERVER),     32);
  config->ntp_interval              = DEFAULT_NTP_INTERVAL;

  // TELEMETRY
  config->telemetry_enabled         = DEFAULT_TELEMETRY_ENABLED;
  write_str(config->telemetry_url   , F(DEFAULT_TELEMETRY_URL),     64);
  write_str(config->telemetry_user  , F(DEFAULT_TELEMETRY_USER),    16);
  write_pass(config->telemetry_pass , F(DEFAULT_TELEMETRY_PASS), 0, 32);
  config->telemetry_interval        = DEFAULT_TELEMETRY_INTERVAL;

  // update
  config->update_enabled            = DEFAULT_UPDATE_ENABLED;
  write_str(config->update_url      , F(DEFAULT_UPDATE_URL),     64);
  config->update_interval           = DEFAULT_UPDATE_INTERVAL;

  // local file storage
  config->storage_enabled           = DEFAULT_STORAGE_ENABLED;
  config->storage_mask              = DEFAULT_STORAGE_MASK;
  config->storage_interval          = DEFAULT_STORAGE_INTERVAL;

  // debug logging
#ifdef ALPHA
  config->logger_enabled            = DEFAULT_LOGGER_ENABLED;
#endif
  config->logger_channels           = DEFAULT_LOGGER_CHANNELS;
  write_ip(&config->logger_host     , DEFAULT_LOGGER_HOST);
  config->logger_port               = DEFAULT_LOGGER_PORT;

  // general switches
  config->mdns_enabled              = DEFAULT_MDNS_ENABLED;
  config->webserver_enabled         = DEFAULT_WEBSERVER_ENABLED;
  config->websocket_enabled         = DEFAULT_WEBSOCKET_ENABLED;
  config->telnet_enabled            = DEFAULT_TELNET_ENABLED;
  config->gpio_enabled              = DEFAULT_GPIO_ENABLED;
  config->rtc_enabled               = DEFAULT_RTC_ENABLED;

  // CPU speed
  config->cpu_turbo                 = DEFAULT_CPU_TURBO;

  // store in flash
  if (!eeprom->commit()) {
    log_print(F("CONF: EEPROM write error"));
  }

  if (unload_eeprom) {
    config_fini();
  }
}

int config_state(void) {
  if (config) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

void config_poll(void) {
  static bool already_polled = false;

  if (!already_polled) {
    already_polled = true;

    if (sizeof (Config) > CONFIG_EEPROM_SIZE) {
      log_print(F("CONF: EEPROM too small"));
    }
    if (config_is_uninitialized) {
      log_print(F("CONF: EEPROM has been formatted"));
    }
    if (config_has_new_version) {
      log_print(F("CONF: firmware has new config version"));
    }
  }
}

bool config_init(void) {
  ref++;

  if (ref > 1) return (true);

  eeprom = new EEPROMClass();
  eeprom->begin(sizeof (Config));
  config = (Config *)eeprom->getDataPtr();

  if (String(F(CONFIG_MAGIC)) != config->magic) {
    config_is_uninitialized = true;
    config_reset();
  } else {
    if (config->version != CONFIG_VERSION) {
      config_has_new_version = true;
      config_reset();
    }
  }

  return (true);
}

bool config_fini(void) {
  ref--;

  if (ref < 0) return (false);
  if (ref > 0) return (true);

  eeprom->end();
  delete (eeprom);
  eeprom = NULL;

  config = NULL;

  return (true);
}

void config_write(void) {
  int len = sizeof (Config);

  if (eeprom) {
    log_print(F("CONF: writing (%i bytes) to EEPROM"), len);

    if (len > CONFIG_EEPROM_SIZE) {
      log_print(F("CONF: EEPROM too small"));
    }

    eeprom->commit();
  } else {
    log_print(F("CONF: EEPROM not mounted"));
  }
}

MODULE(config)
