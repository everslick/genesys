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

#include <user_interface.h>
#include <stdint.h>

extern void *__real_malloc(uint32_t);
extern void *__real_calloc(uint32_t, uint32_t);
extern void *__real_realloc(void *, uint32_t);
extern void  __real_free(void *);

extern uint32_t mem_free, yield_count, traffic_count;

static void (*out_of_memory_cb)(void) = NULL;

static void check_pointer(void *ptr) {
  if (!ptr) {
    if (out_of_memory_cb) out_of_memory_cb();
    mem_free = 0;
  }
}

static void check_memory(void) {
  uint32_t heap = system_get_free_heap_size();

  if (heap < mem_free) mem_free = heap;
}

void register_out_of_memory_cb(void (*cb)(void)) {
  // set callback function
  out_of_memory_cb = cb;
}

void *__wrap_malloc(uint32_t size) {
  void *ptr = __real_malloc(size);

  check_pointer(ptr);
  check_memory();

  return (ptr);
}

void *__wrap_calloc(uint32_t num, uint32_t size) {
  void *ptr = __real_calloc(num, size);

  check_pointer(ptr);
  check_memory();

  return (ptr);
}

void *__wrap_realloc(void *ptr, uint32_t size) {
  ptr = __real_realloc(ptr, size);

  check_pointer(ptr);
  check_memory();

  return (ptr);
}

void __wrap_free(void *ptr) {
  __real_free(ptr);

  check_memory();
}
