#ifndef _NTP_H_
#define _NTP_H_

int ntp_gettime(struct timespec *tp);
int ntp_settime(void);

bool ntp_init(void);
bool ntp_poll(void);

#endif // _NTP_H_
