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
#include <ESP8266WebServer.h>
#include <StreamString.h>
#include <limits.h>

#include "websocket.h"
#include "config.h"
#include "system.h"
#include "clock.h"
#include "html.h"
#include "log.h"
#include "net.h"

#include "http.h"

static ESP8266WebServer *http = NULL;

static String session_key;

static void create_session_key(void) {
  static const char *charset = "abcdefghijklmnopqrstuvwxyz"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "0123456789";
  const int length = 16;
  char buf[length];

  for (int n=0; n<length; n++) {
    buf[n] = charset[RANDOM_REG32 % 62];
  }

  buf[length] = '\0';

  log_print(F("HTTP: created session key: %s\n"), buf);

  session_key = buf;
}

static bool setup_complete(void) {
  String header;

  if (strlen(config->user_name) && strlen(config->user_pass)) {
    return (true);
  }

  header += F("HTTP/1.1 301 OK\r\n");
  header += F("Location: /setup\r\n");
  header += F("Cache-Control: no-cache\r\n\r\n");

  http->sendContent(header);
  system_count_net_traffic(header.length());

  return (false);
}

static bool authenticated(void) {
  String header;

  if (http->hasHeader("Cookie")){
    String cookie = http->header("Cookie");

    if (cookie.indexOf("GENESYS_SESSION_KEY=" + session_key) != -1) {
      return (true);
    }
  }

  header += F("HTTP/1.1 301 OK\r\n");
  header += F("Location: /login\r\n");
  header += F("Cache-Control: no-cache\r\n\r\n");

  http->sendContent(header);
  system_count_net_traffic(header.length());

  return (false);
}

static void send_header(const String &session, const String &redirect) {
  String header;

  header += F("HTTP/1.1 301 OK\r\n");
  header += F("Set-Cookie: GENESYS_SESSION_KEY=");
  header += session;
  header += F("\r\n");
  header += F("Location: ");
  header += redirect;
  header += F("\r\n");
  header += F("Cache-Control: no-cache\r\n\r\n");

  http->sendContent(header);
  system_count_net_traffic(header.length());
}

static bool store_str(char *conf, String value, int len) {
  if (value.length() >= len) return (false);

  if (value.length() > 0) {
    strcpy(conf, value.c_str());
    conf[value.length()] = '\0';
  }

  return (true);
}

static bool store_ip(uint32_t &conf, String value) {
  if (value.length() > 16) return (false);

  if (value.length() > 0) {
    IPAddress ip;

    ip.fromString(value);
    conf = ip;
  }

  return (true);
}

static bool store_int(uint32_t &conf, String value, int min, int max) {
  int val = value.toInt();

  if ((val < min) || (val > max)) {
    return (false);
  }

  conf = val;

  return (true);
}

static bool store_bool(uint8_t &conf, String value) {
  int val = value.toInt();

  if ((val != 0) && (val != 1)) {
    return (false);
  }

  conf = val;

  return (true);
}

static bool store_config(String name, String value) {
  if (name == "user_name")
	 return (store_str(config->user_name, value, 64));
  if (name == "user_pass")
	 return (store_str(config->user_pass, value, 64));

  if (name == "wifi_ssid_sel")
	 return (store_str(config->wifi_ssid, value, 32));
  if (name == "wifi_ssid")
	 return (store_str(config->wifi_ssid, value, 32));
  if (name == "wifi_pass")
	 return (store_str(config->wifi_pass, value, 64));

  if (name == "ip_static")
    return (store_bool(config->ip_static, value));
  if (name == "ip_addr")
    return (store_ip(config->ip_addr, value));
  if (name == "ip_netmask")
    return (store_ip(config->ip_netmask, value));
  if (name == "ip_gateway")
    return (store_ip(config->ip_gateway, value));
  if (name == "ip_dns1")
    return (store_ip(config->ip_dns1, value));
  if (name == "ip_dns2")
    return (store_ip(config->ip_dns2, value));

  if (name == "ap_enabled")
    return (store_bool(config->ap_enabled, value));
  if (name == "ap_addr")
    return (store_ip(config->ap_addr, value));

  if (name == "mdns_enabled")
    return (store_bool(config->mdns_enabled, value));
  if (name == "mdns_name")
	 return (store_str(config->mdns_name, value, 64));

  if (name == "ntp_enabled")
    return (store_bool(config->ntp_enabled, value));
  if (name == "ntp_interval")
    return (store_int(config->ntp_interval, value, 1, 1440));
  if (name == "ntp_server")
	 return (store_str(config->ntp_server, value, 64));

  if (name == "mqtt_enabled")
    return (store_bool(config->mqtt_enabled, value));
  if (name == "mqtt_server")
	 return (store_str(config->mqtt_server, value, 64));
  if (name == "mqtt_user")
	 return (store_str(config->mqtt_user, value, 64));
  if (name == "mqtt_pass")
	 return (store_str(config->mqtt_pass, value, 64));
  if (name == "mqtt_interval")
    return (store_int(config->mqtt_interval, value, 1, 3600));

  return (false);
}

static void handle_update_start_cb(void) {
  String webpage;

  if (!webpage.reserve(512)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  html_insert_page_header(webpage);
  html_insert_page_redirect(webpage, 30);
  html_insert_page_body(webpage);

  webpage += F("<br />Updating ...\n");

  html_insert_page_footer(webpage);

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());
}

static void handle_update_finished_cb(void) {
  String response;

  response += F("Update ");
  response += (Update.hasError()) ? "FAILED" : "OK";
  response += F("!\nRebooting ...\n\n");

  http->send(200, "text/plain", response);
  system_count_net_traffic(response.length());

  system_reboot();
}

static void handle_update_progress_cb(void) {
  uint32_t free_space = (system_free_sketch_space() - 0x1000) & 0xFFFFF000;
  static int received = 0, last_perc = -1;
  HTTPUpload &upload = http->upload();
  StreamString out;

  if (upload.status == UPLOAD_FILE_START){
    log_print(F("HTTP: starting OTA update ...\n"));
    log_print(F("HTTP: available space: %u bytes\n"), free_space);
    log_print(F("HTTP: filename: %s\n"), upload.filename.c_str());

    websocket_broadcast_message("update");
    system_delay(250);
    websocket_disconnect_clients();

    log_poll();

    if (!Update.begin(free_space)) {
      Update.printError(out);
      log_print(out.readString().c_str());
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(out);
      log_print(out.readString().c_str());
    } else {
      received += upload.currentSize;

      int perc = 100 * received / system_sketch_size();

      if (perc != last_perc) {
        log_progress(F("HTTP: received "), "%", perc);
        last_perc = perc;
      }

      system_count_net_traffic(upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true to set the size to the current progress
      log_print(F("HTTP: update successful: %u bytes\n"), upload.totalSize);
    } else {
      Update.printError(out);
      log_print(out.readString().c_str());
    }
  }

  system_yield();
}

static void handle_info_cb(void) {
#ifndef RELEASE
  String webpage;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  if (!webpage.reserve(4*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  html_insert_page_header(webpage);
  html_insert_page_body(webpage);
  html_insert_page_menu(webpage);
  html_insert_info_content(webpage);
  html_insert_page_footer(webpage);

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());
#endif
}

static void handle_reboot_cb(void) {
  String webpage;

  if (!authenticated()) return;

  if (!webpage.reserve(512)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  html_insert_page_header(webpage);
  html_insert_page_redirect(webpage, 10);
  html_insert_page_body(webpage);

  webpage += F("<br />Rebooting ...\n");

  html_insert_page_footer(webpage);

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());

  system_reboot();
}

static void handle_sys_cb(void) {
#ifndef RELEASE
  String webpage;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  if (!webpage.reserve(4*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  html_insert_page_header(webpage);
  html_insert_page_body(webpage);
  html_insert_page_menu(webpage);
  html_insert_sys_content(webpage);
  html_insert_page_footer(webpage);

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());
#endif
}

static bool validate_config(String &html) {
  bool config_ok = true;

  for (int i=0; i<http->args(); i++) {
    String name = http->argName(i);
    String arg = http->arg(i);

    if (!store_config(name, arg)) {
      html += name + " has an invalid value!<br />\n";
      config_ok = false;
    }
  }

  if (config_ok) {
    html += F("<br />Config saved.\n");
    html += F("<br />Rebooting ...\n");
  } else {
    html += F("<br />Config NOT saved.\n");
  }

  return (config_ok);
}

static void handle_conf_cb(void) {
  bool store_new_config = false;
  String webpage;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  if (!webpage.reserve(7*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  html_insert_page_header(webpage);
  html_insert_page_body(webpage);
  html_insert_page_menu(webpage);

  if (http->method() == HTTP_GET) {
    html_insert_conf_content(webpage);
  } else {
    store_new_config = validate_config(webpage);
  }

  html_insert_page_footer(webpage);

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());

  if (store_new_config) {
    config_write();
    system_reboot();
  }
}

static void handle_setup_cb(void) {
  bool store_new_config = false;
  String webpage;

  if (strlen(config->user_name) && strlen(config->user_pass)) {
    if (!authenticated()) return;
  }

  if (!webpage.reserve(4*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  webpage += html_page_header;
  webpage += F("<script type='text/javascript'>\n\n");
  webpage += F(" \
    function fill_wifi_selection() {\n \
      var select = document.getElementsByName('wifi_ssid_sel')[0];\n \
      select.options.length = 0;\n \
  ");

  List<NetAccessPoint>::iterator it;
  List<NetAccessPoint> ap;

  net_scan_wifi(ap);

  for (it=ap.begin(); it!=ap.end(); it++) {
    String enc = (*it).encrypted ? " *" : "";

    webpage += "select.options[select.options.length] = new Option('";
    webpage += (*it).ssid + enc + "', '" + (*it).ssid + "');\n";
  }

  webpage += F(" \
    }\n \
    </script>\n \
  ");

  webpage += html_page_body;

  if (http->method() == HTTP_GET) {
    html_insert_setup_content(webpage);
  } else {
    store_new_config = validate_config(webpage);
  }

  webpage += html_page_footer;

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());

  if (store_new_config) {
    config_write();
    system_reboot();
  }
}

static void handle_login_cb(void) {
  String webpage, header, msg;

  if (!setup_complete()) return;

  msg = "Enter username and password!";

  if (!webpage.reserve(1*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  if (http->hasHeader("Cookie")) {
    String cookie = http->header("Cookie");
  }

  if (http->hasArg("LOGOUT")) {
    send_header("0", "/login");

    return;
  }

  String user = config->user_name;
  String pass = config->user_pass;

  if (http->hasArg("USER") && http->hasArg("PASS")){
    if ((http->arg("USER") == user) && (http->arg("PASS") == pass)) {
      send_header(session_key, "/");

      return;
    }

    msg = "Login failed, try again!\n";
    log_print(F("HTTP: login failed\n"));
  }

  webpage += html_page_header;
  webpage += html_page_body;

  html_insert_login_content(msg, webpage);

  webpage += html_page_footer;

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());
}

static void handle_root_cb(void) {
  String webpage;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  if (!webpage.reserve(1*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  html_insert_page_header(webpage);
  html_insert_page_refresh(webpage);
  html_insert_page_body(webpage);
  html_insert_page_menu(webpage);
  html_insert_root_content(webpage);
  html_insert_page_footer(webpage);

  http->send(200, "text/html", webpage);
  system_count_net_traffic(webpage.length());
}

static void handle_style_cb(void) {
  http->sendHeader("Cache-Control", "max-age=86400");
  http->send(200, "text/css", html_page_style);
  system_count_net_traffic(strlen_P((PGM_P)html_page_style));
}

static void handle_script_cb(void) {
  http->sendHeader("Cache-Control", "max-age=86400");
  http->send(200, "text/javascript", html_page_script);
  system_count_net_traffic(strlen_P((PGM_P)html_page_script));
}

static void handle_404_cb(void) {
#ifndef RELEASE
  String webpage;

  if (!webpage.reserve(1*1024)) {
    log_print(F("HTTP: failed to allocate memory\n"));
  }

  webpage += html_page_header;
  webpage += html_page_body;
  webpage += html_page_menu;
  webpage += F("<h3>404 File Not Found</h3>\n");
  webpage += F("<p>\n");
  webpage += F("URI: ");
  webpage += http->uri();
  webpage += F("<br />\nMethod: ");
  webpage += (http->method() == HTTP_GET) ? "GET" : "POST";
  webpage += F("<br />\nArguments: ");
  webpage += http->args();
  webpage += F("<br />\n");

  for (uint8_t i = 0; i < http->args(); i++) {
    webpage += " " + http->argName(i) + ": " + http->arg(i) + "<br />\n";
  }

  webpage += F("</p>\n");
  webpage += html_page_footer;

  http->send(404, "text/html", webpage);
  system_count_net_traffic(webpage.length());
#endif
}

bool http_init(void) {
  if (http) return (false);

  create_session_key();

  http = new ESP8266WebServer(80);

  http->onNotFound(handle_404_cb);

  http->on("/",          HTTP_GET,  handle_root_cb);
  http->on("/info",      HTTP_GET,  handle_info_cb);
  http->on("/sys",       HTTP_GET,  handle_sys_cb);
  http->on("/reboot",    HTTP_GET,  handle_reboot_cb);

  http->on("/login",     handle_login_cb);
  http->on("/setup",     handle_setup_cb);
  http->on("/conf",      handle_conf_cb);

  http->on("/style.css", HTTP_GET, handle_style_cb);
  http->on("/script.js", HTTP_GET, handle_script_cb);

  http->on("/update",    HTTP_GET,  handle_update_start_cb);
  http->on("/update",    HTTP_POST, handle_update_finished_cb,
                                    handle_update_progress_cb);

  // the list of headers to be recorded
  const char *headerkeys[] = { "User-Agent", "Cookie" };
  size_t headerkeyssize = sizeof (headerkeys) / sizeof (char *);

  // ask server to track these headers
  http->collectHeaders(headerkeys, headerkeyssize);

  html_init();
  http->begin();

  return (true);
}

bool http_poll(void) {
  if (!http) return (false);

  if (net_connected()) {
    http->handleClient(); // answer http requests
  }

  return (true);
}
