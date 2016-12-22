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

#include "module.h"
#include "shell.h"
#include "log.h"

#include "console.h"

static bool active  = false;
static Shell *shell = NULL;

static void logout(void) {
#ifndef RELEASE
  if (shell) {
    shell->Kill();

    delete (shell);
    shell = NULL;

    console_print(F("\r\n"));

    log_print(F("CONS: leaving shell ..."));
  }
#endif
}

int console_state(void) {
  if (active) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool console_init(void) {
#ifndef RELEASE
  if (active) return (false);

  Serial.begin(115200);

  console_print(F("\r\n\r\n"));

  active = true;

  return (true);
#else
  return (false);
#endif
}

bool console_fini(void) {
#ifndef RELEASE
  if (!active) return (false);

  delete (shell);
  shell = NULL;

  Serial.end();

  active = false;

  return (true);
#else
  return (false);
#endif
}

void console_poll(void) {
#ifndef RELEASE
  if (shell) {
    if (!shell->Poll()) logout();
  } else {
    int w, h, c = Serial.read();

    if ((c == '\n') || (c == '\r')) {
      log_print(F("CONS: starting shell (CTRL-D to exit) ..."));

      shell = new Shell(Serial, true); // true = login
    }
  }
#endif
}

bool console_print(const char *str, uint16_t len) {
#ifndef RELEASE
  if (shell) return (false);

  return (Serial.write(str, len));
#else
  return (false);
#endif
}

bool console_print(const String &str) {
#ifndef RELEASE
  return (console_print(str.c_str(), str.length()));
#else
  return (false);
#endif
}

void console_kill_shell(void) {
#ifndef RELEASE
  logout();
#endif
}

void console_dump_debug_info(void) {
#ifndef RELEASE
  if (shell) shell->Run(F("info all"));
#endif
}

MODULE(console)
