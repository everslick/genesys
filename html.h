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

extern const __FlashStringHelper *html_page_header;
extern const __FlashStringHelper *html_page_refresh;
extern const __FlashStringHelper *html_page_style;
extern const __FlashStringHelper *html_page_script;
extern const __FlashStringHelper *html_page_body;
extern const __FlashStringHelper *html_page_menu;
extern const __FlashStringHelper *html_page_footer;
extern const __FlashStringHelper *html_page_load;

bool html_init(void);

void html_insert_page_header(String &html);
void html_insert_page_refresh(String &html);
void html_insert_page_body(String &html);
void html_insert_page_menu(String &html);
void html_insert_page_footer(String &html);

void html_insert_root_content(String &html);
void html_insert_info_content(String &html);
void html_insert_conf_content(String &html);
void html_insert_setup_content(String &html);
void html_insert_meter_content(String &html);
void html_insert_wave_content(String &html);
void html_insert_sys_content(String &html);

void html_insert_login_content(const String &msg, String &html);

void html_insert_page_redirect(String &html, int delay);

#endif // _HTML_H_
