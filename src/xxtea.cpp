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

#include <string.h>

#include "xxtea.h"

#define MX (((z>>5)^(y<<2)) + ((y>>3)^(z<<4)))^((sum^y) + (key[(p&3)^e]^z))
#define DELTA 0x9e3779b9

static uint32_t key[4];

static bool encrypt(uint32_t *data, size_t len) {
  uint32_t n = len - 1;
  uint32_t z = data[n], y, p, q = 6 + 52 / (n + 1), sum = 0, e;

  if (n < 1) return (false);

  while (0 < q--) {
    sum += DELTA;
    e = sum >> 2 & 3;

    for (p = 0; p < n; p++) {
      y = data[p + 1];
      z = data[p] += MX;
    }

    y = data[0];
    z = data[n] += MX;
  }

  return (true);
}

static  bool decrypt(uint32_t *data, size_t len) {
  uint32_t n = len - 1;
  uint32_t z, y = data[0], p, q = 6 + 52 / (n + 1), sum = q * DELTA, e;

  if (n < 1) return (false);

  while (sum != 0) {
    e = sum >> 2 & 3;

    for (p = n; p > 0; p--) {
      z = data[p - 1];
      y = data[p] -= MX;
    }

    z = data[n];
    y = data[0] -= MX;
    sum -= DELTA;
  }

  return (true);
}

bool xxtea_encrypt(char *buf, int len) {
  if (len % 4) return (false);

  return (encrypt((uint32_t *)buf, len / 4));
}

bool xxtea_decrypt(char *buf, int len) {
  if (len % 4) return (false);

  return (decrypt((uint32_t *)buf, len / 4));
}

bool xxtea_init(const char *id, uint32_t salt) {
  strcpy((char *)key, id); // key is 16 bytes, id is 12 bytes

  key[3] = salt; // initialize last 4 key bytes with salt
}
