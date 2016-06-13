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

/**
  \file
  \brief  Macros to generate debug and error logs.
  \author Clemens Kirchgatterer <clemens@1541.org>
  \date   2004-08-18
*/

#ifndef _LOG_H_
#define _LOG_H_

#include <Arduino.h>

#define COL_BLACK   0
#define COL_RED     1
#define COL_GREEN   2
#define COL_YELLOW  3
#define COL_BLUE    4
#define COL_MAGENTA 5
#define COL_CYAN    6
#define COL_WHITE   7
#define COL_DEFAULT 9

/*
  Debug and log output can be sent to different log channels. This log      \n
  channel can be specified in log_init().
*/
enum LogChannel {
  LOG_CHANNEL_NONE    = 0, ///< print to /dev/null
  LOG_CHANNEL_SERIAL  = 1, ///< print to serial console
  LOG_CHANNEL_NETWORK = 2, ///< send each log line over a UDP network socket
  LOG_CHANNEL_BOTH    = 3, ///< send each log line to both channels
};

void loginit(void);
void logpoll(void);
void lograw(const String &text);
void logprogress(const String &prefix, const String &postfix, int value);
void logdumpraw(String &str);
void logdumphtml(String &str);
void logclear(void);
void logcolortext(uint8_t col);
void logcolortime(uint8_t col);
void logend(void);

void logprint(const __FlashStringHelper *fmt, ...);
void logprint(const char *fmt, ...);

void  logregistermessagecb(void (*)(const char *msg));

char *log_color_str(char buf[], uint8_t col);

#ifdef RELEASE

#define log_init()
#define log_poll()

#define log_raw(STR)
#define log_progress(PRE, POST, VAL)
#define log_dump_raw(STR)
#define log_dump_html(STR)
#define log_clear()
#define log_color_text(COL)
#define log_color_time(COL)
#define log_end()
#define log_print(...)

#define log_register_message_cb(MSG)

#else // RELEASE

/**
  \param C \a log channel to use for output
  \param D \a ip and port number for NETWORK logging ("hostname:port")

  \par Examples:
    \code log_init(LOG_CHANNEL_SERIAL, NULL, 0);                \endcode
    \code log_init(LOG_CHANNEL_NETWORK, "192.168.1.123", 4000); \endcode

  %log_init() initializes the Log library. You have to specify the desired   \n
  #LogChannel and a pointer to a const string containing ip address (or     \n
  hostname) and port number of the remote maschine to send each logline to.

  \note
  This function \b must be called before any other Log*() functions can be  \n
  used.
*/
#define log_init() loginit()

#define log_poll() logpoll()

#define log_raw(STR) lograw(STR)

#define log_progress(PRE, POST, VAL) logprogress(PRE, POST, VAL)

#define log_dump_raw(STR)  logdumpraw(STR)
#define log_dump_html(STR) logdumphtml(STR)

#define log_clear() logclear()

#define log_color_text(COL) logcolortext(COL)
#define log_color_time(COL) logcolortime(COL)

/**
  Whenever the program ends, or you wish to stop using any Log* functions,  \n
  call %log_end(). It closes the logfile and resets all parameters to their \n
  start values.

  \note
  No other Log*() functions may be called after %log_end() exept log_init().
*/
#define log_end() logend()

/**
  To print a text to all set up log channels use this macro.

  Messages logged by %log_print() show up in the specified LogChannel. The  \n
  only way to suppress them is to switch of logging completely by defining  \n
  RELEASE to 1.
*/
#define log_print(...) logprint(__VA_ARGS__)

#define log_register_message_cb(MSG) logregistermessagecb(MSG)

#endif // RELEASE

#define LINE_THIN   F("----------------------------------------------\n")
#define LINE_MEDIUM F("==============================================\n")
#define LINE_THICK  F("##############################################\n")

#endif // _LOG_H_
