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
#include <FS.h>

#include "filesystem.h"
#include "telemetry.h"
#include "websocket.h"
#include "storage.h"
#include "console.h"
#include "at24c32.h"
#include "update.h"
#include "config.h"
#include "system.h"
#include "module.h"
#include "clock.h"
#include "html.h"
#include "icon.h"
#include "mdns.h"
#include "led.h"
#include "ntp.h"
#include "rtc.h"
#include "log.h"
#include "net.h"

#include "webserver.h"

#define SESSION_TIMEOUT  3600
#define HTML_BUFFER_SIZE 2800

//#define LOG_PAGE_SIZE

struct Session {
  Session(bool create = true);
  ~Session(void);

  void update(void);

  static Session *load(void);

  time_t expires;
  char   key[16];
};

struct HTTP_PrivateData {
  ESP8266WebServer *webserver;

  uint32_t ap_addr;

  char user[17];
  char pass[33];

  Session *session;

  bool ota_enabled;
  int delayed_reboot;
};

static HTTP_PrivateData *p = NULL;

// current upload
File fs_upload_file;

// global buffer for dynamic HTML code
static String html;

static const char PROGMEM image_png_str[]       = "image/png";
static const char PROGMEM text_javascript_str[] = "text/javascrip";
static const char PROGMEM text_css_str[]        = "text/css";

static const char PROGMEM charset[] = "abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "0123456789";

static void trigger_reboot(int iterations) {
  if (p) p->delayed_reboot = iterations;
}

static const String get_content_type(const String &filename) {
       if (filename.endsWith(F(".html"))) return (F("text/html"));
  else if (filename.endsWith(F(".htm")))  return (F("text/html"));
  else if (filename.endsWith(F(".css")))  return (F("text/css"));
  else if (filename.endsWith(F(".xml")))  return (F("text/xml"));
  else if (filename.endsWith(F(".png")))  return (F("image/png"));
  else if (filename.endsWith(F(".gif")))  return (F("image/gif"));
  else if (filename.endsWith(F(".jpg")))  return (F("image/jpeg"));
  else if (filename.endsWith(F(".ico")))  return (F("image/x-icon"));
  else if (filename.endsWith(F(".mp3")))  return (F("audio/mpeg"));
  else if (filename.endsWith(F(".pdf")))  return (F("application/pdf"));
  else if (filename.endsWith(F(".zip")))  return (F("application/zip"));
  else if (filename.endsWith(F(".gz")))   return (F("application/gzip"));
  else if (filename.endsWith(F(".js")))   return (F("application/javascript"));
                                          return (F("text/plain"));
}

Session::Session(bool create) {
  bool active = true;

  at24c32_write(0x10, &active,  sizeof (active));

  if (create) {
    for (int n=0; n<sizeof (key) - 1; n++) {
      key[n] = pgm_read_byte(&charset[RANDOM_REG32 % 62]);
    }
    key[sizeof (key) - 1] = '\0';
    at24c32_write(0x18, key, sizeof (key));

    update();
  }
}

Session::~Session(void) {
  bool active = false;

  at24c32_write(0x10, &active,  sizeof (active));
}

void Session::update(void) {
  expires = system_utc() + SESSION_TIMEOUT;
  at24c32_write(0x14, &expires, sizeof (expires));
}

Session *Session::load(void) {
  Session *s = NULL;
  bool active;

  at24c32_read(0x10, &active,  sizeof (active));

  if (active) {
    s = new Session(false); // false = do not create new session

    at24c32_read(0x14, &s->expires, sizeof (s->expires));
    at24c32_read(0x18, &s->key[0],  sizeof (s->key));
  }

  return (s);
}

static void send_page_P(const char *html, const char *type, int32_t length = -1) {
  led_flash(LED_YEL);

  if (p->session) p->session->update();

  if (length < 0) length = strlen_P((PGM_P)html);

  p->webserver->sendHeader(F("Cache-Control"), F("max-age=86400"));
  p->webserver->send_P(200, type, html, length);

#ifdef LOG_PAGE_SIZE
  log_print(F("HTTP: serving (complete) page (flash) -> %i bytes"), length);
#endif
}

static void send_page(String &html,
                      const String &type,
                      int code = 200) {

  led_flash(LED_YEL);

  if (p->session) p->session->update();

  p->webserver->send(code, type, html);

#ifdef LOG_PAGE_SIZE
  log_print(F("HTTP: serving (complete) page (RAM) -> %i bytes"), html.length());
#endif

  // free buffer
  html = String();
}

static void send_page_chunk(String &html) {
  led_flash(LED_YEL);

  if (p->session) p->session->update();

  p->webserver->sendContent(html);

#ifdef LOG_PAGE_SIZE
  log_print(F("HTTP: serving chunk -> %i bytes"), html.length());
#endif

  // reset buffer
  html = "";
}

static void send_page_header(bool menu = true) {
  p->webserver->setContentLength(CONTENT_LENGTH_UNKNOWN);
  p->webserver->send(200, F("text/html"), String());

  html_insert_page_header(html);
  html_insert_page_body(html, menu);

  send_page_chunk(html);
}

static void send_page_footer(void) {
  html_insert_page_footer(html);

  send_page_chunk(html);
  p->webserver->sendContent(""); // end of chunked page

  // free buffer
  html = String();
}

static bool setup_complete(void) {
  if ((p->user[0] != '\0') && (p->pass[0] != '\0')) {
    return (true);
  }

  html += F("HTTP/1.1 301 OK\n");
  html += F("Location: /setup?init=1\n");
  html += F("Cache-Control: no-cache\n\n");

  send_page_chunk(html);

  return (false);
}

static bool authenticated(void) {
  if (p->webserver->hasHeader(F("Cookie"))) {
    String cookie = p->webserver->header(F("Cookie"));
    String name = F("GENESYS_SESSION_KEY=");

    if (!p->session) {
      p->session = new Session();
    }

    if (cookie.indexOf(name + p->session->key) != -1) {
      p->ota_enabled = true;
      return (true);
    }

    delete (p->session);
    p->session = NULL;
  }

  html += F("HTTP/1.1 301 OK\n");
  html += F("Location: /login\n");
  html += F("Cache-Control: no-cache\n\n");

  send_page_chunk(html);

  return (false);
}

static void set_request_origin(void) {
  bool connected_via_softap = true;

  IPAddress client_ip = p->webserver->client().remoteIP();
  IPAddress softap_ip(p->ap_addr);

  for (int i=0; i<3; i++) {
    if (client_ip[i] != softap_ip[i]) connected_via_softap = false;
  }

  if (connected_via_softap) {
    html_client_connected_via_softap();
  } else {
    html_client_connected_via_wifi();
  }
}

static void send_auth(const String &key, const String &redirect) {
  html += F("HTTP/1.1 301 OK\n");
  html += F("Set-Cookie: GENESYS_SESSION_KEY=");
  html += key;
  html += F("\n");
  html += F("Location: ");
  html += redirect;
  html += F("\n");
  html += F("Cache-Control: no-cache\n\n");

  send_page_chunk(html);
}

static void send_redirect(const String &redirect) {
  html += F("HTTP/1.1 301 OK\n");
  html += F("Location: ");
  html += redirect;
  html += F("\n");
  html += F("Cache-Control: no-cache\n\n");

  send_page_chunk(html);
}

static void handle_file_action_cb(void) {
  char buf[64];
  String path;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  if (!rootfs) {
    log_print(F("HTTP: filesytem not mounted"));

    return;
  }

  if (p->webserver->hasArg(F("path"))) {
    path = p->webserver->arg(F("path"));
  } else {
    log_print(F("HTTP: no filename specified"));
    p->webserver->send(500, F("text/plain"), F("MISSING ARG"));

    return;
  }

  if (rootfs->exists(path)) {
    if (p->webserver->uri() == F("/delete")) {
      log_print(F("HTTP: deleting file '%s'"), path.c_str());
      rootfs->remove(path);
      send_redirect(F("/files"));

      return;
    }

    if (p->webserver->uri() == F("/view")) {
      snprintf_P(
        buf, sizeof (buf), PSTR("inline; filename=\"%s\""), path.c_str()
      );
    } else {
      snprintf_P(
        buf, sizeof (buf), PSTR("attachment; filename=\"%s\""), path.c_str()
      );
    }

    // set filename and force download/view
    p->webserver->sendHeader(F("Content-Disposition"), buf);

    led_flash(LED_YEL);

    File file = rootfs->open(path, "r");
    p->webserver->streamFile(file, get_content_type(path));
    file.close();
  } else {
    log_print(F("HTTP: file not found: %s"), path.c_str());
    p->webserver->send(404, F("text/plain"), F("FILE NOT FOUND"));
  }
}

static void handle_file_upload_done_cb(void) {
  send_redirect(F("/files"));
}

static void handle_file_upload_cb(void) {
  HTTPUpload &upload = p->webserver->upload();

  if (upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;

    if (rootfs) {
      fs_upload_file = rootfs->open(filename, "w");

      log_print(F("HTTP: uploading file '%s'"), filename.c_str());

      if (!fs_upload_file) {
        log_print(F("HTTP: could not open file for writing"));
      }
    } else {
      log_print(F("HTTP: filesytem not mounted"));
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fs_upload_file) {
      fs_upload_file.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fs_upload_file) {
      fs_upload_file.close();

      log_print(F("HTTP: uploaded %i bytes"), upload.totalSize);
    }
  }
}

static void handle_file_page_cb(void) {
  String path;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  if (p->webserver->hasArg(F("path"))) {
    path = p->webserver->arg(F("path"));
  }

  send_page_header();
  html_insert_file_content(html, path);
  send_page_chunk(html);
  send_page_footer();
}

static void handle_update_start_cb(void) {
  if (!p->ota_enabled) {
    p->webserver->send(403, F("text/plain"), F("Login before OTA update ..."));

    return;
  }

  console_kill_shell();

  log_print(F("HTTP: freeing memory for OTA update ..."));

  telemetry_fini();
  storage_fini();
  update_fini();
  mdns_fini();
  ntp_fini();
  rtc_fini();
  fs_fini();

  led_off(LED_GRN);
  led_off(LED_YEL);

  log_print(F("HTTP: waiting for OTA update ..."));

  websocket_broadcast_message(F("update"));

  p->webserver->send(200, F("text/plain"), F("Waiting for OTA update ..."));
}

static void handle_update_finished_cb(void) {
  if (!p->ota_enabled) {
    p->webserver->send(403, F("text/plain"), F("Login before OTA update ..."));

    return;
  }

  html += F("Update ");
  html += (Update.hasError()) ? F("FAILED") : F("OK");
  html += F("!\nRebooting ...\n\n");

  trigger_reboot(2000);

  p->webserver->send(200, F("text/plain"), html);

  html = "";
}

static void handle_update_progress_cb(void) {
  uint32_t free_space = (system_free_sketch_space() - 0x1000) & 0xFFFFF000;
  HTTPUpload &upload = p->webserver->upload();
  static int received = 0, last_perc = -1;
  StreamString out;

  if (!p->ota_enabled) {
    p->webserver->send(403, F("text/plain"), F("Login before OTA update ..."));

    return;
  }

  if (upload.status == UPLOAD_FILE_START){
    log_print(F("HTTP: available space: %u bytes"), free_space);
    log_print(F("HTTP: filename: %s"), upload.filename.c_str());

    if (!Update.begin(free_space)) {
      Update.printError(out);
      log_print(out.readString().c_str());
    }

    led_off(LED_GRN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(out);
      log_print(out.readString().c_str());

      led_off(LED_GRN);
    } else {
      received += upload.currentSize;

      int perc = 100 * received / system_sketch_size();

      if (perc != last_perc) {
        log_progress(F("HTTP: received "), "%", perc);
        last_perc = perc;
      }

      led_toggle(LED_GRN);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true to set the size to the current progress
      log_print(F("HTTP: update successful: %u bytes"), upload.totalSize);

      led_on(LED_GRN);
    } else {
      Update.printError(out);
      log_print(out.readString().c_str());

      led_off(LED_GRN);
    }
  }
}

static void handle_info_cb(void) {
  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  send_page_header();
  html_insert_info_content(html);
  send_page_chunk(html);
  send_page_footer();
}

static void handle_wifi_scan_cb(void) {
  net_scan_wifi();

  html += F("[");
  html_insert_wifi_list(html);
  html += F("]\n\n");

  send_page(html, F("text/plain"));
}

#ifdef ALPHA
static void handle_sys_cb(void) {
  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  send_page_header();

  // send modules
  html_insert_module_header(html);
  send_page_chunk(html);
  for (int i=0; i<module_count(); i++) {
    html_insert_module_row(html, i);
    send_page_chunk(html);
  }
  html_insert_module_footer(html);
  send_page_chunk(html);

  // rest of sys page
  html_insert_sys_content(html);
  send_page_chunk(html);
  send_page_footer();
}
#endif

#ifndef RELEASE
static void handle_log_cb(void) {
  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  send_page_header();

  // rest of sys page
  html_insert_log_content(html);
  send_page_chunk(html);
  send_page_footer();
}
#endif

static bool config_parse(String &str) {
  bool config_ok = true;

  for (int i=0; i<p->webserver->args(); i++) {
    String name = p->webserver->argName(i);
    String arg = p->webserver->arg(i);

    if (!config_set(name, arg)) {
      if (config_ok) str += F("<br />");
      str += name;
      str += F(" has an invalid value!<br />\n");
      config_ok = false;
    }
  }

  if (!config->wifi_enabled) {
    str += F("<br /><b>WiFI is disabled.</b>\n");
    if (!config->storage_enabled) {
      str += F("<br /><b>Enabling local storage.</b>\n");
      config->storage_enabled = 1;
    }
    if (!config->ap_enabled) {
      str += F("<br /><b>Enabling local AP.</b>\n");
      config->ap_enabled = 1;
    }
    str += F("<br />\n");
  }

  return (config_ok);
}

static void handle_conf_cb(void) {
  bool conf = (p->webserver->uri() == F("/conf"));
  bool menu = true;

  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  if (p->webserver->hasArg(F("init"))) menu = false;

  send_page_header(menu);

  config_init();

  if (p->webserver->method() == HTTP_GET) {
    html_insert_conf_content(html, CONF_HEADER);
    send_page_chunk(html);

    if (!conf) {
      // [SETUP]

      html_insert_conf_content(html, CONF_USER);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_DEVICE);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_WIFI);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_IP);
      send_page_chunk(html);
    } else {
      // [CONF]

      html_insert_conf_content(html, CONF_AP);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_NTP);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_TELEMETRY);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_STORAGE);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_MDNS);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_UPDATE);
      send_page_chunk(html);

      html_insert_conf_content(html, CONF_LOGGER);
      send_page_chunk(html);
    }

    html_insert_conf_content(html, CONF_FOOTER);
    send_page_chunk(html);
  } else {
    if (config_parse(html)) {
      trigger_reboot(2000);
    } else {
      trigger_reboot(20000);
    }
    config_write();

    html += F("<br />Config saved.\n");
    html_insert_websocket_script(html);
    send_page_chunk(html);
  }

  config_fini();

  send_page_footer();
}

static void handle_login_cb(void) {
  if (!setup_complete()) return;

  set_request_origin();

  if (p->webserver->hasArg(F("LOGOUT"))) {
    send_auth(F("0"), F("/login"));

    delete (p->session);
    p->session = NULL;

#ifdef RELEASE
    p->ota_enabled = false;
#endif

    return;
  }

  String msg = F("Enter username and password!");

  if (p->webserver->hasArg(F("USER")) && p->webserver->hasArg(F("PASS"))) {
    if (p->webserver->arg(F("USER")) == p->user) {
      if (p->webserver->arg(F("PASS")) == p->pass) {
        if (!p->session) {
          p->session = new Session();
        }

        send_auth(p->session->key, F("/"));

        return;
      }
    }

    msg = F("Login failed, try again!\n");
    log_print(F("HTTP: login failed"));
  }

  send_page_header(false); // false = no menu
  html_insert_login_content(html, msg);
  send_page_chunk(html);
  send_page_footer();
}

static void handle_root_cb(void) {
  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  send_page_header();
  html_insert_root_content(html);
  send_page_chunk(html);
  send_page_footer();
}

static void handle_clock_cb(void) {
  if (!setup_complete()) return;
  if (!authenticated()) return;

  set_request_origin();

  send_page_header();
  html_insert_clock_content(html);
  send_page_chunk(html);
  send_page_footer();
}

static void handle_style_css_cb(void) {
  send_page_P(html_page_style_css, text_css_str);
}

static void handle_config_js_cb(void) {
  send_page_P(html_page_config_js, text_javascript_str);
}

static void handle_common_js_cb(void) {
  send_page_P(html_page_common_js, text_javascript_str);
}

static void handle_root_js_cb(void) {
  send_page_P(html_page_root_js,  text_javascript_str);
}

static void handle_clock_js_cb(void) {
  send_page_P(html_page_clock_js,  text_javascript_str);
}

static void handle_logo_js_cb(void) {
  send_page_P(html_page_logo_js,   text_javascript_str);
}

static void handle_sys_js_cb(void) {
  send_page_P(html_page_sys_js,    text_javascript_str);
}

static void handle_favicon_cb(void) {
  send_page_P(fav_png,  image_png_str, fav_len);
}

static void handle_saveicon_cb(void) {
  send_page_P(save_png, image_png_str, save_len);
}

static void handle_viewicon_cb(void) {
  send_page_P(view_png, image_png_str, view_len);
}

static void handle_delicon_cb(void) {
  send_page_P(del_png,  image_png_str, del_len);
}

static void handle_404_cb(void) {
#ifndef ALPHA
  send_redirect(F("/"));
#else
  html_insert_page_header(html);
  html_insert_page_body(html);

  html += F("<h3>404 File Not Found</h3>\n");
  html += F("<p>\n");
  html += F("URI: ");
  html += p->webserver->uri();
  html += F("<br />\nMethod: ");
  html += (p->webserver->method() == HTTP_GET) ? F("GET") : F("POST");
  html += F("<br />\nArguments: ");
  html += p->webserver->args();
  html += F("<br />\n");

  for (uint8_t i = 0; i < p->webserver->args(); i++) {
    html += F(" ");
    html += p->webserver->argName(i);
    html += F(": ");
    html += p->webserver->arg(i);
    html += F("<br />\n");
  }

  html += F("</p>\n");

  html_insert_page_footer(html);
  send_page(html, F("text/html"), 404);
#endif
}

int webserver_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool webserver_init(void) {
  String str;

  if (p) return (false);

  config_init();

  if (bootup) {
    if (!config->webserver_enabled) {
      log_print(F("HTTP: webserver disabled in config"));

      config_fini();

      return (false);
    }
  }

  log_print(F("HTTP: initializing webserver"));

  p = (HTTP_PrivateData *)malloc(sizeof (HTTP_PrivateData));
  memset(p, 0, sizeof (HTTP_PrivateData));

#ifdef ALPHA
  p->ota_enabled = true;
#endif

  if (config->ap_enabled) p->ap_addr = config->ap_addr;

  config_get(F("user_name"), str);
  strncpy(p->user, str.c_str(), sizeof (p->user));
  config_get(F("user_pass"), str);
  strncpy(p->pass, str.c_str(), sizeof (p->pass));

  config_fini();

  p->webserver = new ESP8266WebServer(80);

  p->webserver->onNotFound(handle_404_cb);

  p->webserver->on(F("/login"),                handle_login_cb);
  p->webserver->on(F("/setup"),                handle_conf_cb);
  p->webserver->on(F("/conf"),                 handle_conf_cb);

  p->webserver->on(F("/"),          HTTP_GET,  handle_root_cb);
  p->webserver->on(F("/clock"),     HTTP_GET,  handle_clock_cb);
  p->webserver->on(F("/info"),      HTTP_GET,  handle_info_cb);
#ifdef ALPHA
  p->webserver->on(F("/sys"),       HTTP_GET,  handle_sys_cb);
#endif
#ifndef RELEASE
  p->webserver->on(F("/log"),       HTTP_GET,  handle_log_cb);
#endif
  p->webserver->on(F("/scan"),      HTTP_GET,  handle_wifi_scan_cb);

  p->webserver->on(F("/fav.png"),   HTTP_GET,  handle_favicon_cb);
  p->webserver->on(F("/save.png"),  HTTP_GET,  handle_saveicon_cb);
  p->webserver->on(F("/view.png"),  HTTP_GET,  handle_viewicon_cb);
  p->webserver->on(F("/del.png"),   HTTP_GET,  handle_delicon_cb);

  p->webserver->on(F("/style.css"), HTTP_GET,  handle_style_css_cb);
  p->webserver->on(F("/config.js"), HTTP_GET,  handle_config_js_cb);
  p->webserver->on(F("/common.js"), HTTP_GET,  handle_common_js_cb);
  p->webserver->on(F("/clock.js"),  HTTP_GET,  handle_clock_js_cb);
  p->webserver->on(F("/root.js"),   HTTP_GET,  handle_root_js_cb);
  p->webserver->on(F("/logo.js"),   HTTP_GET,  handle_logo_js_cb);
  p->webserver->on(F("/sys.js"),    HTTP_GET,  handle_sys_js_cb);

  p->webserver->on(F("/update"),    HTTP_GET,  handle_update_start_cb);
  p->webserver->on(F("/update"),    HTTP_POST, handle_update_finished_cb,
                                               handle_update_progress_cb);

  p->webserver->on(F("/files"),     HTTP_GET,  handle_file_page_cb);
  p->webserver->on(F("/view"),      HTTP_GET,  handle_file_action_cb);
  p->webserver->on(F("/download"),  HTTP_GET,  handle_file_action_cb);
  p->webserver->on(F("/delete"),    HTTP_GET,  handle_file_action_cb);
  p->webserver->on(F("/upload"),    HTTP_POST, handle_file_upload_done_cb,
                                               handle_file_upload_cb);

  // the list of headers to be recorded
  String h1 = F("User-Agent"), h2 = F("Cookie");
  const char *headerkeys[] = { h1.c_str(), h2.c_str() };
  size_t headerkeyssize = sizeof (headerkeys) / sizeof (char *);

  // ask server to track these headers
  p->webserver->collectHeaders(headerkeys, headerkeyssize);

  if (!html.reserve(HTML_BUFFER_SIZE)) {
    log_print(F("HTTP: failed to reserve %s for html buffer"),
      fs_format_bytes(HTML_BUFFER_SIZE).c_str()
    );
  }

  at24c32_init();
  icon_init();
  html_init();

  p->session = Session::load();

  p->webserver->begin();

  return (true);
}

bool webserver_fini(void) {
  if (!p) return (false);

  log_print(F("HTTP: shutting down webserver"));

  html = String();

  delete (p->webserver);
  delete (p->session);

  // free private p->data
  free(p);
  p = NULL;

  return (true);
}

void webserver_poll(void) {
  if (!p) return;

  p->webserver->handleClient();

  if (p->delayed_reboot) {
    if (p->delayed_reboot == 1) {
      p->delayed_reboot = 0;
      system_reboot();
    } else {
      p->delayed_reboot--;
    }
  }

  if (p->session && (system_utc() > p->session->expires)) {
    log_print(F("HTTP: session timeout, force logout"));

    delete (p->session);
    p->session = NULL;

#ifdef RELEASE
    p->ota_enabled = false;
#endif

    websocket_broadcast_message(F("logout"));
  }
}

MODULE(webserver)
