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

#include "terminal.h"

Terminal::Terminal(Stream &tty) : tty(tty) {
  width  = 0;
  height = 0;

  pty = -1;
}

Terminal::~Terminal(void) {
}

int Terminal::Print(const __FlashStringHelper *fmt, ...) {
  char buffer[128];
  va_list args;
  int length;

  va_start(args, fmt);
  length = vsnprintf_P(buffer, sizeof (buffer), (const char *)fmt, args);
  va_end(args);

  tty.print(buffer);

  return (min(sizeof (buffer), length));
}

int Terminal::Print(const String &str) {
  tty.print(str);

  return (str.length());
}

int Terminal::Center(const String &str) {
  int n = (width - str.length()) / 2;

  Insert(' ', n);

  return (Print(str) + n);
}

int Terminal::Center(const __FlashStringHelper *fmt, ...) {
  char buffer[128];
  va_list args;
  int length;

  va_start(args, fmt);
  length = vsnprintf_P(buffer, sizeof (buffer), (const char *)fmt, args);
  va_end(args);

  int n = (width - min(sizeof (buffer), length)) / 2;

  Insert(' ', n);

  tty.print(buffer);

  return (min(sizeof (buffer), length) + n);
}

void Terminal::LineFeed(int n) {
  String str;

  for (int i=0; i<n; i++) str += F("\r\n");

  Print(str);
}

void Terminal::ScreenClear(void) {
  tty.print(F("\e[H\e[J"));
}

void Terminal::ScreenSave(void) {
  tty.print(F("\e[?47h"));
}

void Terminal::ScreenRestore(void) {
  tty.print(F("\e[?47l"));
}

void Terminal::LineClear(int part) {
  Print(F("\e[%iK"), part);
}

void Terminal::CursorPosition(int x, int y) {
  Print(F("\e[%i;%iH"), y, x);
  Print(F("\e[%i;%if"), y, x);
}

void Terminal::CursorUp(int i) {
  Print(F("\e[%iA"), i);
}

void Terminal::CursorDown(int i) {
  Print(F("\e[%iB"), i);
}

void Terminal::CursorRight(int i) {
  Print(F("\e[%iC"), i);
}

void Terminal::CursorLeft(int i) {
  Print(F("\e[%iD"), i);
}

void Terminal::CursorHide(void) {
  Print(F("\e[?25l"));
}

void Terminal::CursorShow(void) {
  Print(F("\e[?25h"));
}

void Terminal::Color(int attr, int fg, int bg) {
  Print(F("\e[%i;%i;%im"), attr, fg + 30, bg + 40);
}

int Terminal::Insert(char c, int n) {
  char buf[n + 1];

  memset(buf, c, n);
  buf[n] = '\0';

  Print(buf);

  return (n);
}

void Terminal::Size(int cols, int rows) {
  width  = cols;
  height = rows;
}

bool Terminal::GetCursorPosition(int &col, int &row) {
  unsigned int i = 0;
  char buf[16];

  // report cursor location
  Print(F("\e[6n"));

  // read the response: ESC [ rows ; cols R
  while (i < sizeof (buf) - 1) {
    if (tty.readBytes(buf+i, 1) != 1) break;
    if (buf[i++] == 'R') break;
  }
  buf[i] = '\0';

  // parse it
  if ((buf[0] != 27) || (buf[1] != '[')) {
    return (false);
  }
  if (sscanf(buf+2, "%i;%i", &row, &col) != 2) {
    return (false);
  }

  return (true);
}

bool Terminal::GetSize(int &cols, int &rows) {
  int col, row;

  // get the initial position so we can restore it later
  if (!GetCursorPosition(col, row)) return (false);

  // go to bottom-right corner
  CursorPosition(999, 999);

  // get current position
  if (!GetCursorPosition(cols, rows)) return (false);

  // restore position
  CursorPosition(col, row);

  return (true);
}

uint8_t Terminal::GetKey(void) {
  uint8_t c, seq[4];

  if (!tty.available()) return (TERM_KEY_NONE);

  tty.readBytes(&c, 1);

  if (c ==   9) return (TERM_KEY_TAB);
  if (c ==  13) return (TERM_KEY_ENTER);
  if (c == 127) return (TERM_KEY_BACKSPACE);
  if (c ==  27) { // escape sequence

    // Read the next two bytes representing the escape sequence.
    // Use two calls to handle slow terminals returning the two
    // chars at different times.

    if (tty.readBytes(seq+0, 1) == -1) return (TERM_KEY_NONE);

    if (seq[0] == 27) return (TERM_KEY_ESC);

    if (tty.readBytes(seq+1, 1) == -1) return (TERM_KEY_NONE);

    // ESC [ sequences
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        // Extended escape, read additional byte
        if (tty.readBytes(seq+2, 1) == -1) return (TERM_KEY_NONE);

        if (seq[2] == '~') {
          if (seq[1] == '2') return (TERM_KEY_INSERT);
          if (seq[1] == '3') return (TERM_KEY_DELETE);
          if (seq[1] == '5') return (TERM_KEY_PAGEUP);
          if (seq[1] == '6') return (TERM_KEY_PAGEDOWN);
        } else {
          if (tty.readBytes(seq+3, 1) == -1) return (TERM_KEY_NONE);
        }
      } else {
        if (seq[1] == 'A') return (TERM_KEY_UP);
        if (seq[1] == 'B') return (TERM_KEY_DOWN);
        if (seq[1] == 'C') return (TERM_KEY_RIGHT);
        if (seq[1] == 'D') return (TERM_KEY_LEFT);
        if (seq[1] == 'H') return (TERM_KEY_HOME);
        if (seq[1] == 'F') return (TERM_KEY_END);
      }
    } else if (seq[0] == 'O') { // ESC O sequences
      if (seq[1] == 'H') return (TERM_KEY_HOME);
      if (seq[1] == 'F') return (TERM_KEY_END);
    }
  }

  return (c);
}
