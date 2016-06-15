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

#include "FS.h"

#include "log.h"

static bool mounted = false;

static bool filesystem_is_mounted(void) {
  if (!mounted) {
    log_print(F("FS:   SPIFFS not mounted\n"));
  }

  return (mounted);
}

void fs_ls(String &str) {
  if (!filesystem_is_mounted()) return;

  Dir dir = SPIFFS.openDir("");

  str = "";
  while (dir.next()) {
    str += dir.fileName();
    str += " (";
    str += dir.fileSize();
    str += ")\n";
  }
}

void fs_cat(const String &path, String &str) {
  if (!filesystem_is_mounted()) return;

  File f = SPIFFS.open(path, "r");

  if (!f) {
    log_print(F("FS:   file not found: %s\n"), path.c_str());

    return;
  } else {
    while (f.available()) {
      str += f.readString();
    }
  }

  f.close();
}

void fs_rm(const String &path) {
  if (!filesystem_is_mounted()) return;

  if (!SPIFFS.remove(path)) {
    log_print(F("FS:   file not found: %s\n"), path.c_str());
  }
}

bool fs_init(void) {
  if (SPIFFS.begin()) {
    FSInfo info;

    SPIFFS.info(info);

    log_print(F("FS:   SPIFFS mounted (total=%i, used=%i)\n"),
      info.totalBytes, info.usedBytes
    );

    mounted = true;

    return (true);
  }

  log_print(F("FS:   failed to mount SPIFFS\n"));

  return (false);
}
