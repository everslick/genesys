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

#ifndef _SHELL_H_
#define _SHELL_H_

#include <Arduino.h>

#include "terminal.h"
#include "lined.h"

class Shell : public Terminal {

public:

  Shell(Stream &tty, bool login = false);
  ~Shell(void);

  void Run(const String &cmd);
  void Kill(void);
  bool Poll(void);

  void Size(int cols, int rows);

private:

  void Prompt(const __FlashStringHelper *fmt, ...);

  static String user;
  static String pass;

  static int instance;

  // login state machine
  enum LoginState {
    LOGIN_STATE_IDLE,
    LOGIN_STATE_LOGIN,
    LOGIN_STATE_USERNAME,
    LOGIN_STATE_PASSWORD,
    LOGIN_STATE_AUTHENTICATED,
    LOGIN_STATE_EXIT
  } state;

  char prompt[24];
  lined_t *lined;

  bool user_ok;
  bool pass_ok;

  int task;

};

#endif // _SHELL_H_
