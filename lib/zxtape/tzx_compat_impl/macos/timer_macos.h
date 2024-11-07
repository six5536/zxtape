/**
 * timer_macos.h
 *
 * Timer implementation for macOS using a thread with a POSIX like interface.
 *
 */

#ifndef _timer_macos_h_
#define _timer_macos_h_

#include <mach/semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct macos_timer {
  unsigned int running;
  unsigned int threadRunning;
  pthread_t thread;
  semaphore_t startSem;
  semaphore_t timerSem;
  pthread_mutex_t mutex;
  pthread_mutexattr_t mutexAttr;
  uint64_t period;
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

#endif  // _timer_macos_h_