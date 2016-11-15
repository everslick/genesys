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
#include "module.h"
#include "config.h"
#include "shell.h"
#include "net.h"
#include "cli.h"
#include "log.h"

#include "telnet.h"

// server defines
#define TELNET_PORT     23
#define TELNET_SESSIONS  3

// telnet defines
#define IAC        255 // interpret as command
#define DONT       254
#define DO         253
#define WONT       252
#define WILL       251

#define SE         240 // sub-negotiation end
#define NOP        241 // no operation
#define AYT        246 // are you there
#define SB         250 // sub-negotiation begin

#define ECHO         1 // local echo
#define SGA          3 // suppress go ahead
#define TTYPE       24 // terminal type
#define NAWS        31 // window size
#define LINEMODE    34 // linemode option
#define NEW_ENVIRON 39 // new environment variables

#define SEND 1

// telnet state machine
enum TelnetState {
  TELNET_STATE_IDLE,
  TELNET_STATE_CONNECTING,
  TELNET_STATE_CONNECTED,
  TELNET_STATE_RUNNING,
  TELNET_STATE_DISCONNECTING
};

struct telnet_t {
  TelnetState state;

  int negotiation;
  int slot;

  // sub-negotiation state and buffer
  bool    sub_mode;
  uint8_t sub[16];
  size_t  sub_len;

  Shell *shell;

  // TCP context
  WiFiClient client;
};

struct TELNET_PrivateData {
  WiFiServer *server;

  telnet_t *session[TELNET_SESSIONS];
};

static TELNET_PrivateData *p = NULL;

static int telnet_getc(telnet_t *session) {
  if (session && session->client) {
    return (session->client.read());
  }

  return (-1);
}

static uint8_t telnet_options(uint8_t opt) {
  if (opt == ECHO)        return (WILL); // we will echo input
  if (opt == SGA)         return (WILL); // we will set graphics modes
  if (opt == NEW_ENVIRON) return (WONT); // we will not set new environments

  return (0);
}

static uint8_t telnet_willack(uint8_t opt) {
  if (opt == ECHO)        return (DONT); // client must not echo its own input
  if (opt == SGA)         return (DO);   // client can set a graphics mode
  if (opt == NAWS)        return (DO);   // client should tell us its win size
  if (opt == TTYPE)       return (DO);   // client should tell us its terminal
  if (opt == LINEMODE)    return (DONT); // no linemode
  if (opt == NEW_ENVIRON) return (DO);   // client can set a new environment

  return (0);
}

static void telnet_command(telnet_t *session, int cmd, int opt) {
  if (cmd == DO || cmd == DONT) {
    // DO commands say what the client should do
    if ((cmd == DO) || (cmd == DONT)) {
      session->shell->Print(F("%c%c%c"), IAC, cmd, opt);
    }
  } else if (cmd == WILL || cmd == WONT) {
    // similarly, WILL commands say what the server will do
    if ((cmd == WILL) || (cmd == WONT)) {
      session->shell->Print(F("%c%c%c"), IAC, cmd, opt);
    }
  } else {
    // other commands are sent raw
    session->shell->Print(F("%c%c"), IAC, cmd);
  }
}

static void telnet_negotiate(telnet_t *session) {
  for (int option=0; option<256; option++) {
    if (telnet_options(option)) {
      telnet_command(session, telnet_options(option), option);
    }
  }

  for (int option=0; option<256; option++) {
    if (telnet_willack(option)) {
      telnet_command(session, telnet_willack(option), option);
    }
  }
}

static void telnet_begin(telnet_t *session) {
  String name = system_device_name();

  // set terminal title
  session->shell->Print(F("\033ktelnet://%s\033\134"), name.c_str());
  session->shell->Print(F("\033]1;telnet://%s\007"),   name.c_str());
  session->shell->Print(F("\033]2;telnet://%s\007"),   name.c_str());

  // clear the screen
  session->shell->ScreenClear();

  // display the MOTD
  session->shell->Print(F("\033[2m"));
  session->shell->LineFeed(2);
  session->shell->Center(F("ESPADE Telnet Server"));
  session->shell->Print(F("\033[1m"));
  session->shell->LineFeed(2);

  session->shell->Center(
    F("           ]quit\033[0m or \033[1mCTRL-D\033[0m to exit!")
  );

  session->shell->LineFeed(2);
  session->shell->Print(F("\033[?25h"));
}

static void telnet_end(telnet_t *session) {
  // print escape sequences to return cursor to visible mode
  session->shell->Print(F("\033[?25h\033[0m\033[H\033[2J"));
  session->shell->Print(F("\033[1mlogged off from telnet server\033[0m\r\n"));
}

static telnet_t *telnet_new(WiFiClient &client, int slot) {
  telnet_t *session = new telnet_t();

  if (session) {
    session->slot        = slot;
    session->client      = client;
    session->state       = TELNET_STATE_CONNECTING;
    session->shell       = new Shell(session->client, true);
    session->sub_mode    = false;
    session->sub_len     = 0;
    session->negotiation = 0;
  }

  return (session);
}

static int telnet_delete(telnet_t *session) {
  int slot = -1;

  if (session) {
    slot = session->slot;

    log_print(F("TLNT: closing connection to client [%i]\r\n"), slot);

    if (session->client) {
      telnet_end(session);
      delete (session->shell);

      session->client.stop();
      session->client = WiFiClient();
    }

    delete (session);
  }

  return (slot);
}

static void telnet_process(telnet_t *session) {
  static int received_new_win_size = 0;
  int i = session->client.peek();
  bool key = (i >= 0);
  uint8_t c = i;

  if (key && (c == IAC)) { // interpret as command
    telnet_getc(session); // we only peeked, now remove it

    c = telnet_getc(session); // get the command

    if (c == SE) { // end of extended option mode
      session->sub_mode = false;
      if (session->sub[0] == TTYPE) {
        char *term = (char *)&session->sub[2];
      } else if (session->sub[0] == NAWS) {
        session->negotiation++;

        int width  = (session->sub[1] << 8) | session->sub[2];
        int height = (session->sub[3] << 8) | session->sub[4];

        session->shell->Size(width, height);

        // negotiation is complete
        if (session->negotiation == 2) {
          session->state = TELNET_STATE_CONNECTED;
        }
      }
    } else if ((c == NOP) || (c == AYT)) { // no op / are you there
      telnet_command(session, NOP, 0);
    } else if ((c == WILL) || (c == WONT)) { // will / won't negotiation
      uint8_t opt = telnet_getc(session);

      if (telnet_willack(opt)) {
        telnet_command(session, telnet_willack(opt), opt);
      } else {
        telnet_command(session, WONT, opt); // default to WONT
      }
      if ((c == WILL) && (opt == TTYPE)) {
        session->shell->Print(
          F("%c%c%c%c%c%c"), IAC, SB, TTYPE, SEND, IAC, SE
        );
      }
    } else if ((c == DO) || (c == DONT)) { // do / don't negotiation
      uint8_t opt = telnet_getc(session);

      if (telnet_options(opt)) {
        telnet_command(session, telnet_options(opt), opt);
      } else {
        telnet_command(session, DONT, opt); // default to DONT
      }
    } else if (c == SB) { // begin extended option mode
      session->sub_mode = true;
      session->sub_len  = 0;

      memset(session->sub, 0, sizeof (session->sub));
    } else if (c == IAC) { // 0xff, 0xff
      // TODO feed 0xff into data stream
    }
  } else if (key && session->sub_mode) {
    telnet_getc(session); // we only peeked, now remove it

    if (session->sub_len < sizeof (session->sub) - 1) {
      session->sub[session->sub_len++] = c;
    }
  } else {
    if (session->state == TELNET_STATE_IDLE) {
      // do nothing but waiting for state change
    } else if (session->state == TELNET_STATE_CONNECTING) {
      session->state = TELNET_STATE_IDLE;

      telnet_negotiate(session);
    } else if (session->state == TELNET_STATE_CONNECTED) {
      session->state = TELNET_STATE_RUNNING;

      telnet_begin(session);
    } else if (session->state == TELNET_STATE_RUNNING) {
      if (session->shell && !session->shell->Poll()) {
        session->state = TELNET_STATE_DISCONNECTING;
      }
    } else if (session->state == TELNET_STATE_DISCONNECTING) {
      int slot = telnet_delete(session);
      p->session[slot] = NULL;
    }
  }
}

int telnet_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool telnet_init(void) {
  if (p) return (false);

  config_init();

  if (bootup) {
    if (!config->telnet_enabled) {
      log_print(F("TLNT: telnet disabled in config\r\n"));

      config_fini();

      return (false);
    }
  }

  config_fini();

  log_print(F("TLNT: initializing telnet server\r\n"));

  p = (TELNET_PrivateData *)malloc(sizeof (TELNET_PrivateData));
  memset(p, 0, sizeof (TELNET_PrivateData));

  p->server = new WiFiServer(TELNET_PORT);
  p->server->begin();

  if (p->server->status() == CLOSED) {
    log_print(F("TLNT: could not start telnet server\r\n"));

    return (false);
  }

  return (true);
}

bool telnet_fini(void) {
  if (!p) return (false);

  log_print(F("TLNT: shutting down telnet server\r\n"));

  for (int i=0; i<TELNET_SESSIONS; i++) {
    telnet_delete(p->session[i]);
    p->session[i] = NULL;

    system_yield();
  }

  lined_history_free();

  p->server->stop();
  delete (p->server);

  // free private p->data
  free(p);
  p = NULL;

  return (true);
}

void telnet_poll(void) {
  if (!p) return;

  for (int i=0; i<TELNET_SESSIONS; i++) {
    telnet_t *session = p->session[i];

    if (session && session->client) {
      if (session->client.connected()) {
        // poll telnet session and shell
        telnet_process(session);
      } else {
        // clean up closed session
        telnet_delete(session);
        p->session[i] = NULL;
      }

      system_yield();
    }
  }

  // check if there is a new client
  if (p->server->hasClient()) {
    WiFiClient c = p->server->available();
    uint16_t port = c.remotePort();
    IPAddress ip = c.remoteIP();
    int slot = -1;

    // search for free slot in the session pool
    for (int i=0; i<TELNET_SESSIONS; i++) {
      if (!p->session[i]) {
        slot = i;
        break;
      }
    }

    if (slot == -1) { // no free slot found
      log_print(F("TLNT: rejecting new connection from %s:%i\r\n"),
        ip.toString().c_str(), port
      );

      // reject new client connection
      c.stop();
    } else {
      log_print(F("TLNT: client [%i] connected from %s:%i\r\n"),
        slot, ip.toString().c_str(), port
      );

      // accept new client connection
      p->session[slot] = telnet_new(c, slot);
    }
  }
}

MODULE(telnet)
