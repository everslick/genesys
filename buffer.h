/**
  \file
  \author  Clemens Kirchgatterer <clemens@1541.org>
  \date   2003-11-30
  \brief  A simple and efficient buffer class.

  This universal buffer class implements nice methodes and operators to    \n
  fill it with data. Each time the current size threshold is reached, it   \n
  will double it's capacity, starting at a default size of INITLEN bytes.
*/

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <Arduino.h>

/// simple and efficient buffer class
class Buffer {

public:

  /// default constructor
  Buffer(void);
  /// creates an empty buffer of initial size
  Buffer(uint32_t len);
  /// copy constructor
  Buffer(const Buffer &b);
  /// generic void pointer constructor
  Buffer(const void *data, uint32_t len);
  /// constructor for initializing with std::string
  Buffer(const String &s);

  /// free reserved buffer space
  ~Buffer(void);

  /// reserve len bytes for buffer
  void init(uint32_t len);
  /// clear the internal buffer
  void clear(void);

  /// append an arbitrary number of bytes to the buffer
  void append(const void *data, uint32_t len);
  /// append a stl string to the buffer
  void append(const String &s);
  /// append an ansi C character to the buffer
  void append(char c);

  /// fill the buffer with a string of arbitrary bytes
  void assign(const void *data, uint32_t len);
  /// assign a stl string to the buffer
  void assign(const String &s);
  /// clear buffer and assign one byte to it
  void assign(char c);

  /// assign a buffer to a buffer
  Buffer &operator=(const Buffer &b);
  /// assign a stl string to the buffer
  Buffer &operator=(const String &s);
  /// assign an ansi C character to the buffer
  Buffer &operator=(char c);

  /// append a buffer to a buffer
  Buffer &operator+=(const Buffer &b);
  /// append a stl string to a buffer
  Buffer &operator+=(const String &s);
  /// append a buffer with an ansi C character
  Buffer &operator+=(char c);

  /// return a reference to the character on a specific position
  char &operator[](uint32_t pos);

  /// return a pointer to the internal buffer
  const char *data(void) const { buf[used] = '\0'; return (buf); }
  /// return the content of the internal buffer in a std::string
  String str(void) const { return (String(data())); }
  /// return the current used size of the buffer
  uint32_t size(void) const { return (used); }

private:

  uint32_t length;
  uint32_t used;

  char *buf;

};

#endif // _BUFFER_H_
