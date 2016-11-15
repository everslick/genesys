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

#include "i18n.h"

#include "util.h"

#define INT32_MAX_LENGTH 11

String float2str(float f, int prec) {
  String s(f, prec);

  s.replace('.', I18N_FLOAT_COMMA);

  return (s);
}

int modulo(int a, int b) {
  int r = a % b;

  return ((r < 0) ? r + b : r);
}

const char *int2str(int32_t i) {
  static const char numbers[10] = {'0','1','2','3','4','5','6','7','8','9'};
  static const char reverse[10] = {'9','8','7','6','5','4','3','2','1','0'};
  static const char *reverse_end = &reverse[9];
  static char buf[INT32_MAX_LENGTH + 2];
  char *p = buf + INT32_MAX_LENGTH + 1;

  *--p = '\0';
  if (i >= 0) {
    do { *--p = numbers[i%10]; i/=10; } while (i);
  } else {
    do { *--p = reverse_end[i%10]; i/=10; } while (i);
    *--p = '-';
  }

  return (p);
}

String formatInt(int32_t value, uint8_t size, char insert) {
  String ret;

  for (uint8_t i=1; i<=size; i++) {
    uint32_t max = pow(10, i);

    if (value < max) {
      for (uint8_t j=size-i; j>0; j--) {
        ret += insert;
      }
      break;
    }
  }

  ret += value;

  return (ret);
}
