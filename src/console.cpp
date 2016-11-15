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

#include "shell.h"
#include "log.h"

#include "console.h"

static bool active  = false;
static Shell *shell = NULL;

static void logout(void) {
  if (shell) shell->Kill();

  delete (shell);
  shell = NULL;

  console_print(F("\r\n"));

  log_print(F("CONS: leaving shell ...\r\n"));
}

bool console_init(void) {
  if (active) return (false);

  Serial.begin(115200);

  console_print(F("\r\n\r\n"));

  active = true;

  return (true);
}

bool console_fini(void) {
  if (!active) return (false);

  Serial.end();

  delete (shell);
  shell = NULL;

  active = false;

  return (true);
}

void console_poll(void) {
  if (shell) {
    if (!shell->Poll()) logout();
  } else {
    int w, h, c = Serial.read();

    if ((c == '\n') || (c == '\r')) {
      log_print(F("CONS: starting shell (CTRL-D to exit) ...\r\n"));

      shell = new Shell(Serial, true); // true = login
    }
  }
}

bool console_print(const char *str, uint16_t len) {
  if (shell) return (false);

  return (Serial.write(str, len));
}

bool console_print(const String &str) {
  console_print(str.c_str(), str.length());
}

void console_kill_shell(void) {
  logout();
}

void console_dump_debug_info(void) {
  if (shell) shell->Run(F("info all"));
}
