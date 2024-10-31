

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "../../../../include/tzx_compat_impl.h"
#include "posix_timer_macos.h"

// Real-time threads, see
// - https://developer.apple.com/library/archive/technotes/tn2169/_index.html
// -
// https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html#//apple_ref/doc/uid/TP30000905-CH211-BABHGEFA
// - https://chromium.googlesource.com/chromium/src/+/refs/heads/main/base/threading/platform_thread_apple.mm <== GOOD

#define AUDIO_THREAD_IDLE_SLEEP_NS NSEC_PER_SEC * 60 * 60 * 24 * 365  // 1 year

/* Forward declarations */
static void onTimer();
static void createAudioThread(pthread_t thread);
static void destroyAudioThread(pthread_t thread);
static void *audioThread(void *arg);
static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint);
static int setPriorityRealtimeAudio();

/* Local variables */
static pthread_t g_audioThread = NULL;
static sigset_t g_audioThreadSignalMask;
static macos_timer_t g_audioTimer = NULL;
static struct sigevent g_audioTimerEvent;
static struct itimerspec g_audioTimerSpec;
static volatile bool g_bAudioThreadRunning = false;
static volatile bool g_bAudioTimerRunning = false;
static volatile uint64_t g_nAudioTimerPeriodNs = 0;

//
// TZX Compat Implemetation
//

void TZXCompat_create(void) {
  //
  g_bAudioTimerRunning = false;
  g_nAudioTimerPeriodNs = 0;

  createAudioThread(g_audioThread);
}

void TZXCompat_start(void) {
  // Set GPIO pin to output mode (ensuring it is LOW)
  // m_GpioOutputPin.Write(LOW);
  // m_GpioOutputPin.SetMode(GPIOModeOutput);
}

void TZXCompat_stop(void) {
  // Set GPIO pin to input mode
  // m_GpioOutputPin.SetMode(GPIOModeInput);
}

void TZXCompat_timerInitialize(void) {
  // Initialise / reset the timer

  // Stop the timer if it is running
  TZXCompat_timerStop();
}

void TZXCompat_timerStart(unsigned long periodUs) {
  // Stop the timer if it is running
  TZXCompat_timerStop();

  // Start the timer for a period in microseconds
  g_nAudioTimerPeriodNs = (uint64_t)periodUs * 1000ul;
  g_bAudioTimerRunning = true;

  // Signal the timer to cancel any current sleep
  pthread_kill(g_audioThread, SIGTERM);
}

void TZXCompat_timerStop(void) {
  // Stop the timer
  g_bAudioTimerRunning = false;
  g_nAudioTimerPeriodNs = 0;

  // Signal the timer to cancel any current sleep
  pthread_kill(g_audioThread, SIGINT);
}

static void onTimer() {
  // Fire the timer event in the TZXCompat layer
  TZXCompat_onTimer();
}

unsigned int TZXCompat_getTickMs(void) {
  // Get the current timer value in milliseconds
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);

  unsigned int ms = (unsigned int)((spec.tv_sec * 1000) + (spec.tv_nsec / NSEC_PER_SEC));

  return ms;
}

// TODO - not to be implemetned but to be called on timer interrupt
// void TZXCompat_onTimer(void) {
//   // Function to call on Timer interrupt
// }

// Set the GPIO output pin low
void TZXCompat_setAudioLow() {
  // TZXCompat_log("LowWrite");
  // pZxTape->wave_set_low();
  // m_GpioOutputPin.Write(LOW);

  // int loops = g_nAudioTimerPeriodNs / 10000;
  // for (int i = 0; i < loops; i++) {
  //   printf("_");
  // }
  // printf("%d\n", g_nAudioTimerPeriodNs);
}

// Set the GPIO output pin high
void TZXCompat_setAudioHigh() {
  // TZXCompat_log("HighWrite");
  // pZxTape->wave_set_high();
  // m_GpioOutputPin.Write(HIGH);

  // int loops = g_nAudioTimerPeriodNs / 10000;
  // for (int i = 0; i < loops; i++) {
  //   printf("-");
  // }
  // printf("%d\n", g_nAudioTimerPeriodNs);
}

/**
 * Delay a number of milliseconds in a busy loop
 */
void TZXCompat_delay(unsigned long ms) {
  //
}

/**
 * Disable interrupts
 */
void TZXCompat_noInterrupts() {
  // Not necessary as timer is in same thread as the main zxtape thread? NO!?
}

/**
 * Re-enable interrupts
 */
void TZXCompat_interrupts() {
  // Not necessary as timer is in same thread as the main zxtape thread? NO!?
}

//
// File API
//

unsigned char TZXCompat_fileOpen(void *dir, unsigned int index, unsigned oflag) {
  // TODO: Implement
  // char* pF = TZX_fileName; < Must be full path

  // Must set TZX_filesize

  return 1;
}

void TZXCompat_fileClose() {
  // TODO: Implement
}

int TZXCompat_fileRead(void *buf, unsigned long count) {
  // TODO: Implement
  return 0;
}

unsigned char TZXCompat_fileSeekSet(unsigned long long pos) {
  // TODO: Implement

  return 1;
}

//
// Log functions
//

// Log a TZX message
void TZXCompat_log(const char *pFormat, ...) {
  va_list args;

  // Log a message
  va_start(args, pFormat);
  fprintf(stdout, "%s [%s] ", "TZX", "DEBUG");
  vfprintf(stdout, pFormat, args);
  fprintf(stdout, "\n");
  va_end(args);
}

// Log a zxtape message
void zxtape_log(const char *pLevel, const char *pFormat, ...) {
  va_list args;

  // Log a message
  va_start(args, pFormat);
  fprintf(stdout, "%s [%s] ", "ZxTape", pLevel);
  vfprintf(stdout, pFormat, args);
  fprintf(stdout, "\n");
  va_end(args);
}

//
// private functions
//

static void createAudioThread(pthread_t thread) {
  if (g_bAudioThreadRunning) return;

  g_bAudioThreadRunning = true;

  // Enable SIGUSR1 signal for the audio thread
  // sigemptyset(&g_audioThreadSignalMask);
  // sigaddset(&g_audioThreadSignalMask, SIGUSR1);
  // pthread_sigmask(SIG_UNBLOCK, &g_audioThreadSignalMask, NULL);

  int res = pthread_create(&thread, NULL, audioThread, NULL);
  assert(res == 0);

  // Join the thread - no, as we want to run the tape in the background
  // pthread_join(thread, NULL);

  // Set the thread priority for realtime audio
  setPriorityRealtimeAudio();
}

static void destroyAudioThread(pthread_t thread) {
  if (!g_bAudioThreadRunning) return;

  g_bAudioThreadRunning = false;

  // Cancel the thread
  pthread_cancel(thread);

  // Join the thread
  pthread_join(thread, NULL);
}

static void *audioThread(void *arg) {
  while (g_bAudioThreadRunning) {
    if (g_bAudioTimerRunning) {
      // Wait for the timer period
      struct timespec ts;
      ts.tv_sec = g_nAudioTimerPeriodNs / NSEC_PER_SEC;
      ts.tv_nsec = g_nAudioTimerPeriodNs % NSEC_PER_SEC;
      int res = nanosleep(&ts, NULL);

      if (!g_bAudioThreadRunning) break;  // Check if the thread is still running

      // Call the timer function
      if (g_bAudioTimerRunning && res == 0) {
        onTimer();
      }
    } else {
      // Wait for the 'AUDIO_THREAD_IDLE_SLEEP_NS' period
      struct timespec ts;
      ts.tv_sec = AUDIO_THREAD_IDLE_SLEEP_NS / NSEC_PER_SEC;
      ts.tv_nsec = AUDIO_THREAD_IDLE_SLEEP_NS % NSEC_PER_SEC;
      nanosleep(&ts, NULL);

      // Debug only
      ts.tv_sec = 0;
    }
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

  return setRealtime(time_constraints.period, time_constraints.computation, time_constraints.constraint);
}

static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint) {
  struct thread_time_constraint_policy ttcpolicy;
  int ret;
  thread_port_t threadport = pthread_mach_thread_np(pthread_self());

  ttcpolicy.period = period;
  ttcpolicy.computation = computation;
  ttcpolicy.constraint = constraint;
  ttcpolicy.preemptible = 1;

  ret = thread_policy_set(threadport, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&ttcpolicy,
                          THREAD_TIME_CONSTRAINT_POLICY_COUNT);

  return ret;
}