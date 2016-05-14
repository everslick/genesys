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

#include "buffer.h"

#define INITLEN 32 // default size of buffer payload

Buffer::Buffer(void) {
  init(INITLEN);
}

Buffer::Buffer(uint32_t len) {
  init(len);
}

Buffer::Buffer(const Buffer &b) {
  init(b.used);

  append(b.buf, b.used);
}

Buffer::Buffer(const void *data, uint32_t len) {
  init(len);

  append(data, len);
}

Buffer::Buffer(const String &s) {
  init(s.length());

  append(s);
}

Buffer::~Buffer(void) {
  free(buf);
}

void
Buffer::init(uint32_t len) {
  length = len;
  used = 0;

  if (!(buf = (char *)malloc(length))) {
    length = used = 0;

    return;
  }
}

void
Buffer::clear(void) {
  used = 0;
}

void
Buffer::append(const void *data, uint32_t len) {
  if ((used + len) > length) {
    length = (length > 8*1024) ? (used + len + 1024) : (used + len) * 2;

    if (!(buf = (char *)realloc(buf, length))) {
      length = used = 0;

      return;
    }
  }

  memcpy(buf + used, data, len);

  used += len;
}

void
Buffer::append(const String &s) {
  append(s.c_str(), s.length());
}

void
Buffer::append(char c) {
  append(&c, 1);
}

void
Buffer::assign(const void *data, uint32_t len) {
  used = 0;

  append(data, len);
}

void
Buffer::assign(const String &s) {
  used = 0;

  append(s);
}

void
Buffer::assign(char c) {
  used = 0;

  append(c);
}

Buffer &
Buffer::operator=(const Buffer &b) {
  if (&b != this) assign(b.buf, b.used);

  return (*this);
}

Buffer &
Buffer::operator=(const String &s) {
  assign(s);

  return (*this);
}

Buffer &
Buffer::operator=(char c) {
  assign(c);

  return (*this);
}

Buffer &
Buffer::operator+=(const Buffer &b) {
  append(b.buf, b.used);

  return (*this);
}

Buffer &
Buffer::operator+=(const String &s) {
  append(s);

  return (*this);
}

Buffer &
Buffer::operator+=(char c) {
  append(c);

  return (*this);
}

char &
Buffer::operator[](uint32_t pos) {
  return (buf[pos]);
}