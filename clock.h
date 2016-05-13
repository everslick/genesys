#ifndef _CLOCK_H_
#define _CLOCK_H_

#include <sys/types.h>
#include <stdint.h>
#include <limits.h>

enum {
  CLOCK_REALTIME,
  CLOCK_MONOTONIC
};

//struct timespec {
//  time_t tv_sec; /* seconds */
//  long tv_nsec;  /* nanoseconds */
//};

struct timespec clock_subtime(struct timespec *a, struct timespec *b);
struct timespec clock_addtime(struct timespec *a, struct timespec *b);

// returns 1 if a > b, -1 if a < b, 0 if a == b
int clock_cmptime(struct timespec *a, struct timespec *b);

int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_settime(clockid_t clk_id, struct timespec *tp);

time_t clock_time(void);

#endif // _CLOCK_H_
