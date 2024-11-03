/**
 * Implementation of the POSIX timer for macOS using dispatch
 *
 * The following timer code is based on a Gist by Jorgen Lundman:
 *
 * https://gist.github.com/lundman/731d0d7d09eca072cd1224adb00d9b9e
 */

#include "posix_timer_macos.h"

#include <mach/boolean.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/errno.h>

/* Forward declarations */
static inline void _timer_handler(void *arg);
static inline void _timer_cancel(void *arg);

inline int timer_create(clockid_t clockid, struct sigevent *sevp, macos_timer_t *timerid) {
  struct macos_timer *tim;

  *timerid = NULL;

  switch (clockid) {
    case CLOCK_REALTIME:

      /* What is implemented so far */
      if (sevp->sigev_notify != SIGEV_THREAD) {
        errno = ENOTSUP;
        return (-1);
      }

      tim = (struct macos_timer *)malloc(sizeof(struct macos_timer));
      if (tim == NULL) {
        errno = ENOMEM;
        return (-1);
      }

      tim->running = 0;
      tim->tim_queue = dispatch_queue_create("org.six5536.posix_timer_macos.timerqueue", 0);
      tim->tim_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, tim->tim_queue);

      tim->tim_func = sevp->sigev_notify_function;
      tim->tim_arg = sevp->sigev_value.sival_ptr;
      *timerid = tim;

      /* Opting to use pure C instead of Block versions */
      dispatch_set_context(tim->tim_timer, tim);
      dispatch_source_set_event_handler_f(tim->tim_timer, _timer_handler);
      dispatch_source_set_cancel_handler_f(tim->tim_timer, _timer_cancel);

      return (0);
    default:
      break;
  }

  errno = EINVAL;
  return (-1);
}

inline int timer_delete(macos_timer_t tim) {
  /* Calls _timer_cancel() */
  if (tim != NULL) dispatch_source_cancel(tim->tim_timer);

  return (0);
}

inline int timer_settime(macos_timer_t tim, int flags, const struct itimerspec *its, struct itimerspec *remainvalue) {
  if (tim != NULL) {
    /* Both zero, is disarm */
    if (its->it_value.tv_sec == 0 && its->it_value.tv_nsec == 0) {
      /* There's a comment about suspend count in Apple docs */
      if (tim->running) {
        dispatch_suspend(tim->tim_timer);
        tim->running = 0;
      }
      return (0);
    }

    dispatch_time_t start;
    start = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * its->it_value.tv_sec + its->it_value.tv_nsec);
    dispatch_source_set_timer(tim->tim_timer, start, NSEC_PER_SEC * its->it_value.tv_sec + its->it_value.tv_nsec, 0);
    if (!tim->running) {
      dispatch_resume(tim->tim_timer);
      tim->running = 1;
    }
  }
  return (0);
}

static inline void _timer_cancel(void *arg) {
  struct macos_timer *tim = (struct macos_timer *)arg;
  dispatch_release(tim->tim_timer);
  dispatch_release(tim->tim_queue);
  tim->tim_timer = NULL;
  tim->tim_queue = NULL;
  tim->running = 0;

  // Timer is freed here
  free(tim);
}

static inline void _timer_handler(void *arg) {
  struct macos_timer *tim = (struct macos_timer *)arg;
  union sigval sv;

  sv.sival_ptr = tim->tim_arg;

  if (tim->tim_func != NULL) tim->tim_func(sv);
}