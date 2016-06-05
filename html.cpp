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

// this file is generated during make
#include "buildinfo.h"

#include "system.h"
#include "config.h"
#include "clock.h"
#include "log.h"
#include "net.h"

#include "html.h"

static const char *html_root_fmt;
static const char *html_info_fmt;
static const char *html_conf_fmt;
static const char *html_setup_fmt;
static const char *html_login_fmt;
static const char *html_sys_fmt;
static const char *html_redirect_fmt;

const __FlashStringHelper *html_page_header;
const __FlashStringHelper *html_page_refresh;
const __FlashStringHelper *html_page_style;
const __FlashStringHelper *html_page_script;
const __FlashStringHelper *html_page_body;
const __FlashStringHelper *html_page_menu;
const __FlashStringHelper *html_page_footer;

void html_insert_root_content(String &html) {
  char *pagebuf = (char *)malloc(1024);
  char time[16], uptime[24];

  if (!pagebuf) {
    log_print(F("HTML: failed to allocate [HOME] page buffer\n")); return;
  }

  snprintf_P(pagebuf, 1024, html_root_fmt,
    FIRMWARE,
    system_uptime(uptime),
    system_time(time),
    analogRead(17)
  );

  html += pagebuf;
  free(pagebuf);
}

void html_insert_info_content(String &html) {
#ifndef RELEASE
  char *pagebuf = (char *)malloc(3*1024);
  const char *flash_mode = "UNKNOWN";

  if (!pagebuf) {
    log_print(F("HTML: failed to allocate [INFO] page buffer\n")); return;
  }

  switch (ESP.getFlashChipMode()) {
    case FM_QIO:  flash_mode = "QIO";  break;
    case FM_QOUT: flash_mode = "QOUT"; break;
    case FM_DIO:  flash_mode = "DIO";  break;
    case FM_DOUT: flash_mode = "DOUT"; break;
  }

  snprintf_P(pagebuf, 3*1024, html_info_fmt,
    client_id,
    FIRMWARE,
    ESP.getSdkVersion(),
    ESP.getBootVersion(),

    _BuildInfo.date,
    _BuildInfo.time,
    _BuildInfo.src_version,
    _BuildInfo.env_version,

    ESP.getSketchSize(),
    ESP.getFreeSketchSpace(),
    ESP.getFreeHeap(),
    system_free_stack(),
    system_stack_corrupt() ? "CORRUPTED" : "OK",
    ESP.getCpuFreqMHz(),
    ESP.getBootMode(),

    net_ip().c_str(),
    net_gateway().c_str(),
    net_dns().c_str(),
    net_netmask().c_str(),
    net_mac().c_str(),
    net_rssi(),

    net_ap_ip().c_str(),
    net_ap_gateway().c_str(),
    net_ap_netmask().c_str(),
    net_ap_mac().c_str(),

    ESP.getFlashChipId(),
    ESP.getFlashChipRealSize(),
    ESP.getFlashChipSize(),
    ESP.getFlashChipSpeed(),
    flash_mode
  );

  html += pagebuf;
  free(pagebuf);
#endif // RELEASE
}

void html_insert_conf_content(String &html) {
  char *pagebuf = (char *)malloc(5*1024);

  if (!pagebuf) {
    log_print(F("HTML: failed to allocate [CONF] page buffer\n")); return;
  }

  IPAddress ip_addr(config->ip_addr);
  IPAddress ip_netmask(config->ip_netmask);
  IPAddress ip_gateway(config->ip_gateway);
  IPAddress ip_dns1(config->ip_dns1);
  IPAddress ip_dns2(config->ip_dns2);
  IPAddress ap_addr(config->ap_addr);

  snprintf_P(pagebuf, 5*1024, html_conf_fmt,
    config->user_name,
    config->wifi_ssid,

    config->ip_static ? "" : "checked",
    config->ip_static ? "checked" : "",
	 ip_addr.toString().c_str(),
	 ip_netmask.toString().c_str(),
	 ip_gateway.toString().c_str(),
	 ip_dns1.toString().c_str(),
	 ip_dns2.toString().c_str(),

    config->ap_enabled ? "" : "checked",
    config->ap_enabled ? "checked" : "",
    ap_addr.toString().c_str(),

    config->mdns_enabled ? "" : "checked",
    config->mdns_enabled ? "checked" : "",
    config->mdns_name,

    config->ntp_enabled ? "" : "checked",
    config->ntp_enabled ? "checked" : "",
    config->ntp_server,
    config->ntp_interval,

    config->mqtt_enabled ? "" : "checked",
    config->mqtt_enabled ? "checked" : "",
    config->mqtt_server,
    config->mqtt_user,
    config->mqtt_interval
  );

  html += pagebuf;
  free(pagebuf);
}

void html_insert_setup_content(String &html) {
  char *pagebuf = (char *)malloc(3*1024);

  if (!pagebuf) {
    log_print(F("HTML: failed to allocate [SETUP] page buffer\n")); return;
  }

  IPAddress ip_addr(config->ip_addr);
  IPAddress ip_netmask(config->ip_netmask);
  IPAddress ip_gateway(config->ip_gateway);
  IPAddress ip_dns1(config->ip_dns1);
  IPAddress ip_dns2(config->ip_dns2);

  snprintf_P(pagebuf, 3*1024, html_setup_fmt,
    config->user_name,
    config->ip_static ? "" : "checked",
    config->ip_static ? "checked" : "",
	 ip_addr.toString().c_str(),
	 ip_netmask.toString().c_str(),
	 ip_gateway.toString().c_str(),
	 ip_dns1.toString().c_str(),
	 ip_dns2.toString().c_str()
  );

  html += pagebuf;
  free(pagebuf);
}

void html_insert_login_content(const String &msg, String &html) {
  char *pagebuf = (char *)malloc(2*1024);

  if (!pagebuf) {
    log_print(F("HTML: failed to allocate [LOGIN] page buffer\n")); return;
  }

  snprintf_P(pagebuf, 2*1024, html_login_fmt,
    FIRMWARE,
    client_id,
    msg.c_str()
  );

  html += pagebuf;
  free(pagebuf);
}
 
void html_insert_sys_content(String &html) {
#ifndef RELEASE
  char *pagebuf = (char *)malloc(4*1024);

  if (!pagebuf) {
    log_print(F("HTML: failed to allocate [SYS] page buffer\n")); return;
  }

  snprintf_P(pagebuf, 4*1024, html_sys_fmt, net_ip().c_str());

  html += pagebuf;
  free(pagebuf);
#endif
}

void html_insert_page_header(String &html) {
  html += html_page_header;
}

void html_insert_page_refresh(String &html) {
  html += html_page_refresh;
}

void html_insert_page_redirect(String &html, int delay) {
  char pagebuf[128];

  snprintf_P(pagebuf, sizeof (pagebuf), html_redirect_fmt, delay);

  html += pagebuf;
}

void html_insert_page_body(String &html) {
  html += html_page_body;
}

void html_insert_page_menu(String &html) {
  html += html_page_menu;
}

void html_insert_page_footer(String &html) {
  html += html_page_footer;
}

bool html_init(void) {

  html_redirect_fmt = PSTR(" \
    <meta http-equiv='refresh' content='%i;URL=/'/>\n \
  ");

  html_login_fmt = PSTR(" \
    <div class='login'>\n \
      <fieldset>\n \
        <center>\n \
        <h3>GENESYS %s&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%s</h3>\n \
        </center>\n \
        <hr />\n \
        <form action='/login' method='POST'>\n \
          <p>\n \
          <label for='a'>Username: </label>\n \
          <input id='a' type='text' name='USER' placeholder='username' autofocus>\n \
          </p>\n \
          <p>\n \
          <label for='b'>Password: </label>\n \
          <input id='b' type='password' name='PASS' placeholder='password'>\n \
          </p>\n \
          <br />\n \
          <center>\n \
          <input type='submit' name='SUBMIT' value='Login'>\n \
          </center>\n \
          <br />\n \
        </form>\n \
        <hr />\n \
        <center>%s</center>\n \
      </fieldset>\n \
    </div>\n \
  ");

  html_root_fmt = PSTR(" \
    <p class='fixed'>\n \
      <br /><br /><center><H3>GENESYS %s</H3></center>\n \
    </p>\n \
    <form class='table'>\n \
      <p>\n \
        <label for='uptime'>Uptime: </label>\n \
        <input id='uptime' type='text' disabled value='%s'>\n \
      </p>\n \
      <p>\n \
        <label for='utc'>UTC: </label>\n \
        <input id='utc' type='text' disabled value='%s'>\n \
      </p>\n \
      <p>\n \
        <label for='adc'>ADC: </label>\n \
        <input id='adc' type='text' disabled value='%i'>\n \
      </p>\n \
    </form>\n \
    <br />\n \
  ");

  html_conf_fmt = PSTR(" \
    <script>\n \
      window.onload = function(e) {\n \
        if (document.getElementsByName('ip_static')[0].checked) {\n \
          set_elements_inactive(ip_elements(), true);\n \
        }\n \
        if (document.getElementsByName('ap_enabled')[0].checked) {\n \
          set_elements_inactive(ap_elements(), true);\n \
        }\n \
        if (document.getElementsByName('mdns_enabled')[0].checked) {\n \
          set_elements_inactive(mdns_elements(), true);\n \
        }\n \
        if (document.getElementsByName('ntp_enabled')[0].checked) {\n \
          set_elements_inactive(ntp_elements(), true);\n \
        }\n \
        if (document.getElementsByName('mqtt_enabled')[0].checked) {\n \
          set_elements_inactive(mqtt_elements(), true);\n \
        }\n \
      }\n \
    </script> \n \
    <br />\n \
    <div class='config'>\n \
    <form id='config' action='#' method='POST' enctype='multipart/form-data'>\n \
      <fieldset>\n \
        <legend>User</legend>\n \
        <p><label>Username:</label>\n \
        <input name='user_name' type='text' value='%s' /></p>\n \
        <p><label>Password:</label>\n \
        <input name='user_pass' type='password' /></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>WiFi</legend>\n \
        <p><label>SSID:</label>\n \
        <input name='wifi_ssid' type='text' value='%s' /></p>\n \
        <p><label>Password:</label>\n \
        <input name='wifi_pass' type='password' /></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>IP</legend>\n \
        <input class='radio' name='ip_static' type='radio' value='0' %s />\n \
        DHCP<br />\n \
        <input class='radio' name='ip_static' type='radio' value='1' %s />\n \
        Static<br />\n \
        <hr />\n \
        <p><label>Address:</label>\n \
        <input name='ip_addr' type='text' value='%s'/></p>\n \
        <p><label>Netmask:</label>\n \
        <input name='ip_netmask' type='text' value='%s'/></p>\n \
        <p><label>Gateway:</label>\n \
        <input name='ip_gateway' type='text' value='%s'/></p>\n \
        <p><label>DNS1:</label>\n \
        <input name='ip_dns1' type='text' value='%s'/></p>\n \
        <p><label>DNS2:</label>\n \
        <input name='ip_dns2' type='text' value='%s'/></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>AP</legend>\n \
        <input name='ap_enabled' type='radio' value='0' %s />Disabled<br />\n \
        <input name='ap_enabled' type='radio' value='1' %s />Enabled<br />\n \
        <hr />\n \
        <p><label>Address:</label>\n \
        <input name='ap_addr' type='text' value='%s'/></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>mDNS</legend>\n \
        <input name='mdns_enabled' type='radio' value='0' %s />Disabled<br />\n \
        <input name='mdns_enabled' type='radio' value='1' %s />Enabled<br />\n \
        <hr />\n \
        <p><label>Name:</label>\n \
        <input name='mdns_name' type='text' value='%s' class='r'/>.local</p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>NTP</legend>\n \
        <input name='ntp_enabled' type='radio' value='0' %s />Disabled<br />\n \
        <input name='ntp_enabled' type='radio' value='1' %s />Enabled<br />\n \
        <hr />\n \
        <p><label>Server:</label>\n \
        <input name='ntp_server' type='text' value='%s'/></p>\n \
        <p><label>Sync Interval:</label>\n \
        <input name='ntp_interval' type='number' value='%i' min='1' />\n \
        minutes</p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>MQTT</legend>\n \
        <input name='mqtt_enabled' type='radio' value='0' %s />Disabled<br />\n \
        <input name='mqtt_enabled' type='radio' value='1' %s />Enabled<br />\n \
        <hr />\n \
        <p><label>Server:</label>\n \
        <input name='mqtt_server' type='text' value='%s' /></p>\n \
        <p><label>Username:</label>\n \
        <input name='mqtt_user' type='text' value='%s' /></p>\n \
        <p><label>Password:</label>\n \
        <input name='mqtt_pass' type='password' /></p>\n \
        <p><label>Publish Interval:</label>\n \
        <input name='mqtt_interval' type='number' value='%i' min='1' />\n \
        seconds</p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <center>\n \
      <input type='submit' value='Save' />\n \
      </center>\n \
      <br /><br />\n \
    </form>\n \
    </div>\n \
  ");

#ifndef RELEASE
  html_info_fmt = PSTR(" \
    <p class='fixed'>\n \
      Client ID:          %s      <br />\n \
      Firmware Version:   %s      <br />\n \
      ESP-SDK Version:    %s      <br />\n \
      Bootloader Version: %i      <br />\n \
      <br />\n \
      Build Date:         %s      <br />\n \
      Build Time:         %s      <br />\n \
      Build SRC:          %s      <br />\n \
      Build ENV:          %s      <br />\n \
      <br />\n \
      Sketch Size:        %u bytes<br />\n \
      Free Sketch Space:  %u bytes<br />\n \
      Free Heap:          %u bytes<br />\n \
      Free Stack:         %u bytes<br />\n \
      Stack Guard:        %s      <br />\n \
      CPU Clock:          %u MHz  <br />\n \
      Bootmode:           %i      <br />\n \
      <br />\n \
      IP Address:         %s      <br />\n \
      Default GW:         %s      <br />\n \
      DNS:                %s      <br />\n \
      Net Mask:           %s      <br />\n \
      MAC Address:        %s      <br />\n \
      WiFi Signal:        %i%     <br />\n \
      <br />\n \
      AP IP:              %s      <br />\n \
      AP GW:              %s      <br />\n \
      AP Net Mask:        %s      <br />\n \
      AP MAC Address:     %s      <br />\n \
      <br />\n \
      Flash Chip ID:      %08X    <br />\n \
      Flash Real Size:    %u      <br />\n \
      Flash Size:         %u      <br />\n \
      Flash Speed:        %u      <br />\n \
      Flash Mode:         %s      <br />\n \
    </p>\n \
  ");

  html_sys_fmt = PSTR(" \
    <script>\n \
      var connection = new WebSocket('ws://%s:80/ws');\n \
      var reload = true;\n \
      \n \
      function load_timer() {\n \
        if (reload) {\n \
          connection.send('load');\n \
          setTimeout(load_timer, 1000);\n \
        }\n \
      }\n \
      function log_timer() {\n \
        if (reload) {\n \
          connection.send('log');\n \
          setTimeout(log_timer, 500);\n \
        }\n \
      }\n \
      function time_timer() {\n \
        if (reload) {\n \
          connection.send('time');\n \
          setTimeout(time_timer, 100);\n \
        }\n \
      }\n \
      function adc_timer() {\n \
        if (reload) {\n \
          connection.send('adc');\n \
          setTimeout(adc_timer, 3000);\n \
        }\n \
      }\n \
      \n \
      connection.onopen = function() {\n \
        setTimeout(load_timer, 10);\n \
        setTimeout(time_timer, 20);\n \
        setTimeout(adc_timer, 30);\n \
        setTimeout(log_timer, 40);\n \
      };\n \
      \n \
      connection.onerror = function(error) {\n \
        console.log('WebSocket Error ', error);\n \
        reload = false;\n \
      };\n \
      \n \
      connection.onmessage = function(e) {\n \
        var d = JSON.parse(e.data);\n \
        \n \
        if (d.type == 'broadcast') {\n \
          websocket_handle_broadcast(d);\n \
        }\n \
        \n \
        if (d.type == 'load') {\n \
          drawLoad(d);\n \
        }\n \
        if (d.type == 'log') {\n \
          document.getElementById('syslog').innerHTML = d.text;\n \
        }\n \
        if (d.type == 'time') {\n \
          document.getElementById('uptime').value = d.uptime;\n \
          document.getElementById('utc').value = d.utc;\n \
        }\n \
        if (d.type == 'adc') {\n \
          document.getElementById('adc').value = d.value;\n \
        }\n \
      }\n \
    </script>\n \
    \n \
    <br />\n \
    <span class='legend' style='background-color:#c01010;'>\n \
      &nbsp;&nbsp;&nbsp;\n \
    </span>&nbsp;CPU<br />\n \
    <span class='legend' style='background-color:#1010c0;'>\n \
      &nbsp;&nbsp;&nbsp;\n \
    </span>&nbsp;Memory<br />\n \
    <span class='legend' style='background-color:#10c010;'>\n \
      &nbsp;&nbsp;&nbsp;\n \
    </span>&nbsp;Network<br />\n \
    \n \
    <h3>Load</h3>\n \
    <canvas class='load' id='canvas_load'></canvas>\n \
    <br />\n \
    \n \
    <h3>Log</h3>\n \
    <div class='syslog'>\n \
      <pre id=syslog><span style='color:white'> LOADING ...</span></pre>\n \
    </div>\n \
    \n \
    <form class='table'>\n \
      <h3>Time</h3>\n \
      <p>\n \
        <label for='uptime'>Uptime: </label>\n \
        <input id='uptime' type='text' disabled>\n \
      </p>\n \
      <p>\n \
        <label for='utc'>UTC: </label>\n \
        <input id='utc' type='text' disabled>\n \
      </p>\n \
      <h3>ADC</h3>\n \
      <p>\n \
        <label for='adc'>Value: </label>\n \
        <input id='adc' type='text' disabled>\n \
      </p>\n \
    </form>\n \
    <br />\n \
    <form action='/reboot' method='GET'>\n \
      <input type='submit' value='Reboot' />\n \
    </form>\n \
  ");
#else // RELEASE
  html_info_fmt = PSTR("");
  html_sys_fmt  = PSTR("");
#endif // RELEASE

  html_setup_fmt = PSTR(" \
    <script>\n \
      window.onload = function(e) {\n \
        if (document.getElementsByName('ip_static')[0].checked) {\n \
          set_elements_inactive(ip_elements(), true);\n \
        }\n \
        fill_wifi_selection();\n \
      }\n \
    </script> \n \
    <br />\n \
    <div class='config'>\n \
    <form id='config' action='#' method='POST' enctype='multipart/form-data'>\n \
      <fieldset>\n \
        <legend>User</legend>\n \
        <p><label>Username:</label>\n \
        <input name='user_name' type='text' value='%s' /></p>\n \
        <p><label>Password:</label>\n \
        <input name='user_pass' type='password' /></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>WiFi</legend>\n \
        <p><label>SSID:</label>\n \
        <select name='wifi_ssid_sel'></select></p>\n \
        <p><label>Password:</label>\n \
        <input name='wifi_pass' type='password' /></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <fieldset>\n \
        <legend>IP</legend>\n \
        <input class='radio' name='ip_static' type='radio' value='0' %s />\n \
        DHCP<br />\n \
        <input class='radio' name='ip_static' type='radio' value='1' %s />\n \
        Static<br />\n \
        <hr />\n \
        <p><label>Address:</label>\n \
        <input name='ip_addr' type='text' value='%s'/></p>\n \
        <p><label>Netmask:</label>\n \
        <input name='ip_netmask' type='text' value='%s'/></p>\n \
        <p><label>Gateway:</label>\n \
        <input name='ip_gateway' type='text' value='%s'/></p>\n \
        <p><label>DNS1:</label>\n \
        <input name='ip_dns1' type='text' value='%s'/></p>\n \
        <p><label>DNS2:</label>\n \
        <input name='ip_dns2' type='text' value='%s'/></p>\n \
      </fieldset>\n \
      <br /><br />\n \
      <center>\n \
      <input type='submit' value='Save' />\n \
      </center>\n \
      <br /><br />\n \
    </form>\n \
    </div>\n \
  ");

  html_page_script = F(" \
    function websocket_handle_broadcast(d) {\n \
      if (d.value == 'reboot') {\n \
        reload = false;\n \
        document.body.innerHTML = '<br>Rebooting ...';\n \
        setTimeout(function() { document.location.href = '/'; }, 10000);\n \
        window.location.href = '/';\n \
      }\n \
      \n \
      if (d.value == 'update') {\n \
        reload = false;\n \
        document.body.innerHTML = '<br>Updating ...';\n \
        setTimeout(function() { document.location.href = '/'; }, 25000);\n \
      }\n \
    }\n \
    function ip_elements() {\n \
      return new Array(\n \
        document.getElementsByName('ip_addr')[0],\n \
        document.getElementsByName('ip_netmask')[0],\n \
        document.getElementsByName('ip_gateway')[0],\n \
        document.getElementsByName('ip_dns1')[0],\n \
        document.getElementsByName('ip_dns2')[0]\n \
      );\n \
    }\n \
    function ap_elements() {\n \
      return new Array(\n \
        document.getElementsByName('ap_addr')[0]\n \
      );\n \
    }\n \
    function mdns_elements() {\n \
      return new Array(\n \
        document.getElementsByName('mdns_name')[0]\n \
      );\n \
    }\n \
    function ntp_elements() {\n \
      return new Array(\n \
        document.getElementsByName('ntp_server')[0],\n \
        document.getElementsByName('ntp_interval')[0]\n \
      );\n \
    }\n \
    function mqtt_elements() {\n \
      return new Array(\n \
        document.getElementsByName('mqtt_server')[0],\n \
        document.getElementsByName('mqtt_user')[0],\n \
        document.getElementsByName('mqtt_pass')[0],\n \
        document.getElementsByName('mqtt_interval')[0]\n \
      );\n \
    }\n \
    function set_elements_inactive(elements, disabled) {\n \
      elements.forEach(function(elem) {\n \
        elem.disabled = disabled;\n \
      });\n \
    }\n \
    document.onclick = function(e) {\n \
      var elem = e ? e.target : window.event.srcElement;\n \
      var disabled = (elem.value === '0') ? true : false;\n \
      var elements = [];\n \
      \n \
      if (elem.name === 'ip_static')    elements = ip_elements();\n \
      if (elem.name === 'ap_enabled')   elements = ap_elements();\n \
      if (elem.name === 'mdns_enabled') elements = mdns_elements();\n \
      if (elem.name === 'ntp_enabled')  elements = ntp_elements();\n \
      if (elem.name === 'mqtt_enabled') elements = mqtt_elements();\n \
      \n \
      set_elements_inactive(elements, disabled);\n \
    }\n \
    \n \
    function drawLoad(data) {\n \
      var canvas = document.getElementById('canvas_load')\n \
      var ctx = canvas.getContext('2d');\n \
      \n \
      drawLoadAxes(ctx);\n \
      drawLoadGraph(ctx, 'cpu', data);\n \
      drawLoadGraph(ctx, 'mem', data);\n \
      drawLoadGraph(ctx, 'net', data);\n \
    }\n \
    \n \
    function drawLoadGraph(ctx, name, data) {\n \
      if (name == 'cpu') {\n \
        var color = 'rgb(192, 16, 16)'; // red\n \
        var val = data.cpu.values;\n \
      } else if (name == 'mem') {\n \
        var color = 'rgb(16, 16, 192)'; // blue\n \
        var val = data.mem.values;\n \
      } else if (name == 'net') {\n \
        var color = 'rgb(16, 192, 16)'; // green\n \
        var val = data.net.values;\n \
      }\n \
      \n \
      var delta  = ctx.canvas.width / (val.length-1);\n \
      var height = ctx.canvas.height;\n \
      var scale  = height / 100;\n \
      \n \
      ctx.beginPath();\n \
      ctx.lineWidth = 2;\n \
      ctx.strokeStyle = color;\n \
      \n \
      ctx.moveTo(0, height - (scale * val[0]));\n \
      for (i=1; i<val.length-1; i++) {\n \
        var x  = delta * i;\n\
        var y  = height - (scale * val[i]);\n \
        var xc = x + delta / 2;\n \
        var yc = (y + height - (scale * val[i+1])) / 2;\n \
        ctx.quadraticCurveTo(x, y, xc, yc);\n \
      }\n \
      ctx.quadraticCurveTo(delta * i,     height - (scale * val[i]),\n \
                           delta * (i+1), height - (scale * val[i+1]));\n \
      ctx.stroke();\n \
    }\n \
    \n \
    function drawLoadAxes(ctx) {\n \
      var x0 = 0;\n \
      var y0 = ctx.canvas.height;\n \
      var w = ctx.canvas.width;\n \
      var h = ctx.canvas.height;\n \
      \n \
      ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);\n \
      ctx.beginPath();\n \
      ctx.strokeStyle = 'rgb(128, 128, 128)';\n \
      ctx.moveTo(0, y0); ctx.lineTo(w, y0); // X axis\n \
      ctx.moveTo(x0, 0); ctx.lineTo(x0, h); // Y axis\n \
      ctx.stroke();\n \
    }\n \
  ");

  html_page_style = F(" \
    body {\n \
      background-color:#f9f9f9; color:#555555;\n \
      font-family:Helvetica Neue, Helvetica, Arial, sans-serif;\n \
      font-weight:400; font-size:85%;\n \
    }\n \
    \n \
    pre { overflow:auto; }\n \
    \n \
    .fixed { font-family:'Courier New', Courier, Monospace; }\n \
    \n \
    a:        { cursor:pointer; }\n \
    a:link    { color:#109010; text-decoration:none; font-weight:700; }\n \
    a:visited { color:#109010; text-decoration:none; font-weight:700; }\n \
    a:active  { color:#109010; text-decoration:none; font-weight:700; }\n \
    a:hover   { color:#2eca00; text-decoration:none; font-weight:700; }\n \
    \n \
    h3 { font-weight:500; font-size:1.4em; }\n \
    \n \
    p { height:0.9em; }\n \
    \n \
    .config { position:absolute; width:26em; left:50%; margin-left:-13em; }\n \
    \n \
    fieldset {\n \
      padding-right:2em; padding-left:2em; display:inline-block;\n \
      background-color:#efefef; border:solid 1px #dddddd;\n \
      border-radius:0.3em; width:22em;\n \
    }\n \
    \n \
    legend {\n \
      font-size:1.15em; font-weight:500; background-color:#efefef;\n \
      border-width:1px; border-style:solid; border-color:#dddddd;\n \
      border-radius:0.3em; padding-left:0.5em; padding-right:0.5em; \n \
    }\n \
    \n \
    label {\n \
      display:inline-block; width:10em;\n \
      text-align:right; padding-right:0.5em;\n \
    }\n \
    \n \
    input[type='text']     { width:10em; }\n \
    input[type='password'] { width:10em; }\n \
    input[type='radio']    { width:2em; margin-left:11em; }\n \
    input[type='number']   { width:3em; }\n \
    input[type='submit'] {\n \
      width:10em; color:#555555;\n \
      font-size:1em; cursor:pointer;\n \
    }\n \
    input.r { width:8.5em; text-align:right; }\n \
    select  { width:10em; color:#555555; font-size:0.85em; }\n \
    \n \
    .login {\n \
      position:absolute; width:26em; left:50%; margin-left:-13em;\n \
      height:15em; top:20%;\n \
    }\n \
    \n \
    .load { width:25em; height:5em; }\n \
    \n \
    .syslog {\n \
      font-family:'Courier New', Courier, Monospace;\n \
      border:1px solid red;\n \
      min-width:34em; min-height:5em; max-width:34em; max-height:40em;\n \
      background-color:#201030; padding:0.4em; margin-bottom:0.5em;\n \
    }\n \
  ");

  html_page_body = F(" \
    </head>\n \
    <body>\n \
  ");

#ifdef RELEASE
  html_page_menu = F(" \
    <a href='/'     >[HOME] </a>&thinsp;\n \
    <a href='/conf' >[CONF] </a>&thinsp;\n \
    <a href='/login?LOGOUT=YES'>[LOGOUT]</a><br />\n \
  ");
#else
  html_page_menu = F(" \
    <a href='/'     >[HOME] </a>&thinsp;\n \
    <a href='/conf' >[CONF] </a>&thinsp;\n \
    <a href='/info' >[INFO] </a>&thinsp;\n \
    <a href='/sys'  >[SYS]  </a>&thinsp;\n \
    <a href='/login?LOGOUT=YES'>[LOGOUT]</a><br />\n \
  ");
#endif

  html_page_header = F(" \
    <!DOCTYPE html>\n \
    <html><head>\n \
    <title>GENESYS</title>\n \
    <link rel='stylesheet' type='text/css' href='/style.css'>\n \
    <script src='script.js'></script>\n \
    <meta name='viewport' content='width=device-width, initial-scale=1'>\n \
  ");

  html_page_refresh = F(" \
    <meta http-equiv='refresh' content='2'/>\n \
  ");

  html_page_footer = F(" \
    </body></html>\n \
  ");

/*
  html_page_meter = reinterpret_cast<const __FlashStringHelper *>(
    __extension__({
      static const char __c[] PROGMEM = (
        #include "meter.html"
      ); &__c[0];
    })
  );
*/

  return (true);
}
