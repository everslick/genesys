#include <user_interface.h>
#include <stdint.h>

extern void * __real_malloc(uint32_t);
extern void * __real_calloc(uint32_t, uint32_t);
extern void * __real_realloc(void *, uint32_t);
extern void __real_free(void *);

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
