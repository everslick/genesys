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

#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#include <Stream.h>

// TERMINAL ATTRIBUTES
#define TERM_RESET           0
#define TERM_BRIGHT          1
#define TERM_DIM             2
#define TERM_UNDERLINE       3
#define TERM_BLINK           4
#define TERM_REVERSE         7
#define TERM_HIDDEN          8

// TERMINAL COLORS
#define TERM_BLACK           0
#define TERM_RED             1
#define TERM_GREEN           2
#define TERM_YELLOW          3
#define TERM_BLUE            4
#define TERM_MAGENTA         5
#define TERM_CYAN            6
#define TERM_WHITE           7
#define TERM_DEFAULT         9

// TERMINAL KEY CODES
#define TERM_KEY_NONE        0
#define TERM_KEY_CTRL_A      1
#define TERM_KEY_CTRL_B      2
#define TERM_KEY_CTRL_C      3
#define TERM_KEY_CTRL_D      4
#define TERM_KEY_CTRL_E      5
#define TERM_KEY_CTRL_F      6
#define TERM_KEY_BELL        7
#define TERM_KEY_BACKSPACE   8
#define TERM_KEY_TAB         9
#define TERM_KEY_LINEFEED   10
#define TERM_KEY_CTRL_K     11
#define TERM_KEY_CTRL_L     12
#define TERM_KEY_ENTER      13
#define TERM_KEY_CTRL_N     14
#define TERM_KEY_CTRL_O     15
#define TERM_KEY_CTRL_P     16
#define TERM_KEY_CTRL_Q     17
#define TERM_KEY_CTRL_R     18
#define TERM_KEY_CTRL_S     19
#define TERM_KEY_CTRL_T     20
#define TERM_KEY_CTRL_U     21
#define TERM_KEY_CTRL_V     22
#define TERM_KEY_CTRL_W     23
#define TERM_KEY_CTRL_X     24
#define TERM_KEY_CTRL_Y     25
#define TERM_KEY_CTRL_Z     26
#define TERM_KEY_ESC        27
#define TERM_KEY_DELETE    127

#define TERM_KEY_LEFT      128
#define TERM_KEY_RIGHT     129
#define TERM_KEY_UP        130
#define TERM_KEY_DOWN      131
#define TERM_KEY_PAGEUP    132
#define TERM_KEY_PAGEDOWN  133
#define TERM_KEY_INSERT    134
#define TERM_KEY_HOME      135
#define TERM_KEY_END       136

class Terminal {

public:

  enum {
    CLEAR_WHOLE_LINE,
    CLEAR_LEFT_FROM_CURSOR,
    CLEAR_RIGHT_FROM_CURSOR
  };

  Terminal(Stream &tty);
  virtual ~Terminal(void);

  int Center(const __FlashStringHelper *fmt, ...);
  int Center(const String &str);

  int Print(const __FlashStringHelper *fmt, ...);
  int Print(const String &str);

  int Insert(char c, int n = 1);

  void LineFeed(int n = 1);
  void LineClear(int part = CLEAR_WHOLE_LINE);

  void ScreenClear(void);
  void ScreenSave(void);
  void ScreenRestore(void);

  void CursorPosition(int x, int y);
  void CursorUp(int i);
  void CursorDown(int i);
  void CursorRight(int i);
  void CursorLeft(int i);
  void CursorHide(void);
  void CursorShow(void);

  void Color(int attr, int fg, int bg);

  virtual void Size(int cols, int rows);

  bool GetSize(int &rows, int &cols);
  bool GetCursorPosition(int &row, int &col);

  int Width(void)  { return (width);  }
  int Height(void) { return (height); }

  uint8_t GetKey(void);

  Stream &tty;
  int pty;

protected:

  int width;
  int height;

};

#endif // _TERMINAL_H_
