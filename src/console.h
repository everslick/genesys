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

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <Arduino.h>

int  console_state(void);
bool console_init(void);
bool console_fini(void);
void console_poll(void);

bool console_print(const char *str, uint16_t len);
bool console_print(const String &str);

void console_kill_shell(void);
void console_dump_debug_info(void);

#endif // _CONSOLE_H_
