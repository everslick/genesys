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

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <Arduino.h>
#include <limits.h>

typedef struct SysLoad {
  uint8_t cpu;
  uint8_t mem;
  uint8_t net;
} SysLoad;

extern char device_id[];
extern char device_name[];

extern bool bootup;

bool system_init(void);
bool system_fini(void);
void system_poll(void);

void system_yield(void);
void system_reboot(void);

const String system_device_name(void);

const String system_hw_device(void);
const String system_hw_version(void);
const String system_fw_version(void);
const String system_fw_build(void);

uint32_t system_net_xfer(void);
uint32_t system_main_loops(void);
uint32_t system_mem_free(void);

bool system_turbo_get(void);
bool system_turbo_set(bool on);

uint32_t system_free_heap(void);
uint32_t system_free_stack(void);

bool system_stack_corrupt(void);

uint32_t system_sketch_size(void);
uint32_t system_free_sketch_space(void);

uint8_t system_cpu_load(void);
uint8_t system_mem_usage(void);
uint8_t system_net_traffic(void);

SysLoad &system_load_history(uint16_t index);
uint16_t system_load_history_entries(void);

char *system_uptime(char buf[]);
char *system_time(char buf[], time_t time = INT_MAX);

void system_device_info(String &str);
void system_version_info(String &str);
void system_build_info(String &str);
void system_sys_info(String &str);
void system_load_info(String &str);
void system_flash_info(String &str);
void system_net_info(String &str);
void system_ap_info(String &str);
void system_wifi_info(String &str);

#endif // _SYSTEM_H_
