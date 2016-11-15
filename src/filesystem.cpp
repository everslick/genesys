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

#include "spiffs_api.h"
#include "FS.h"

#include "module.h"
#include "log.h"

#include "filesystem.h"

extern "C" uint32_t _SPIFFS_start;
extern "C" uint32_t _SPIFFS_end;
extern "C" uint32_t _SPIFFS_page;
extern "C" uint32_t _SPIFFS_block;

FS *rootfs = NULL;

static bool filesystem_is_mounted(void) {
  if (!rootfs) {
    log_print(F("FS:   SPIFFS not mounted\r\n"));
  }

  return (rootfs != NULL);
}

String fs_format_bytes(size_t bytes) {
  if (bytes < 1024) {
    return (String(bytes) + F("B"));
  }

  return (String(bytes / 1024.0) + F("KB"));
}

void fs_format(void) {
  if (!filesystem_is_mounted()) return;

  if (rootfs->format()) {
    FSInfo info;

    rootfs->info(info);

    log_print(F("FS:   SPIFFS formatted (size=%s)\r\n"),
      fs_format_bytes(info.totalBytes).c_str()
    );
  } else {
    log_print(F("FS:   could not format SPIFFS\r\n"));
  }
}

void fs_df(String &str) {
  if (!filesystem_is_mounted()) return;

  char buf[128];
  FSInfo info;

  rootfs->info(info);
  str = F("Filesystem      Size       Used      Avail   Use%   Mounted on\r\n");
  snprintf_P(buf, sizeof (buf),
     PSTR("spiffs    %10s %10s %10s    %2i%%   /\r\n"),
     fs_format_bytes(info.totalBytes).c_str(),
     fs_format_bytes(info.usedBytes).c_str(),
     fs_format_bytes(info.totalBytes - info.usedBytes).c_str(),
     (int)(((float)info.usedBytes / (float)info.totalBytes) * 100)
  );
  str += buf;
}

void fs_ls(String &str) {
  if (!filesystem_is_mounted()) return;

  Dir dir = rootfs->openDir("");

  str = "";
  while (dir.next()) {
    str += dir.fileName();
    str += F(" (");
    str += fs_format_bytes(dir.fileSize());
    str += F(")\r\n");
  }
}

void fs_mv(const String &from, const String &to) {
  if (!filesystem_is_mounted()) return;

  if (!rootfs->rename(from, to)) {
    log_print(F("FS:   cannot mv file '%s'\r\n"), from.c_str());
  }
}

void fs_rm(const String &path) {
  if (!filesystem_is_mounted()) return;

  if (!rootfs->remove(path)) {
    log_print(F("FS:   cannot remove file '%s'\r\n"), path.c_str());
  }
}

int fs_state(void) {
  if (rootfs) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool fs_init(void) {
  uint32_t ota_size, ota_start, ota_end, start, size, page, block;
  uint32_t used = ESP.getSketchSize();
  FS *fs = NULL;

  if (rootfs) return (false);

  size   = (uint32_t)(&_SPIFFS_end) - (uint32_t)(&_SPIFFS_start);
  start  = (uint32_t) &_SPIFFS_start - 0x40200000;
  page   = (uint32_t) &_SPIFFS_page;
  block  = (uint32_t) &_SPIFFS_block;

  ota_start = (used + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));
  ota_end   = (uint32_t) &_SPIFFS_start - 0x40200000;
  ota_size  = ota_end - ota_start;

  if (ota_size > size) {
    log_print(F("FS:   using OTA flash for SPIFFS (%s vs %s)\r\n"),
      fs_format_bytes(ota_size).c_str(),
      fs_format_bytes(size).c_str()
    );

    size = ota_size;
    start = ota_start;
  }

  fs = new FS(
    fs::FSImplPtr(
      new SPIFFSImpl(start, size, page, block, 5) // 5 = max num open files
    )
  );

  if (fs->begin()) {
    FSInfo info;

    fs->info(info);

    log_print(F("FS:   SPIFFS mounted (total=%s, used=%s)\r\n"),
      fs_format_bytes(info.totalBytes).c_str(),
      fs_format_bytes(info.usedBytes).c_str()
    );

    rootfs = fs;

    return (true);
  }

  log_print(F("FS:   failed to mount SPIFFS\r\n"));

  fs->end();
  delete (fs);

  return (false);
}

bool fs_fini(void) {
  if (!rootfs) return (false);

  log_print(F("FS:   unmounting SPIFFS\r\n"));

  rootfs->end();
  delete (rootfs);
  rootfs = NULL;

  return (true);
}

MODULE(fs)
