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

#include "system.h"
#include "config.h"
#include "lined.h"
#include "cli.h"

#include "shell.h"

String Shell::user;
String Shell::pass;

int Shell::instance = 0;

Shell::Shell(Stream &tty, bool login) : Terminal(tty) {
  memset(prompt, 0, sizeof (prompt));
  lined = lined_begin(this);
  lined_prompt(lined, prompt);

  state = LOGIN_STATE_IDLE;

  if (!login) {
    state = LOGIN_STATE_AUTHENTICATED;
    Prompt(F("%s:~$ "), system_device_name().c_str());
    lined_reset(lined);
  }

  if (instance == 0) {
    config_init();

    config_get(F("user_name"), user);
    config_get(F("user_pass"), pass);

    config_fini();
  }

  pty = instance++;
  task = -1;
}

Shell::~Shell(void) {
  lined_end(lined);
}

void Shell::Run(const String &cmd) {
  task = cli_run_command(*this, cmd);
}

void Shell::Kill(void) {
  cli_kill_task(task);
  task = cli_poll_task(task);
}

void Shell::Size(int cols, int rows) {
  Terminal::Size(cols, rows);
  lined_resize(lined, cols, rows);
}

void Shell::Prompt(const __FlashStringHelper *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vsnprintf_P(prompt, sizeof (prompt), (const char *)fmt, args);
  va_end(args);
}

bool Shell::Poll(void) {
  if (state == LOGIN_STATE_IDLE) {
    state = LOGIN_STATE_LOGIN;

    Print(F("\r\n%s V%s Firmware V%s%stty/%i\r\n\r\n"),
      system_hw_device().c_str(),  system_hw_version().c_str(),
      system_fw_version().c_str(), system_fw_build().c_str(), pty 
    );
  } else if (state == LOGIN_STATE_LOGIN) {
    state = LOGIN_STATE_USERNAME;
    user_ok = pass_ok = false;

    Prompt(F("%s login: "), system_device_name().c_str());
    lined_echo(lined, 1);
    lined_reset(lined);
  } else if (state == LOGIN_STATE_USERNAME) {
    if (tty.available()) {
      int key = lined_poll(lined);

      if (key == TERM_KEY_ENTER) {
        const char *line = lined_line(lined);

        if (user == line) user_ok = true;
        Print(F("\r\n"));
        state = LOGIN_STATE_PASSWORD;

        Prompt(F("Password: "));
        lined_reset(lined);
        lined_echo(lined, 0);
        lined_reset(lined);
      } else if (key == TERM_KEY_CTRL_D) {
        state = LOGIN_STATE_EXIT;
      }
    }
  } else if (state == LOGIN_STATE_PASSWORD) {
    if (tty.available()) {
      int key = lined_poll(lined);

      if (key == TERM_KEY_ENTER) {
        const char *line = lined_line(lined);

        if (pass == line) pass_ok = true;
        Print(F("\r\n\r\n"));

        if (user_ok && pass_ok) {
          state = LOGIN_STATE_AUTHENTICATED;
          Prompt(F("%s:~$ "), system_device_name().c_str());
          lined_echo(lined, 1);
          lined_reset(lined);
        } else {
          state = LOGIN_STATE_LOGIN;
          Print(F("Login incorrect\r\n"));
        }
      } else if (key == TERM_KEY_CTRL_D) {
        state = LOGIN_STATE_EXIT;
      }
    }
  } else if (state == LOGIN_STATE_AUTHENTICATED) {
    if (task != -1) { // there is a task running in this shell
      if (tty.available()) {
        if (tty.peek() == TERM_KEY_CTRL_C) {
          cli_kill_task(task);
        }
      }

      task = cli_poll_task(task);

      if (task == -1) lined_reset(lined);
    } else {
      if (tty.available()) {
        int key = lined_poll(lined);

        if (key == TERM_KEY_ENTER) {
          const char *cmd = lined_line(lined);

          Print(F("\r\n"));

          if (String(cmd) == F("logout")) {
            state = LOGIN_STATE_EXIT;
          } else {
            task = cli_run_command(*this, cmd);
            lined_history_add(cmd);
          }

          lined_reset(lined);
        } else if (key == TERM_KEY_CTRL_D) {
          state = LOGIN_STATE_EXIT;
        }
      }
    }
  } else if (state == LOGIN_STATE_EXIT) {
    return (false);
  }

  return (true);
}
