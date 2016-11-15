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

#include <Arduino.h>
#include <FS.h>

#include "filesystem.h"
#include "system.h"
#include "module.h"
#include "config.h"
#include "clock.h"
#include "log.h"
#include "net.h"

#include "html.h"

//#define LOG_BUFFER_USAGE

const char *html_page_style_css;
const char *html_page_reset_css;

const char *html_page_config_js;
const char *html_page_common_js;
const char *html_page_sys_js;

static const char *conf_header;
static const char *conf_user_content;
static const char *conf_device_content;
static const char *conf_wifi_content;
static const char *conf_ip_content;
static const char *conf_ap_content;
static const char *conf_mdns_content;
static const char *conf_ntp_content;
static const char *conf_rtc_content;
static const char *conf_transport_content;
static const char *conf_update_content;
static const char *conf_footer;

static const char *conf_storage_header;
static const char *conf_storage_values;
static const char *conf_storage_interval;
static const char *conf_storage_footer;

static const char *websocket_content;
static const char *login_content;
static const char *sys_content;

static const char *page_header;
static const char *page_menu_sta;
static const char *page_menu_ap;
static const char *page_footer;

static const char *upload_form;

static bool client_connected_via_softap = false;

static void check_buffer_size(int len, int size, const String &purpose) {
#ifdef DEBUG
  if (len >= size) {
    log_print(F("HTML: %s buffer too small (size=%i, need=%i)\r\n"), 
      purpose.c_str(), size, len
    );
  }

#ifdef LOG_BUFFER_USAGE
  log_print(F("HTML: %s buffer = %i bytes\r\n"), purpose.c_str(), len);
#endif

#endif // DEBUG
}

static void insert_wifi_script(String &html) {
  const String &wifi = net_list_wifi();

  html += F("\n<script>\n");
  html += F("var ssid_in_conf ='");
  html += config->wifi_ssid;
  html += F("';\n");
  html += F("var wifi = [\n");
  html += wifi;
  html += F("];\n");
  html += F("</script>\n\n");
}
 
static void insert_websocket_script(String &html) {
  char buf[250];

  int len = snprintf_P(buf, sizeof (buf), websocket_content,
    1,
    //(client_connected_via_softap) ? 0 : 1,
    (client_connected_via_softap) ? net_ap_ip().c_str() : net_ip().c_str()
  );

  check_buffer_size(len, sizeof (buf), F("websocket script"));

  html += buf;
}

void html_client_connected_via_softap(void) {
  client_connected_via_softap = true;
}

void html_client_connected_via_wifi(void) {
  client_connected_via_softap = false;
}

void html_insert_root_content(String &html) {
  char buf[200];

  html += F("\
    <h3>ADC</h3>\n\
    <br />\n\
    <label for='adc' class='meter'>Value: </label>\n\
    <input id='adc' type='text' readonly />\n\
    <br />\n\
  ");

  insert_websocket_script(html);
}

void html_insert_info_content(String &html) {
  html += F("<xmp class='fixed'>");
  system_device_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_version_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_build_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_load_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_sys_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_flash_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_net_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_ap_info(html);
  html += F("</xmp>\n");
  html += F("<hr />");
  html += F("<xmp class='fixed'>");
  system_wifi_info(html);
  html += F("</xmp>\n");

  insert_websocket_script(html);
}

static int insert_conf_user(String &html) {
  char buf[300];

  int len = snprintf_P(buf, sizeof (buf), conf_user_content,
    config->user_name
  );

  check_buffer_size(len, sizeof (buf), F("user conf"));

  html += buf;

  return (len);
}

static int insert_conf_device(String &html) {
  char buf[200];

  int len = snprintf_P(buf, sizeof (buf), conf_device_content,
    config->device_name
  );

  check_buffer_size(len, sizeof (buf), F("device conf"));

  html += buf;

  return (len);
}

static int insert_conf_wifi(String &html) {
  char buf[1000];

  insert_wifi_script(html);

  int len = snprintf_P(buf, sizeof (buf), conf_wifi_content,
    config->wifi_enabled ? "" : "checked",
    config->wifi_enabled ? "checked" : "",
    config->wifi_ssid,
    config->wifi_power,
    config->wifi_watchdog
  );

  check_buffer_size(len, sizeof (buf), F("wifi conf"));

  html += buf;

  return (len);
}

static int insert_conf_ip(String &html) {
  char buf[800];

  IPAddress ip_addr(config->ip_addr);
  IPAddress ip_netmask(config->ip_netmask);
  IPAddress ip_gateway(config->ip_gateway);
  IPAddress ip_dns1(config->ip_dns1);
  IPAddress ip_dns2(config->ip_dns2);

  int len = snprintf_P(buf, sizeof (buf), conf_ip_content,
    config->ip_static ? "" : "checked",
    config->ip_static ? "checked" : "",
	 ip_addr.toString().c_str(),
	 ip_netmask.toString().c_str(),
	 ip_gateway.toString().c_str(),
	 ip_dns1.toString().c_str(),
	 ip_dns2.toString().c_str()
  );

  check_buffer_size(len, sizeof (buf), F("ip conf"));

  html += buf;

  return (len);
}

static int insert_conf_mdns(String &html) {
  char buf[400];

  int len = snprintf_P(buf, sizeof (buf), conf_mdns_content,
    config->mdns_enabled ? "" : "checked",
    config->mdns_enabled ? "checked" : "",
    config->device_name
  );

  check_buffer_size(len, sizeof (buf), F("mdns conf"));

  html += buf;

  return (len);
}

static int insert_conf_ap(String &html) {
  char buf[400];

  IPAddress ap_addr(config->ap_addr);

  int len = snprintf_P(buf, sizeof (buf), conf_ap_content,
    config->ap_enabled ? "" : "checked",
    config->ap_enabled ? "checked" : "",
    ap_addr.toString().c_str()
  );

  check_buffer_size(len, sizeof (buf), F("ap conf"));

  html += buf;

  return (len);
}

static int insert_conf_ntp(String &html) {
  char buf[550];

  int len = snprintf_P(buf, sizeof (buf), conf_ntp_content,
    config->ntp_enabled ? "" : "checked",
    config->ntp_enabled ? "checked" : "",
    config->ntp_server,
    config->ntp_interval
  );

  check_buffer_size(len, sizeof (buf), F("ntp conf"));

  html += buf;

  return (len);
}

static int insert_conf_rtc(String &html) {
  html += FPSTR(conf_rtc_content);

  return (0);
}

static int insert_conf_transport(String &html) {
  char buf[1350];

  int len = snprintf_P(buf, sizeof (buf), conf_transport_content,
    config->transport_enabled ? "" : "checked",
    config->transport_enabled ? "checked" : "",
    config->transport_url,
    config->transport_user,
    config->transport_pass,
    config->transport_interval
  );

  check_buffer_size(len, sizeof (buf), F("transport conf"));

  html += buf;

  return (len);
}

static int insert_conf_update(String &html) {
  char buf[600];

  int len = snprintf_P(buf, sizeof (buf), conf_update_content,
    config->update_enabled ? "" : "checked",
    config->update_enabled ? "checked" : "",
    config->update_url,
    config->update_interval
  );

  check_buffer_size(len, sizeof (buf), F("update conf"));

  html += buf;

  return (len);
}

static int insert_conf_storage(String &html) {
  char buf[400];

  int len = snprintf_P(buf, sizeof (buf), conf_storage_header,
    config->storage_enabled ? "" : "checked",
    config->storage_enabled ? "checked" : "",
    config->storage_interval,
    config->storage_mask
  );

  check_buffer_size(len, sizeof (buf), F("storage conf"));

  html += buf;

  html += FPSTR(conf_storage_values);
  html += FPSTR(conf_storage_interval);
  html += FPSTR(conf_storage_footer);

  return (len);
}

static int insert_conf_header(String &html) {
  int len = html.length();

  html += FPSTR(conf_header);

  return (html.length() - len);
}

static int insert_conf_footer(String &html) {
  int len = html.length();

  html += FPSTR(conf_footer);
  insert_websocket_script(html);

  return (html.length() - len);
}

void html_insert_conf_content(String &html, int conf) {
  int len = 0;

       if (conf == CONF_HEADER)  len = insert_conf_header(html);
  else if (conf == CONF_USER)    len = insert_conf_user(html);
  else if (conf == CONF_DEVICE)  len = insert_conf_device(html);
  else if (conf == CONF_WIFI)    len = insert_conf_wifi(html);
  else if (conf == CONF_IP)      len = insert_conf_ip(html);
  else if (conf == CONF_AP)      len = insert_conf_ap(html);
  else if (conf == CONF_MDNS)    len = insert_conf_mdns(html);
  else if (conf == CONF_NTP)     len = insert_conf_ntp(html);
  else if (conf == CONF_RTC)     len = insert_conf_rtc(html);
  else if (conf == CONF_MQTT)    len = insert_conf_transport(html);
  else if (conf == CONF_UPDATE)  len = insert_conf_update(html);
  else if (conf == CONF_STORAGE) len = insert_conf_storage(html);
  else if (conf == CONF_FOOTER)  len = insert_conf_footer(html);

#ifdef LOG_BUFFER_USAGE
  log_print(F("HTML: conf page = %i bytes\r\n"), len);
#endif
}

void html_insert_login_content(String &html, const String &msg) {
  char buf[800];

  insert_websocket_script(html);

  int len = snprintf_P(buf, sizeof (buf), login_content,
    system_hw_device().c_str(), system_device_name().c_str(), msg.c_str()
  );

  check_buffer_size(len, sizeof (buf), F("login content"));

  html += buf;
}

void html_insert_module_header(String &html) {
#ifdef DEBUG
  html += F("<h3>Module</h3>\n");
  html += F("<table cellspacing='0'>\n");
#endif
}

void html_insert_module_row(String &html, int module) {
#ifdef DEBUG
  char buf[550];
  int state;

  const char *fmt = PSTR("\
    <tr>\n\
      <td style='text-align:right;padding-right:15px'>%s</td>\n\
      <td style='padding-right:15px'>\n\
        <input id='module_%i_state' type='text' class='c' value='%s' />\
      </td>\n\
      <td>\n\
        <button type='button' class='medium'\
          onclick='if (connection) connection.send(\"module init %i\")'>Start\
        </button>\n\
        <button type='button' class='medium'\
          onclick='if (connection) connection.send(\"module fini %i\")'>Stop\
        </button>\n\
      <td>\n\
    </tr>\n\
  ");

  module_call_state(module, state);

  int len = snprintf_P(buf, sizeof (buf), fmt,
    module_name(module).c_str(), module,
    module_state_str(state).c_str(), module, module
  );

  html = buf;

  check_buffer_size(len, sizeof (buf), F("sys module line"));
#endif // DEBUG
}

void html_insert_module_footer(String &html) {
#ifdef DEBUG
  html += F("</table><hr />\n");
#endif
}

void html_insert_sys_content(String &html) {
#ifdef DEBUG
  html += FPSTR(sys_content);

  insert_websocket_script(html);
#endif
}

void html_insert_file_content(String &html, const String &path) {
  char buf[128];
  FSInfo info;
  int len;

  if (!rootfs) {
    html += F("<br />Module FS is INACTIVE<br />");

    insert_websocket_script(html);

    return;
  }

  Dir dir = rootfs->openDir(path);

  html += F("<br /><pre class='fixed'>");
  html += F("<b> File                   Size</b><hr/ >");
  while (dir.next()) {
    html += F(" ");
    html += dir.fileName();
    len = dir.fileName().length();
    for (int i=0; i<23-len; i++) html += F(" ");

    String size = fs_format_bytes(dir.fileSize());
    html += size;
    len = size.length();
    for (int i=0; i<10-len; i++) html += F(" ");

    snprintf_P(buf, sizeof (buf),
      PSTR("<a href='/view?path=%s'><img class='icon' src='view.png' alt='View' title='View in browser'></a> "),
      dir.fileName().c_str()
    );
    html += buf;

    snprintf_P(buf, sizeof (buf),
      PSTR("<a href='/download?path=%s'><img class='icon' src='save.png' alt='Download' title='Save to disk'></a> "),
      dir.fileName().c_str()
    );
    html += buf;

    snprintf_P(buf, sizeof (buf),
      PSTR("<a href='/delete?path=%s'><img class='icon' src='del.png' alt='Delete' title='Delete from device'></a>"),
      dir.fileName().c_str()
    );
    html += buf;

    html += F("<br />");
  }
  html += F("</pre><hr />\n");

#ifdef DEBUG
  html += FPSTR(upload_form);
#endif

  rootfs->info(info);

  html += F("total: ");
  html += fs_format_bytes(info.totalBytes);
  html += F("<br />");
  html += F("used:  ");
  html += fs_format_bytes(info.usedBytes);
  html += F("<br />");

  insert_websocket_script(html);
}

void html_insert_page_header(String &html) {
  char buf[400];

  int len = snprintf_P(buf, sizeof (buf), page_header,
    system_device_name().c_str()
  );

  check_buffer_size(len, sizeof (buf), F("page header"));

  html += buf;
}

void html_insert_page_body(String &html, bool menu) {
  html += F("<body>");

  if (menu) {
    if (client_connected_via_softap) {
      html += FPSTR(page_menu_ap);
    } else {
      html += FPSTR(page_menu_sta);
    }
  }

  html += F("<div id='content'>\n");
}

void html_insert_page_footer(String &html) {
  html += FPSTR(page_footer);
}

void html_insert_websocket_script(String &html) {
  insert_websocket_script(html);
}

bool html_init(void) {

  login_content = PSTR(" \
    <div class='login'>\n\
      <fieldset>\n\
        <center>\n\
        <h3>%s&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%s</h3>\n\
        </center>\n\
        <hr />\n\
        <form action='/login' method='POST'>\n\
          <label for='a'>Username: </label>\n\
          <input id='a' type='text' name='USER' placeholder='username' autofocus />\n\
          <br />\n\
          <label for='b'>Password: </label>\n\
          <input id='b' type='password' name='PASS' placeholder='password' />\n\
          <br /><br />\n\
          <center>\n\
            <input type='submit' value='Login' />\n\
          </center>\n\
          <br />\n\
        </form>\n\
        <hr />\n\
        <center>%s</center>\n\
      </fieldset>\n\
    </div>\n\
  ");

  websocket_content = PSTR(" \
    <script>\n\
      var connection = null;\n\
      var polling = false;\n\
      if (%i) {\n\
        connection = new WebSocket('ws://%s:81/', ['genesys']);\n\
        polling = true;\n\
      }\n\
    </script>\n\
    <script src='common.js'></script>\n\
  ");

  conf_header = PSTR(" \
    <script src='config.js'></script>\n\
    <br />\n\
    <div class='config'>\n\
    <form id='config' action='#' method='POST' enctype='multipart/form-data'>\n\
  ");

  conf_footer = PSTR(" \
      <input type='submit' value='Save' />\n\
    </form>\n\
    <br />\n\
    </div>\n\
  ");

  conf_user_content = PSTR(" \
    <fieldset>\n\
      <legend>User</legend>\n\
      <label>Username:</label>\n\
      <input name='user_name' maxlength='16' type='text' value='%s' />\n\
      <br />\n\
      <label>Password:</label>\n\
      <input name='user_pass' maxlength='28' type='password' />\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_device_content = PSTR(" \
    <fieldset>\n\
      <legend>Device</legend>\n\
      <label>Name:</label>\n\
      <input name='device_name' maxlength='16' type='text' value='%s' />\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_wifi_content = PSTR(" \
    <fieldset>\n\
      <legend>WiFi</legend>\n\
      <input name='wifi_enabled' type='radio' value='0' %s />Disabled<br />\n\
      <input name='wifi_enabled' type='radio' value='1' %s />Enabled<br />\n\
      <hr />\n\
      <label>Available:</label>\n\
      <select id='wifi_ssid_sel' onchange='wifi_select_changed()'>\n\
      </select>\n\
      <button class='tiny' name='wifi_scan' type='button'\n\
              onclick='wifi_scan_network()'>&#x21bb;\n\
      </button>\n\
      <br />\n\
      <label>SSID:</label>\n\
      <input name='wifi_ssid' maxlength='32' type='text' value='%s' />\n\
      <br />\n\
      <label>Password:</label>\n\
      <input name='wifi_pass' maxlength='28' type='password' />\n\
      <br />\n\
      <label>Power:</label>\n\
      <input name='wifi_power' type='number' value='%i' min='0' max='21' />\n\
      dBm\n\
      <br />\n\
      <label>Watchdog:</label>\n\
      <input name='wifi_watchdog' type='number' value='%i' min='0' max='60' />\n\
      minute(s)\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_ip_content = PSTR(" \
    <fieldset>\n\
      <legend>IP</legend>\n\
      <input class='radio' name='ip_static' type='radio' value='0' %s />\n\
      DHCP<br />\n\
      <input class='radio' name='ip_static' type='radio' value='1' %s />\n\
      Static<br />\n\
      <hr />\n\
      <label>Address:</label>\n\
      <input name='ip_addr' type='text' value='%s' />\n\
      <br />\n\
      <label>Netmask:</label>\n\
      <input name='ip_netmask' type='text' value='%s' />\n\
      <br />\n\
      <label>Gateway:</label>\n\
      <input name='ip_gateway' type='text' value='%s' />\n\
      <br />\n\
      <label>DNS1:</label>\n\
      <input name='ip_dns1' type='text' value='%s' />\n\
      <br />\n\
      <label>DNS2:</label>\n\
      <input name='ip_dns2' type='text' value='%s' />\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_mdns_content = PSTR(" \
    <fieldset>\n\
      <legend>mDNS</legend>\n\
      <input name='mdns_enabled' type='radio' value='0' %s />Disabled<br />\n\
      <input name='mdns_enabled' type='radio' value='1' %s />Enabled<br />\n\
      <hr />\n\
      <label>Name:</label>\n\
      <input type='text' value='%s' class='r' disabled readonly/>.local\n\
    </fieldset>\n\
    <br /><br />\n\
");

  conf_ap_content = PSTR(" \
    <fieldset>\n\
      <legend>AP</legend>\n\
      <input name='ap_enabled' type='radio' value='0' %s />Disabled<br />\n\
      <input name='ap_enabled' type='radio' value='1' %s />Enabled<br />\n\
      <hr />\n\
      <label>Address:</label>\n\
      <input name='ap_addr' type='text' value='%s' />\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_ntp_content = PSTR(" \
    <fieldset>\n\
      <legend>NTP</legend>\n\
      <input name='ntp_enabled' type='radio' value='0' %s />Disabled<br />\n\
      <input name='ntp_enabled' type='radio' value='1' %s />Enabled<br />\n\
      <hr />\n\
      <label>Server:</label>\n\
      <input name='ntp_server' maxlength='32' type='text' value='%s' />\n\
      <br />\n\
      <label>Sync Interval:</label>\n\
      <input name='ntp_interval' type='number' value='%i' min='1' max='1440' />\n\
      minute(s)\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_rtc_content = PSTR(" \
    <fieldset>\n\
      <legend>RTC</legend>\n\
      <label for='date'>Date: </label>\n\
      <input id='date' type='text' class='c' readonly />\n\
      <br />\n\
      <label for='time'>Time: </label>\n\
      <input id='time' type='text' class='c' readonly />\n\
      <hr />\n\
      <label for='browser'>Browser: </label>\n\
      <input id='browser' type='text' class='c' readonly />\n\
      <button class='tiny' type='button'\n\
              onclick='conf_rtc_browser_sync()'>&#10142;\n\
      </button>\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_transport_content = PSTR(" \
    <fieldset>\n\
      <legend>Transport</legend>\n\
      <input name='transport_enabled' type='radio' value='0' %s />Disabled<br />\n\
      <input name='transport_enabled' type='radio' value='1' %s />Enabled<br />\n\
      <hr />\n\
      <label>Protocol:</label>\n\
      <select name='transport_protocol' onchange='transport_select_changed()'>\n\
        <option value='0' %s >NATIV/MQTT</option>\n\
        <option value='1' %s >EMON/MQTT</option>\n\
        <option value='2' %s >EMON/JSON</option>\n\
      </select>\n\
      <br />\n\
      <label id='transport_url_label'>Broker:</label>\n\
      <input name='transport_url' maxlength='64' type='text' value='%s' />\n\
      <br />\n\
      <label>Username:</label>\n\
      <input name='transport_user' maxlength='16' type='text' value='%s' />\n\
      <br />\n\
      <label>Password:</label>\n\
      <input name='transport_pass' maxlength='28' type='password' />\n\
      <br />\n\
      <label>Key:</label>\n\
      <input name='transport_key' maxlength='32' type='text' value='%s' />\n\
      <br />\n\
      <label>Node:</label>\n\
      <input name='transport_node' type='number' value='%i' min='0' max='255'/>\n\
      <br />\n\
      <label>Send Interval:</label>\n\
      <input name='transport_interval' type='number' value='%i' min='1' max='3600' />\n\
      second(s)\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

  conf_update_content = PSTR(" \
    <fieldset>\n\
      <legend>Update</legend>\n\
      <input name='update_enabled' type='radio' value='0' %s />Disabled<br />\n\
      <input name='update_enabled' type='radio' value='1' %s />Enabled<br />\n\
      <hr />\n\
      <label>URL:</label>\n\
      <input name='update_url' maxlength='64' type='text' value='%s' />\n\
      <br />\n\
      <label>Poll Interval:</label>\n\
      <input name='update_interval' type='number' value='%i' min='1' max='240' />\n\
      hour(s)\n\
    </fieldset>\n\
    <br /><br />\n\
  ");

#ifdef DEBUG
  sys_content = PSTR(" \
    <script src='sys.js'></script>\n\
    <h3>Load</h3>\n\
    <table>\n\
      <tr>\n\
        <td style='padding-right:15px'>\n\
          <span class='legend' style='background-color:#c01010;'>\n\
            &nbsp;&nbsp;&nbsp;\n\
          </span>\n\
          &nbsp;CPU\n\ 
        </td>\n\
        <td style='text-align:right;padding-right:15px'>\n\
          <span id='load_cpu_perc'>0%</span>\n\
        </td>\n\
        <td>\n\
          <span id='load_cpu_loops'>0</span>\n\
        </td>\n\
      </tr>\n\
      <tr>\n\
        <td style='padding-right:15px'>\n\
          <span class='legend' style='background-color:#1010c0;'>\n\
            &nbsp;&nbsp;&nbsp;\n\
          </span>\n\
          &nbsp;Memory\n\ 
        </td>\n\
        <td style='text-align:right;padding-right:15px'>\n\
          <span id='load_mem_perc'>0%</span>\n\
        </td>\n\
        <td>\n\
          <span id='load_mem_free'>0</span>\n\
        </td>\n\
      </tr>\n\
      <tr>\n\
        <td style='padding-right:15px'>\n\
          <span class='legend' style='background-color:#10c010;'>\n\
            &nbsp;&nbsp;&nbsp;\n\
          </span>\n\
          &nbsp;Network\n\ 
        </td>\n\
        <td style='text-align:right;padding-right:15px'>\n\
          <span id='load_net_perc'>0%</span>\n\
        </td>\n\
        <td>\n\
          <span id='load_net_xfer'>0</span>\n\
        </td>\n\
      </tr>\n\
    </table>\n\
    <br />\n\
    \n\
    <canvas class='load' id='canvas_load'></canvas>\n\
    <br />\n\
    <hr />\n\
    \n\
    <h3>Log</h3>\n\
    <div class='fixed' id=syslog>\n\
      <span style='color:white'> LOADING ...</span>\n\
    </div>\n\
    <hr />\n\
    \n\
    <form class='table'>\n\
      <h3>Time</h3>\n\
      <label for='uptime'>Uptime: </label>\n\
      <input id='uptime' type='text' class='c' readonly />\n\
      <br />\n\
      <label for='utc'>UTC: </label>\n\
      <input id='utc' type='text' class='c' readonly />\n\
      <hr />\n\
      <h3>RTC</h3>\n\
      <label for='date'>Date: </label>\n\
      <input id='date' type='text' class='c' readonly />\n\
      <br />\n\
      <label for='time'>Time: </label>\n\
      <input id='time' type='text' class='c' readonly />\n\
      <br />\n\
      <label for='temp_rtc'>Temp: </label>\n\
      <input id='temp_rtc' type='text' class='c' readonly /> &deg;C\n\
      <hr />\n\
      <h3>Relais</h3>\n\
      <label for='relais'>State: </label>\n\
      <input id='relais' type='text' class='c' readonly />\n\
      <button class='tiny' type='button'\n\
              onclick='if (connection) connection.send(\"relais 1\")'>1\n\
      </button>\n\
      <button class='tiny' type='button'\n\
              onclick='if (connection) connection.send(\"relais 0\")'>0\n\
      </button>\n\
      <button class='tiny' type='button'\n\
              onclick='if (connection) connection.send(\"relais !\")'>!\n\
      </button>\n\
      <hr />\n\
      <h3>ADC</h3>\n\
      <label for='adc'>Value: </label>\n\
      <input id='adc' type='text' class='c' readonly />\n\
    </form>\n\
    <hr />\n\
    \n\
    <button type='button' onclick='if (connection) connection.send(\"reboot\")'>Reboot</button>\n\
    <br />\n\
    <br />\n\
  ");

  upload_form = PSTR("\n\
    <form method='POST' action='/upload' enctype='multipart/form-data'>\n\
      <input type='file' name='upload'>\n\
      <input type='submit' class='big' value='Upload'>\n\
    </form>\n\
    <br />\n\
  ");
#endif // DEBUG 

  conf_storage_header = PSTR(" \
    <fieldset>\n\
      <legend>Storage</legend>\n\
      <input name='storage_enabled'  type='radio'  value='0' %s />Disabled<br />\n\
      <input name='storage_enabled'  type='radio'  value='1' %s />Enabled<br />\n\
      <input name='storage_interval' type='hidden' value='%i'   />\n\
      <input name='storage_mask'     type='hidden' value='%i'   />\n\
  ");

  conf_storage_values = PSTR(" \
      <hr />\n\
      <table cellspacing='0'>\n\
      <tr>\n\
        <th>Phase:</th>\n\
        <th>Value:</th>\n\
      </tr>\n\
      <tr>\n\
        <td>\n\
          <input id='storage_mask_0'  type='checkbox' />A<br />\n\
          <input id='storage_mask_1'  type='checkbox' />B<br />\n\
          <input id='storage_mask_2'  type='checkbox' />C<br />\n\
          <input id='storage_mask_3'  type='checkbox' />Comb.\n\
        </td>\n\
        <td>\n\
          <input id='storage_mask_4'  type='checkbox' />VRMS<br />\n\
          <input id='storage_mask_5'  type='checkbox' />IRMS<br />\n\
          <input id='storage_mask_6'  type='checkbox' />WATT<br />\n\
          <input id='storage_mask_7'  type='checkbox' />VAR<br />\n\
          <input id='storage_mask_8'  type='checkbox' />VA<br />\n\
        </td>\n\
        <td>\n\
          <input id='storage_mask_9'  type='checkbox' />Freq.<br />\n\
          <input id='storage_mask_10' type='checkbox' />Temp.\n\
        </td>\n\
      </tr>\n\
      </table>\n\
      <hr />\n\
  ");

  conf_storage_interval = PSTR(" \
      <label>Save Interval:</label>\n\
      <select class='medium'\n\
              id='storage_interval_sel'\n\
              onchange='storage_select_changed()'>\n\
      </select>\n\
      minute(s)\n\
  ");

  conf_storage_footer = PSTR(" \
    </fieldset>\n\
    <br /><br />\n\
  ");

  html_page_common_js = PSTR(" \
    function websocket_handle_broadcast(d) {\n\
      if (d.value == 'reboot') {\n\
        spinner_show('Rebooting ...');\n\
        setTimeout(function() {\n\
          document.location.href = '/';\n\
        }, 35000);\n\
        polling = false;\n\
        return;\n\
      }\n\
      \n\
      if (d.value == 'update') {\n\
        spinner_show('Updating ...');\n\
        polling = false;\n\
        return;\n\
      }\n\
    }\n\
    \n\
    if (connection) {\n\
      connection.onclose = function() {\n\
        console.log('WebSocket: ', 'Remote side closed connection');\n\
        polling = false;\n\
      }\n\
      \n\
      connection.onerror = function(error) {\n\
        console.log('WebSocket: ', error);\n\
        polling = false;\n\
      }\n\
      \n\
      connection.onopen = function() {\n\
        if (typeof open_handler == 'function') {\n\
          open_handler();\n\
        }\n\
      }\n\
      \n\
      connection.onmessage = function(e) {\n\
        var d = JSON.parse(e.data);\n\
        \n\
        if (d.type == 'broadcast') {\n\
          websocket_handle_broadcast(d);\n\
          return;\n\
        }\n\
        \n\
        if (typeof message_handler == 'function') {\n\
          message_handler(d);\n\
        }\n\
      }\n\
    }\n\
    \n\
    function get_element(name) {\n\
      var elem = document.getElementsByName(name)[0];\n\
      if (elem) return (elem);\n\
      return (document.getElementById(name));\n\
    }\n\
    \n\
    function set_type(element, type) {\n\
      var elem = get_element(element);\n\
      if (elem) return (elem.attributes['type'] = type);\n\
    }\n\
    \n\
    function set_value(element, value) {\n\
      var elem = get_element(element);\n\
      if (elem) {\n\
        if (elem.nodeName == 'LABEL') {\n\
          return (elem.innerHTML = value);\n\
        } else if (elem.nodeName == 'SPAN') {\n\
          return (elem.textContent = value);\n\
        } else {\n\
          return (elem.value = value);\n\
        }\n\
      }\n\
    }\n\
    \n\
    function set_readonly(element, readonly) {\n\
      var elem = (get_element(element));\n\
      if (elem) return (elem.readOnly = readonly);\n\
    }\n\
    \n\
    function set_disabled(element, disabled) {\n\
      var elem = (get_element(element));\n\
      if (elem) return (elem.disabled = disabled);\n\
    }\n\
    \n\
    function set_visible(element, visible) {\n\
      var elem = (get_element(element));\n\
      if (elem) {\n\
        if (!visible) {\n\
          return (elem.style.display = 'none');\n\
        } else {\n\
          return (elem.style.display = 'initial');\n\
        }\n\
      }\n\
    }\n\
    \n\
    function set_color(element, color) {\n\
      var elem = (get_element(element));\n\
      if (elem) return (elem.style.color = color);\n\
    }\n\
    \n\
    function spinner_show(text) {\n\
      if (text != null) {\n\
        document.body.innerHTML = '\
          <center><h1 class=\"caption\">' + text + '</h1></center>\
          <div id=\"spinner\" class=\"spin\"></div>\
        ';\n\
      }\n\
      get_element('spinner').style.visibility = 'visible';\n\
    }\n\
    \n\
    function spinner_hide() {\n\
      get_element('spinner').style.visibility = 'hidden';\n\
    }\n\
  ");
 
  html_page_config_js = PSTR(" \
    function transport_select_changed() {\n\
      var select = get_element('transport_protocol');\n\
      if (select == null) return;\n\
      var idx = select.selectedIndex;\n\
      \n\
      if (idx == 2) { \n\
        set_value('transport_url_label', 'URL:');\n\
        set_disabled('transport_user',    true);\n\
        set_disabled('transport_pass',    true);\n\
        set_disabled('transport_node',    false);\n\
        set_disabled('transport_key',     false);\n\
      } else {\n\
        set_value('transport_url_label', 'Broker:');\n\
        set_disabled('transport_user',    false);\n\
        set_disabled('transport_pass',    false);\n\
        set_disabled('transport_node',    true);\n\
        set_disabled('transport_key',     true);\n\
      }\n\
    }\n\
    \n\
    function wifi_scan_network() {\n\
      spinner_show();\n\
      \n\
      request = new XMLHttpRequest();\n\
      request.open('GET', '/scan', true);\n\
      request.onreadystatechange = function(e) {\n\
        if ((request.readyState == 4) && (request.status == 200)) {\n\
          wifi = eval('(' + request.responseText + ')');\n\
          wifi_select_fill();\n\
        }\n\
        spinner_hide();\n\
      }\n\
      \n\
      request.send();\n\
    }\n\
    \n\
    function wifi_disable_elements() {\n\
      var select = get_element('wifi_ssid_sel');\n\
      if (select == null) return;\n\
      var idx = select.selectedIndex;\n\
      \n\
      if (idx == select.options.length - 1) { \n\
        set_readonly('wifi_ssid', false);\n\
        set_disabled('wifi_pass', false);\n\
      } else {\n\
        set_readonly('wifi_ssid', true);\n\
        set_disabled('wifi_pass', wifi[idx]['crypt']==7);\n\
      }\n\
      set_disabled('wifi_scan', false);\n\
    }\n\
    \n\
    function wifi_select_changed() {\n\
      var select = get_element('wifi_ssid_sel');\n\
      if (select == null) return;\n\
      var idx = select.selectedIndex;\n\
      var value = ssid_in_conf;\n\
      \n\
      if (idx != select.options.length - 1) {\n\
        value = wifi[idx]['ssid'];\n\
      }\n\
      set_value('wifi_ssid', value);\n\
      wifi_disable_elements();\n\
    }\n\
    \n\
    function wifi_select_fill() {\n\
      var select = get_element('wifi_ssid_sel');\n\
      if (select == null) return;\n\
      var found = false;\n\
      \n\
      select.options.length = 0;\n\
      for (var i=0; i<wifi.length; i++) {\n\
        var key = wifi[i]['ssid'];\n\
        var str = key;\n\
        str += ' (' + wifi[i]['rssi'] + '%) ';\n\
        str += (wifi[i]['crypt'] != 7) ? '*' : '';\n\
        select.options[select.options.length] = new Option(str, key);\n\
      }\n\
      select.options[select.options.length] = new Option('<hidden>', '---');\n\
      \n\
      for (var i=0; i<wifi.length; i++) {\n\
        if (wifi[i]['ssid'] == ssid_in_conf) {\n\
          select.value = ssid_in_conf;\n\
          wifi_select_changed();\n\
          found = true;\n\
          break;\n\
        }\n\
      }\n\
      if (!found) select.value = '---';\n\
    }\n\
    \n\
    function storage_select_changed() {\n\
      var select = get_element('storage_interval_sel');\n\
      if (select == null) return;\n\
      \n\
      set_value('storage_interval', select.value);\n\
    }\n\
    \n\
    function storage_select_fill() {\n\
      var select = get_element('storage_interval_sel');\n\
      if (select == null) return;\n\
      var input = get_element('storage_interval');\n\
      \n\
      select.options.length = 0;\n\
      for (var i=0; i<=60; i++) {\n\
        if (60%i == 0) {\n\
          select.options[select.options.length] = new Option(i, i);\n\
        }\n\
      }\n\
      \n\
      select.value = input.value;\n\
    }\n\
    \n\
    function storage_values_check() {\n\
      var input = get_element('storage_mask');\n\
      if (input == null) return;\n\
      var mask = parseInt(input.value);\n\
      \n\
      for (var i=0; i<11; i++) {\n\
        get_element('storage_mask_' + i).checked = (mask & (1<<i));\n\
      }\n\
    }\n\
    \n\
    function wifi_elements() {\n\
      return new Array(\n\
        get_element('wifi_ssid_sel'),\n\
        get_element('wifi_ssid'),\n\
        get_element('wifi_pass'),\n\
        get_element('wifi_scan'),\n\
        get_element('wifi_power'),\n\
        get_element('wifi_watchdog')\n\
      );\n\
    }\n\
    function ip_elements() {\n\
      return new Array(\n\
        get_element('ip_addr'),\n\
        get_element('ip_netmask'),\n\
        get_element('ip_gateway'),\n\
        get_element('ip_dns1'),\n\
        get_element('ip_dns2')\n\
      );\n\
    }\n\
    function ap_elements() {\n\
      return new Array(\n\
        get_element('ap_addr')\n\
      );\n\
    }\n\
    function ntp_elements() {\n\
      return new Array(\n\
        get_element('ntp_server'),\n\
        get_element('ntp_interval')\n\
      );\n\
    }\n\
    function transport_elements() {\n\
      return new Array(\n\
        get_element('transport_url'),\n\
        get_element('transport_user'),\n\
        get_element('transport_pass'),\n\
        get_element('transport_interval')\n\
      );\n\
    }\n\
    function update_elements() {\n\
      return new Array(\n\
        get_element('update_url'),\n\
        get_element('update_interval')\n\
      );\n\
    }\n\
    function storage_elements() {\n\
      var ret = new Array(get_element('storage_interval'));\n\
      for (var i=0; i<11; i++) {\n\
        ret.push(get_element('storage_mask_' + i));\n\
      }\n\
      return (ret);\n\
    }\n\
    function set_elements_inactive(elements, disabled) {\n\
      elements.forEach(function(elem) {\n\
        elem.disabled = disabled;\n\
      });\n\
    }\n\
    document.onclick = function(e) {\n\
      var elem = e ? e.target : window.event.srcElement;\n\
      var disabled = (elem.value === '0') ? true : false;\n\
      var elements = [];\n\
      \n\
      if (elem.name === 'wifi_enabled') {\n\
        set_elements_inactive(wifi_elements(), disabled);\n\
        if (!disabled) wifi_disable_elements();\n\
        return;\n\
      }\n\
      if (elem.name ===         'ip_static') elements =        ip_elements();\n\
      if (elem.name ===        'ap_enabled') elements =        ap_elements();\n\
      if (elem.name ===       'ntp_enabled') elements =       ntp_elements();\n\
      if (elem.name === 'transport_enabled') elements = transport_elements();\n\
      if (elem.name ===    'update_enabled') elements =    update_elements();\n\
      if (elem.name ===   'storage_enabled') elements =   storage_elements();\n\
      \n\
      set_elements_inactive(elements, disabled);\n\
      \n\
      if (elem.id.substring(0, 13) == 'storage_mask_') {\n\
        var mask = get_element('storage_mask');\n\
        var bit = parseInt(elem.id.substring(13));\n\
        var val = parseInt(mask.value);\n\
        if (elem.checked) { val |= (1<<bit); } else { val &= ~(1<<bit); }\n\
        mask.value = val;\n\
      }\n\
    }\n\
    \n\
    function set_inactive(element, elements) {\n\
      var e = get_element(element);\n\
      if (e && e.checked) {\n\
        set_elements_inactive(elements, true);\n\
      }\n\
    }\n\
    \n\
    function conf_set_browser_time() {\n\
      var d = new Date();\n\
      \n\
      var hr  = d.getUTCHours();\n\
      var min = d.getUTCMinutes();\n\
      var sec = d.getUTCSeconds();\n\
      \n\
      if (hr  < 10) { hr  = '0' + hr;  }\n\
      if (min < 10) { min = '0' + min; }\n\
      if (sec < 10) { sec = '0' + sec; }\n\
      \n\
      set_value('browser', hr + ':' + min + ':' + sec);\n\
    }\n\
    \n\
    function time_timer() {\n\
      if (polling) {\n\
        connection.send('time');\n\
        conf_set_browser_time();\n\
        setTimeout(time_timer, 233);\n\
      }\n\
    }\n\
    \n\
    function open_handler() {\n\
      setTimeout(time_timer, 10);\n\
    }\n\
    \n\
    function message_handler(d) {\n\
      if (d.type == 'time') {\n\
        set_value('date', d.date);\n\
        set_value('time', d.time);\n\
      }\n\
    }\n\
    \n\
    function conf_rtc_browser_sync() {\n\
      if (connection) connection.send('rtc ' + Date.now());\n\
    }\n\
    \n\
    window.onload = function(e) {\n\
      set_inactive(     'wifi_enabled',      wifi_elements());\n\
      set_inactive(       'ip_static',         ip_elements());\n\
      set_inactive(       'ap_enabled',        ap_elements());\n\
      set_inactive(      'ntp_enabled',       ntp_elements());\n\
      set_inactive('transport_enabled', transport_elements());\n\
      set_inactive(   'update_enabled',    update_elements());\n\
      set_inactive(  'storage_enabled',   storage_elements());\n\
      \n\
      transport_select_changed();\n\
      wifi_select_fill();\n\
      storage_select_fill();\n\
      storage_values_check();\n\
    }\n\
  ");

  html_page_sys_js = PSTR(" \
    function load_timer() {\n\
      if (polling) {\n\
        connection.send('load');\n\
        setTimeout(load_timer, 1009);\n\
      }\n\
    }\n\
    function log_timer() {\n\
      if (polling) {\n\
        connection.send('log');\n\
        setTimeout(log_timer, 701);\n\
      }\n\
    }\n\
    function state_timer() {\n\
      if (polling) {\n\
        connection.send('state');\n\
        setTimeout(state_timer, 777);\n\
      }\n\
    }\n\
    function time_timer() {\n\
      if (polling) {\n\
        connection.send('time');\n\
        setTimeout(time_timer, 293);\n\
      }\n\
    }\n\
    function adc_timer() {\n\
      if (polling) {\n\
        connection.send('adc');\n\
        setTimeout(adc_timer, 2999);\n\
      }\n\
    }\n\
    function relais_timer() {\n\
      if (polling) {\n\
        connection.send('relais');\n\
        setTimeout(relais_timer, 441);\n\
      }\n\
    }\n\
    function temp_timer() {\n\
      if (polling) {\n\
        connection.send('temp');\n\
        setTimeout(temp_timer, 2237);\n\
      }\n\
    }\n\
    \n\
    function open_handler() {\n\
      setTimeout(  load_timer, 10);\n\
      setTimeout(  time_timer, 20);\n\
      setTimeout(   adc_timer, 30);\n\
      setTimeout(   log_timer, 40);\n\
      setTimeout(relais_timer, 50);\n\
      setTimeout(  temp_timer, 60);\n\
      setTimeout( state_timer, 70);\n\
    }\n\
    \n\
    function message_handler(d) {\n\
      if (d.type == 'load') {\n\
        sys_draw_load(d);\n\
      }\n\
      if (d.type == 'log') {\n\
        get_element('syslog').innerHTML = d.text;\n\
      }\n\
      if (d.type == 'module') {\n\
        sys_update_modules(d);\n\
      }\n\
      if (d.type == 'time') {\n\
        set_value('uptime', d.uptime);\n\
        set_value('date',   d.date);\n\
        set_value('time',   d.time);\n\
        set_value('utc',    d.utc);\n\
      }\n\
      if (d.type == 'temp') {\n\
        set_value('temp_rtc', d.rtc);\n\
      }\n\
      if (d.type == 'adc') {\n\
        set_value('adc', d.value);\n\
      }\n\
      if (d.type == 'relais') {\n\
        set_value('relais', d.value);\n\
      }\n\
    }\n\
    \n\
    function sys_update_modules(data) {\n\
      var state = data.state;\n\
      for (i=0; i<state.length; i++) {\n\
        set_value('module_' + i + '_state', state[i]);\n\
        set_color('module_' + i + '_state', (state[i]=='ACTIVE')?'#1b1':'#b11');\n\
      }\n\
    }\n\
    \n\
    function sys_draw_load(data) {\n\
      var canvas = get_element('canvas_load')\n\
      var ctx = canvas.getContext('2d');\n\
      \n\
      drawLoadAxes(ctx);\n\
      drawLoadGraph(ctx, 'cpu', data);\n\
      drawLoadGraph(ctx, 'mem', data);\n\
      drawLoadGraph(ctx, 'net', data);\n\
      \n\
      var cpu = data.cpu.values[data.cpu.values.length-1];\n\
      var mem = data.mem.values[data.mem.values.length-1];\n\
      var net = data.net.values[data.net.values.length-1];\n\
      \n\
      set_value('load_cpu_perc', cpu + '%');\n\
      set_value('load_mem_perc', mem + '%');\n\
      set_value('load_net_perc', net + '%');\n\
      \n\
      set_value('load_cpu_loops', '(' + data.cpu.loops + ' loops/s)');\n\
      set_value('load_mem_free',  '(' + data.mem.free  + ' bytes free)');\n\
      set_value('load_net_xfer',  '(' + data.net.xfer  + ' bytes/s)');\n\
    }\n\
    \n\
    function drawLoadGraph(ctx, name, data) {\n\
      if (name == 'cpu') {\n\
        var color = 'rgb(192, 16, 16)'; // red\n\
        var val = data.cpu.values;\n\
      } else if (name == 'mem') {\n\
        var color = 'rgb(16, 16, 192)'; // blue\n\
        var val = data.mem.values;\n\
      } else if (name == 'net') {\n\
        var color = 'rgb(16, 192, 16)'; // green\n\
        var val = data.net.values;\n\
      }\n\
      \n\
      var delta  = ctx.canvas.width / (val.length-1);\n\
      var height = ctx.canvas.height;\n\
      var scale  = height / 100;\n\
      \n\
      ctx.beginPath();\n\
      ctx.lineWidth = 2;\n\
      ctx.strokeStyle = color;\n\
      \n\
      ctx.moveTo(0, height - (scale * val[0]));\n\
      for (i=1; i<val.length-1; i++) {\n\
        var x  = delta * i;\n\
        var y  = height - (scale * val[i]);\n\
        var xc = x + delta / 2;\n\
        var yc = (y + height - (scale * val[i+1])) / 2;\n\
        ctx.quadraticCurveTo(x, y, xc, yc);\n\
      }\n\
      ctx.quadraticCurveTo(delta * i,     height - (scale * val[i]),\n\
                           delta * (i+1), height - (scale * val[i+1]));\n\
      ctx.stroke();\n\
    }\n\
    \n\
    function drawLoadAxes(ctx) {\n\
      var x0 = 0;\n\
      var y0 = ctx.canvas.height;\n\
      var w = ctx.canvas.width;\n\
      var h = ctx.canvas.height;\n\
      \n\
      ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);\n\
      ctx.beginPath();\n\
      ctx.strokeStyle = 'rgb(128, 128, 128)';\n\
      ctx.moveTo(0, y0); ctx.lineTo(w, y0); // X axis\n\
      ctx.moveTo(x0, 0); ctx.lineTo(x0, h); // Y axis\n\
      ctx.stroke();\n\
    }\n\
  ");

  html_page_reset_css = PSTR(" \
    html, body, div, span, applet, object, iframe,\n\
    h1, h2, h3, h4, h5, h6, p, blockquote, pre,\n\
    a, abbr, acronym, address, big, cite, code,\n\
    del, dfn, em, font, img, ins, kbd, q, s, samp,\n\
    small, strike, strong, sub, sup, tt, var,\n\
    dl, dt, dd, ol, ul, li,\n\
    fieldset, form, label, legend,\n\
    table, caption, tbody, tfoot, thead, tr, th, td {\n\
    	margin:0;\n\
    	padding:0;\n\
    	border:0;\n\
    	outline:0;\n\
    	font-weight:inherit;\n\
    	font-style:inherit;\n\
    	font-size:100%;\n\
    	font-family:inherit;\n\
    	vertical-align:baseline;\n\
    }\n\
    /* remember to define focus styles! */\n\
    :focus {\n\
    	outline:0;\n\
    }\n\
    body {\n\
    	color:black;\n\
    	background:white;\n\
    }\n\
    ol, ul {\n\
    	list-style:none;\n\
    }\n\
    /* tables still need 'cellspacing=0' in the markup */\n\
    table {\n\
    	border-collapse:separate;\n\
    	border-spacing:2px;\n\
    }\n\
    caption, th, td {\n\
    	text-align:left;\n\
    	font-weight:normal;\n\
    }\n\
    blockquote:before, blockquote:after,\n\
    q:before, q:after {\n\
    	content: "";\n\
    }\n\
    blockquote, q {\n\
    	quotes: "" "";\n\
    }\n\
  ");

  html_page_style_css = PSTR(" \
    html { overflow-y:scroll; }\n\
    body {\n\
      background-color:#f9f9f9; color:#555555;\n\
      font-family:Helvetica Neue, Helvetica, Arial, sans-serif;\n\
    	line-height:1em; font-weight:400; font-size:85%;\n\
    }\n\
    \n\
    .fixed { font-family:'Courier New', Courier, Monospace; }\n\
    \n\
    a:        { cursor:pointer; }\n\
    a:link    { color:#109010; text-decoration:none; font-weight:700; }\n\
    a:visited { color:#109010; text-decoration:none; font-weight:700; }\n\
    a:active  { color:#109010; text-decoration:none; font-weight:700; }\n\
    a:hover   { color:#2eca00; text-decoration:none; font-weight:700; }\n\
    \n\
    th { text-align:center }\n\
    hr { margin:8px 0; }\n\
    h3 {\n\
      margin-top:20px; margin-bottom:10px;\n\
      font-weight:500; font-size:1.4em;\n\
    }\n\
    \n\
    .icon {\n\
      width:1em; height:1em;\n\
    }\n\
    .config, .login, #content {\n\
      position:absolute;\n\
      width:26em; left:50%; margin-left:-13em;\n\
    }\n\
    header {\n\
      position:fixed;\n\
      background-color:#cccccc;\n\
      z-index:9980;\n\
      padding:7px;\n\
      width:100%;\n\
    }\n\
    #content { padding-top:2.5em; }\n\
    .config th {\n\
      text-align:left;\n\
      padding:0 2px 5px;\n\
      line-height:20px;\n\
    }\n\
    .login {\n\
      height:15em;\n\
      top:8em;\n\
    }\n\
    \n\
    fieldset {\n\
      display:inline-block;\n\
      padding-right:2em; padding-left:2em;\n\
      padding-top:1em; padding-bottom:1em;\n\
      background-color:#efefef; border:solid 1px #dddddd;\n\
      border-radius:0.3em; width:22em;\n\
    }\n\
    \n\
    legend {\n\
      font-size:1.15em; font-weight:500; background-color:#efefef;\n\
      border-width:1px; border-style:solid; border-color:#dddddd;\n\
      border-radius:0.3em; padding:0.3em 0.5em; \n\
    }\n\
    \n\
    label {\n\
      display:inline-block; width:8em;\n\
      text-align:right; padding-right:0.4em;\n\
    }\n\
    \n\
    xmp { white-space:pre-wrap; word-wrap:break-word; }\n\
    \n\
    input, select, button {\n\
      width:10em; margin-top:0; margin-bottom:4px;\n\
      box-sizing:border-box;\n\
      color:#555555;\n\
    }\n\
    input[type='checkbox'],\n\
    input[type='radio'] {\n\
      vertical-align:middle;\n\
      width:1.2em; height:1.2em;\n\
      margin-left:9em; margin-top:1px;\n\
    }\n\
    input[type='checkbox'] { margin-left:2em; }\n\
    input[type='number'] { width:4em; -moz-appearance:textfield; }\n\
    input[type='file'] { width:20em; }\n\
    \n\
    select, button,\n\
    input[type='submit'],\n\
    input[type='reset'], \n\
    input[type='file'] { \n\
      cursor:pointer;\n\
    }\n\
    input.c        { width:8.5em; text-align:center; }\n\
    input.r        { width:8.5em; text-align:right; }\n\
    input.calib    { width:5em;   text-align:right; }\n\
    input.meter    { width:6em;   text-align:right; }\n\
    input.datetime { width:11em;  text-align:center; }\n\
    label.meter {\n\
      display:inline-block; width:5em;\n\
      text-align:right; padding-right:0.4em;\n\
    }\n\
    .tiny   { width:2em; }\n\
    .small  { width:2em; }\n\
    .medium { width:4.5em; }\n\
    .big    { width:6em; }\n\
    \n\
    .wave { width:25em; height:8em; }\n\
    .load { width:25em; height:5em; }\n\
    \n\
    .caption {\n\
      position:absolute;\n\
      height:2.5em; width:14em;\n\
      top:50%; left:50%;\n\
      margin-left:-7em; margin-top:-2em;\n\
      font-weight:800; font-size:2em;\n\
    }\n\
    .spin {\n\
      z-index:9990;\n\
      position:fixed;\n\
      visibility:hidden;\n\
      top:50%; left:50%;\n\
      width:2em; height:2em;\n\
      margin-left:-1em; margin-top:-1em;\n\
      background:transparent;\n\
      border:8px solid #ffcfcf;\n\
      border-top-color:#ff0000;\n\
      border-radius:100%;\n\
      -webkit-animation:spin linear .7s infinite;\n\
      animation:spin linear .7s infinite;\n\
    }\n\
    \n\
    #syslog {\n\
      min-width:36em; max-width:36em; min-height:12em;\n\
      background-color:#050505; padding:0.4em; margin-bottom:0.5em;\n\
      font-size:0.7em;\n\
    }\n\
    @-webkit-keyframes spin {\n\
      100% { -webkit-transform:rotate(360deg); }\n\
    }\n\
    @keyframes spin {\n\
      100% { transform:rotate(360deg); }\n\
    }\n\
    @-moz-document url-prefix(http://) {\n\
      .tiny,\n\
      button::-moz-focus-inner,\n\
      input[type='button']::-moz-focus-inner,\n\
      input[type='submit']::-moz-focus-inner,\n\
      input[type='reset']::-moz-focus-inner {\n\
        padding:0 !important;\n\
        border:0 none !important;\n\
      }\n\
      [readonly] {\n\
        cursor:default;\n\
        -moz-user-select:none;\n\
        user-select:none;\n\
      }\n\
    }\n\
  ");

#ifndef DEBUG
  page_menu_sta = PSTR(" \
    <header>\n\
      <center>\n\
        <a href='/'      >[HOME]   </a>&thinsp;\n\
        <a href='/files' >[FILES]  </a>&thinsp;\n\
        <a href='/setup' >[SETUP]  </a>&thinsp;\n\
        <a href='/login?LOGOUT=YES'>[LOGOUT]</a>\n\
      </center>\n\
    </header>\n\
  ");

  page_menu_ap = PSTR(" \
    <header>\n\
      <center>\n\
        <a href='/'      >[HOME]   </a>&thinsp;\n\
        <a href='/files' >[FILES]  </a>&thinsp;\n\
        <a href='/setup' >[SETUP]  </a>&thinsp;\n\
        <a href='/login?LOGOUT=YES'>[LOGOUT]</a>\n\
      </center>\n\
    </header>\n\
  ");
#else
  page_menu_sta = PSTR(" \
    <header style='background-color:#fffe8f;'>\n\
      <center>\n\
        <a href='/'      >[HOME]</a>\n\
        <a href='/files' >[FILES]</a>\n\
        <a href='/setup' >[SETUP]</a>\n\
        <a href='/conf'  >[CONF]</a>\n\
        <a href='/info'  >[INFO]</a>\n\
        <a href='/sys'   >[SYS]</a>\n\
      </center>\n\
    </header>\n\
  ");

  page_menu_ap = PSTR(" \
    <header style='background-color:#fffe8f;'>\n\
      <center>\n\
        <a href='/'      >[HOME]</a>\n\
        <a href='/files' >[FILES]</a>\n\
        <a href='/setup' >[SETUP]</a>\n\
        <a href='/conf'  >[CONF]</a>\n\
        <a href='/info'  >[INFO]</a>\n\
        <a href='/sys'   >[SYS]</a>\n\
      </center>\n\
    </header>\n\
  ");
#endif

  page_header = PSTR(" \
    <!DOCTYPE html>\n\
    <html>\n\
    <head>\n\
    <title>%s</title>\n\
    <link rel='icon' type='image/x-icon' href='/fav.png' />\n\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\n\
    <link rel='stylesheet' type='text/css' href='/reset.css'>\n\
    <link rel='stylesheet' type='text/css' href='/style.css'>\n\
    </head>\n\
  ");

  page_footer = PSTR(" \
    <div id='spinner' class='spin'></div>\n\
    </div></body></html>\n\
  ");

/*
  page_meter = reinterpret_cast<const __FlashStringHelper *>(
    __extension__({
      static const char __c[] PROGMEM = (
        #include "meter.html"
      ); &__c[0];
    })
  );
*/

  return (true);
}
