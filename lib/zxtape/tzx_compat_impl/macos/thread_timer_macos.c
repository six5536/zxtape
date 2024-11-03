/**
 * Implementation of the POSIX timer for macOS using a thread
 *
 * The following timer code is based on a Gist by Jorgen Lundman:
 *
 * https://gist.github.com/lundman/731d0d7d09eca072cd1224adb00d9b9e
 */

#include "thread_timer_macos.h"

#include <assert.h>
#include <mach/boolean.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/semaphore.h>
#include <mach/thread_policy.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/errno.h>

/* Forward declarations */
static inline void _timer_start(macos_timer_t tim, const struct itimerspec *its, struct itimerspec *remainvalue);
static inline void _timer_cancel(macos_timer_t tim, const struct itimerspec *its, struct itimerspec *remainvalue);
static inline void _timer_handler(void *arg);
static void _timer_create_thread(pthread_t thread, macos_timer_t tim);
static void _timer_destroy_thread(pthread_t thread, macos_timer_t tim);
static void *_timer_thread(void *arg);
static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint, boolean_t preemptible);
static int setPriorityRealtimeAudio();

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
      semaphore_create(mach_task_self(), &tim->startSem, SYNC_POLICY_FIFO, 0);
      semaphore_create(mach_task_self(), &tim->timerSem, SYNC_POLICY_FIFO, 0);
      pthread_mutexattr_init(&tim->mutexAttr);
      pthread_mutexattr_settype(&tim->mutexAttr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init(&tim->mutex, &tim->mutexAttr);

      tim->tim_func = sevp->sigev_notify_function;
      tim->tim_arg = sevp->sigev_value.sival_ptr;
      *timerid = tim;

      // Create the thread
      _timer_create_thread(tim->thread, tim);

      return (0);
    default:
      break;
  }

  errno = EINVAL;
  return (-1);
}

inline int timer_delete(macos_timer_t tim) {
  /* Calls _timer_cancel() */
  if (tim != NULL) {
    // Cancel any running timer
    _timer_cancel(tim, NULL, NULL);

    // Destroy the thread
    _timer_destroy_thread(tim->thread, tim);

    // Destroy the semaphores
    semaphore_destroy(mach_task_self(), tim->startSem);
    semaphore_destroy(mach_task_self(), tim->timerSem);

    // Destroy the mutex
    pthread_mutex_destroy(&tim->mutex);

    // Free the timer
    free(tim);
  }

  return (0);
}

inline int timer_settime(macos_timer_t tim, int flags, const struct itimerspec *its, struct itimerspec *remainvalue) {
  if (tim != NULL) {
    /* Both zero, is disarm */
    if (its->it_value.tv_sec == 0 && its->it_value.tv_nsec == 0) {
      _timer_cancel(tim, its, remainvalue);
      return (0);
    }

    _timer_start(tim, its, remainvalue);
  }
  return (0);
}

static inline void _timer_start(macos_timer_t tim, const struct itimerspec *its, struct itimerspec *remainvalue) {
  // Lock the mutex
  pthread_mutex_lock(&tim->mutex);

  // Cancel the timer if it is running
  _timer_cancel(tim, its, remainvalue);

  // Start the timer
  tim->period = NSEC_PER_SEC * its->it_value.tv_sec + its->it_value.tv_nsec;
  tim->running = 1;
  semaphore_signal(tim->startSem);

  // Release the mutex
  pthread_mutex_unlock(&tim->mutex);
}

static inline void _timer_cancel(macos_timer_t tim, const struct itimerspec *its, struct itimerspec *remainvalue) {
  // Lock the mutex
  pthread_mutex_lock(&tim->mutex);

  if (tim->running) {
    // Stop the timer
    tim->running = 0;  // How do we know if the timer is running?
    semaphore_signal(tim->timerSem);

    // Call the cancel callback
  }

  // Release the mutex
  pthread_mutex_unlock(&tim->mutex);
}

static inline void _timer_handler(void *arg) { struct macos_timer *tim = (struct macos_timer *)arg; }

static void _timer_create_thread(pthread_t thread, macos_timer_t tim) {
  if (tim->threadRunning) return;

  tim->threadRunning = true;

  // Enable SIGUSR1 signal for the audio thread
  // sigemptyset(&g_audioThreadSignalMask);
  // sigaddset(&g_audioThreadSignalMask, SIGUSR1);
  // pthread_sigmask(SIG_UNBLOCK, &g_audioThreadSignalMask, NULL);

  int res = pthread_create(&thread, NULL, _timer_thread, tim);
  assert(res == 0);

  // Join the thread - no, as we want to run the timer in the background
  // pthread_join(thread, NULL);

  // Set the thread priority for realtime audio
  // setPriorityRealtimeAudio();
}

static void _timer_destroy_thread(pthread_t thread, macos_timer_t tim) {
  if (!tim->threadRunning) return;

  tim->threadRunning = false;

  // Signal the start semaphore in order to allow the thread to exit
  semaphore_signal(tim->startSem);

  // Cancel the thread
  pthread_cancel(thread);

  // Join the thread
  pthread_join(thread, NULL);
}

static void *_timer_thread(void *arg) {
  struct macos_timer *tim = (struct macos_timer *)arg;
  int res = 0;
  union sigval sv;

  while (tim->threadRunning) {
    // Wait for the start semaphore
    semaphore_wait(tim->startSem);
    if (!tim->threadRunning) break;  // Check if the thread is still running

    // Wait for the timer period
    pthread_mutex_lock(&tim->mutex);  // Lock the mutex
    if (tim->period > 0) {
      struct mach_timespec ts;
      ts.tv_sec = tim->period / NSEC_PER_SEC;
      ts.tv_nsec = tim->period % NSEC_PER_SEC;

      pthread_mutex_unlock(&tim->mutex);  // Unlock the mutex for the wait
      res = semaphore_timedwait(tim->timerSem, ts);
    } else {
      pthread_mutex_unlock(&tim->mutex);  // Unlock the mutex
    }

    if (!tim->threadRunning) break;  // Check if the thread is still running

    // Call the timer function
    pthread_mutex_lock(&tim->mutex);  // Lock the mutex after the wait
    if (tim->running && tim->threadRunning && res == KERN_OPERATION_TIMED_OUT) {
      tim->running = 0;
      sv.sival_ptr = tim->tim_arg;
      if (tim->tim_func != NULL) tim->tim_func(sv);
    } else {
      // Timer was cancelled
      tim->running = 0;
    }
    pthread_mutex_unlock(&tim->mutex);  // Unlock the mutex
  }

  return (void *)0;
}

static int setPriorityRealtimeAudio() {
  // Set the thread priority to realtime

  // Code based on information in
  // https://chromium.googlesource.com/chromium/src/+/refs/heads/main/base/threading/platform_thread_apple.mm

  thread_time_constraint_policy_data_t time_constraints;
  mach_timebase_info_data_t tb_info;
  mach_timebase_info(&tb_info);

  // Empirical configuration.
  // Define the guaranteed and max fraction of time for the audio thread.
  // These "duty cycle" values can range from 0 to 1.  A value of 0.5
  // means the scheduler would give half the time to the thread.
  // These values have empirically been found to yield good behavior.
  // Good means that audio performance is high and other threads won't starve.
  const double kGuaranteedAudioDutyCycle = 0.75;
  const double kMaxAudioDutyCycle = 0.85;
  // Define constants determining how much time the audio thread can
  // use in a given time quantum.  All times are in milliseconds.
  // About 128 frames @44.1KHz
  const double kTimeQuantum = 2.9;
  // Time guaranteed each quantum.
  const double kAudioTimeNeeded = kGuaranteedAudioDutyCycle * kTimeQuantum;
  // Maximum time each quantum.
  const double kMaxTimeAllowed = kMaxAudioDutyCycle * kTimeQuantum;
  // Get the conversion factor from milliseconds to absolute time
  // which is what the time-constraints call needs.
  double ms_to_abs_time = (double)(tb_info.denom) / tb_info.numer * 1000000.0f;
  time_constraints.period = kTimeQuantum * ms_to_abs_time;
  time_constraints.computation = kAudioTimeNeeded * ms_to_abs_time;
  time_constraints.constraint = kMaxTimeAllowed * ms_to_abs_time;
  time_constraints.preemptible = 0;

  return setRealtime(time_constraints.period, time_constraints.computation, time_constraints.constraint,
                     time_constraints.preemptible);
}

static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint, boolean_t preemptible) {
  struct thread_time_constraint_policy ttcpolicy;
  int ret;
  thread_port_t threadport = pthread_mach_thread_np(pthread_self());

  ttcpolicy.period = period;
  ttcpolicy.computation = computation;
  ttcpolicy.constraint = constraint;
  ttcpolicy.preemptible = preemptible;

  ret = thread_policy_set(threadport, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&ttcpolicy,
                          THREAD_TIME_CONSTRAINT_POLICY_COUNT);

  return ret;
}