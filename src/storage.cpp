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

#include "filesystem.h"
#include "datetime.h"
#include "config.h"
#include "system.h"
#include "module.h"
#include "clock.h"
#include "util.h"
#include "rtc.h"
#include "log.h"

#include "storage.h"

struct CSV_PrivateData {
  // settings from config
  uint32_t storage_interval;
  uint32_t storage_mask;
};

static File f;

static CSV_PrivateData *p = NULL;

enum {
  STORAGE_MASK_ADC  = 1<<0,
  STORAGE_MASK_TEMP = 1<<1,
};

static void write_header(void) {
  uint32_t mask = p->storage_mask;
  String head;

  if (!head.reserve(128)) {
    log_print(F("CSV:  failed to allocate header buffer"));
  }

  // make header line
  head += F("timestamp;localtime;");
  if (mask & STORAGE_MASK_ADC)  head += F("adc;");
  if (mask & STORAGE_MASK_TEMP) head += F("temp;");

  // replace the last ';' with '\r' and append '\n'
  head[head.length() - 1] = '\r'; head += F("\n");

  if (f) {
    f.print(head);
    f.flush();
  } else {
    log_print(F("CSV:  cannot write header to file"));
  }
}

static void open_file(void) {
  if (rootfs) {
    String file = system_device_name() + String(".csv");
    bool header = !rootfs->exists(file);

    f = rootfs->open(file, "a");

    if (f) {
      if (header) {
        log_print(F("CSV:  created storage file '%s'"), file.c_str());

        write_header();
      }
    } else {
      log_print(F("CSV:  failed to open file '%s'"), file.c_str());
    }
  }
}

static void append_values(void) {
  uint32_t mask = p->storage_mask;
  String csv;

  if (!csv.reserve(256)) {
    log_print(F("CSV:  failed to allocate line buffer"));
  }

  // make CSV line
  DateTime dt(clock_time());
  dt.ConvertToLocalTime();
  csv += String(clock_time())   + F(";"); // timestamp
  csv += dt.str()               + F(";"); // localtime

  if (mask & STORAGE_MASK_ADC)  csv += String(analogRead(17)) + F(";");
  if (mask & STORAGE_MASK_TEMP) csv += float2str(rtc_temp())  + F(";");

  // replace the last ';' with '\r' and append '\n'
  csv[csv.length() - 1] = '\r'; csv += F("\n");

  if (!f) open_file();

  if (f) {
    f.print(csv);
    f.flush();
  } else {
    log_print(F("CSV:  cannot write data to file"));
  }
}

int storage_state(void) {
  if (p) return (MODULE_STATE_ACTIVE);

  return (MODULE_STATE_INACTIVE);
}

bool storage_init(void) {
  if (p) return (false);

  config_init();
  fs_init();

  if (bootup && !config->storage_enabled) {
    log_print(F("CSV:  local storage disabled in config"));

    //fs_fini(); XXX log may still need the FS (TODO impl. ref-count)
    config_fini();

    return (false);
  }

  log_print(F("CSV:  initializing local storage"));

  p = (CSV_PrivateData *)malloc(sizeof (CSV_PrivateData));
  memset(p, 0, sizeof (CSV_PrivateData));

  p->storage_interval = config->storage_interval;
  p->storage_mask     = config->storage_mask;

  config_fini();

  return (true);
}

bool storage_fini(void) {
  if (!p) return (false);

  log_print(F("CVS:  disabling local file storage"));

  // close file
  f.close();

  // free private data
  free(p);
  p = NULL;

  return (true);
}

void storage_poll(void) {
  static int last_time_minute = -1;
  static uint32_t ms = millis();

  if (p && ((millis() - ms) > 500)) {
    int minute = (clock_time() % 3600) / 60; // 3600 secs per minute

    ms = millis();

    if (f) {
      if (rootfs) {
        // check if the file has been deleted
        if (!f.seek(0, SeekCur)) {
          log_print(F("CSV:  file was deleted, closing it"));

          f.close();
        }
      } else {
        // check if FS is still mounted
        log_print(F("CSV:  filesystem was unmounted, closing file"));

        f.close();
      }
    }

    if ((minute != last_time_minute) && ((minute % p->storage_interval) == 0)) {
      // write values to file
      append_values();

      // remember minutes for next round
      last_time_minute = minute;
    }
  }
}

MODULE(storage)
