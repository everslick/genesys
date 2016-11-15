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

#ifndef _HTML_H_
#define _HTML_H_

enum {
  CONF_HEADER,
  CONF_USER,
  CONF_DEVICE,
  CONF_WIFI,
  CONF_IP,
  CONF_AP,
  CONF_MDNS,
  CONF_NTP,
  CONF_RTC,
  CONF_MQTT,
  CONF_UPDATE,
  CONF_STORAGE,
  CONF_FOOTER
};

extern const char *html_page_style_css;
extern const char *html_page_reset_css;

extern const char *html_page_common_js;
extern const char *html_page_config_js;
extern const char *html_page_sys_js;

bool html_init(void);

void html_client_connected_via_softap(void);
void html_client_connected_via_wifi(void);

void html_insert_page_header(String &html);
void html_insert_page_body(String &html, bool menu = true);
void html_insert_page_footer(String &html);

void html_insert_login_content(String &html, const String &msg);
void html_insert_file_content(String &html, const String &path);
void html_insert_conf_content(String &html, int conf);
void html_insert_root_content(String &html);
void html_insert_info_content(String &html);
void html_insert_sys_content(String &html);

void html_insert_module_header(String &html);
void html_insert_module_row(String &html, int module);
void html_insert_module_footer(String &html);

void html_insert_websocket_script(String &html);
void html_insert_upload_form(String &html);

#endif // _HTML_H_
