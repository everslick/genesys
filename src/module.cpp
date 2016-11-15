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

#include "module.h"

#define MAX_MODULES 16

static int modules_registered = 0;
static Module modules[MAX_MODULES];

static int module_index(const String &module) {
  for (int i=0; i<modules_registered; i++) {
    if (module == modules[i].name) return (i);
  }

  return (-1);
}

static bool module_interface(int module, ModuleInterface &interface) {
  if (module >= 0 && module < modules_registered) {
    modules[module].interface(interface);

    return (true);
  }

  return (false);
}

bool module_register(const Module &module) {
  if (modules_registered == MAX_MODULES) return (false);

  modules[modules_registered++] = module;

  return (true);
}

bool module_call_state(const String &module, int &state) {
  return (module_call_state(module_index(module), state));
}

bool module_call_init(const String &module, bool &ret) {
  return (module_call_init(module_index(module), ret));
}

bool module_call_fini(const String &module, bool &ret) {
  return (module_call_fini(module_index(module), ret));
}

bool module_call_state(int module, int &state) {
  ModuleInterface interface;

  state = MODULE_STATE_UNKNOWN;

  if (module_interface(module, interface)) {
    state = interface.state(); return (true);
  }

  return (false);
}

bool module_call_init(int module, bool &ret) {
  ModuleInterface interface;

  if (module_interface(module, interface)) {
    ret = interface.init(); return (true);
  }

  return (false);
}

bool module_call_fini(int module, bool &ret) {
  ModuleInterface interface;

  if (module_interface(module, interface)) {
    ret = interface.fini(); return (true);
  }

  return (false);
}

String module_state_str(int state) {
  if (state == MODULE_STATE_ACTIVE)   return (F("ACTIVE"));
  if (state == MODULE_STATE_INACTIVE) return (F("INACTIVE"));

  return (F("UNKNOWN"));
}

String module_name(int idx) {
  if (idx >= modules_registered) return (F("UNKNOWN"));

  return (modules[idx].name);
}

int module_count(void) {
  return (modules_registered);
}
