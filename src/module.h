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

#ifndef _MODULE_H_
#define _MODULE_H_

#include <Arduino.h>

#ifdef DEBUG
#define MODULE(_MOD_)                                              \
static const char *_MOD_ ## _name = #_MOD_;                        \
                                                                   \
void _MOD_ ## _interface(ModuleInterface &interface) {             \
  interface.state = _MOD_ ## _state;                               \
  interface.init  = _MOD_ ## _init;                                \
  interface.fini  = _MOD_ ## _fini;                                \
  interface.mem   = NULL;                                          \
}                                                                  \
                                                                   \
Module _MOD_ ## _module = { _MOD_ ## _name, _MOD_ ## _interface }; \
                                                                   \
static bool reg = module_register(_MOD_ ## _module);
#else
#define MODULE(_MOD_)
#endif

enum ModuleState {
  MODULE_STATE_UNKNOWN,
  MODULE_STATE_ACTIVE,
  MODULE_STATE_INACTIVE
};

struct ModuleInterface {
  int (*state)(void);
  bool (*init)(void);
  bool (*fini)(void);
  int (*mem)(void);
};

struct Module {
  const char *name;
  void (*interface)(ModuleInterface &);
};

bool module_register(const Module &module);

bool module_call_state(const String &module, int &state);
bool module_call_init(const String &module, bool &ret);
bool module_call_fini(const String &module, bool &ret);

bool module_call_state(int idx, int &state);
bool module_call_init(int idx, bool &ret);
bool module_call_fini(int idx, bool &ret);

String module_state_str(int state);
String module_name(int idx);
int module_count(void);

#endif // _MODULE_H_
