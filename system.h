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

extern char client_id[];

bool system_init(void);
bool system_poll(void);

void system_delay(uint32_t ms);
void system_yield(void);
void system_idle(void);

void system_reboot(void);

uint32_t system_free_heap(void);
uint32_t system_free_stack(void);

bool system_stack_corrupt(void);

uint32_t system_sketch_size(void);
uint32_t system_free_sketch_space(void);

uint8_t system_cpu_load(void);
uint8_t system_mem_usage(void);
uint8_t system_net_traffic(void);

void system_count_net_traffic(uint32_t bytes);

SysLoad &system_load_history(uint16_t index);
uint16_t system_load_history_entries(void);

char *system_uptime(char buf[]);
char *system_time(char buf[], time_t time = INT_MAX);

void system_device_info(String &str);
void system_sys_info(String &str);
void system_net_info(String &str);
void system_ap_info(String &str);
void system_wifi_info(String &str);

#endif // _SYSTEM_H_
