
#ifndef _posix_timer_macos_h_
#define _posix_timer_macos_h_

#include <dispatch/dispatch.h>
#include <time.h>

/* If used a lot, queue should probably be outside of this struct */
struct macos_timer {
  unsigned int running;
  dispatch_queue_t tim_queue;
  dispatch_source_t tim_timer;
  void (*tim_func)(union sigval);
  void *tim_arg;
};

typedef struct macos_timer *macos_timer_t;

struct itimerspec {
  struct timespec it_interval; /* timer period */
  struct timespec it_value;    /* timer expiration */
};

int timer_create(clockid_t clockid, struct sigevent *sevp, macos_timer_t *timerid);
int timer_delete(macos_timer_t tim);
int timer_settime(macos_timer_t tim, int flags, const struct itimerspec *its, struct itimerspec *remainvalue);

#endif  // _posix_timer_macos_h_