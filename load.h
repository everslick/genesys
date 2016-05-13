#ifndef _LOAD_H_
#define _LOAD_H_

#ifdef __cplusplus
extern "C" {
#endif

void register_out_of_memory_cb(void (*cb)(void));

#ifdef __cplusplus
}
#endif

#endif // _LOAD_H_
