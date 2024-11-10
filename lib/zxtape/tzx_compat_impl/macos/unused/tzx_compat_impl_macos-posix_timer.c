

#include <pthread.h>
#include <time.h>

#include "../../../../include/tzx_compat_impl.h"
#include "posix_timer_macos.h"

// Real-time threads, see
// - https://developer.apple.com/library/archive/technotes/tn2169/_index.html
// -
// https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html#//apple_ref/doc/uid/TP30000905-CH211-BABHGEFA
// - https://chromium.googlesource.com/chromium/src/+/refs/heads/main/base/threading/platform_thread_apple.mm <== GOOD

#define AUDIO_THREAD_IDLE_SLEEP_NS 1000000  // 1ms

/* Forward declarations */
void onTimer();
// void createAudioThread(pthread_t thread);
// void destroyAudioThread(pthread_t thread);
// void *audioThread(void *arg);

/* Local variables */
static pthread_t g_audioThread = NULL;
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

  // Create the timer

  g_bAudioTimerRunning = false;
  g_nAudioTimerPeriodNs = 0;

  // Event
  g_audioTimerEvent.sigev_notify = SIGEV_THREAD;
  g_audioTimerEvent.sigev_notify_function = onTimer;
  g_audioTimerEvent.sigev_notify_attributes = NULL;
  g_audioTimerEvent.sigev_value.sival_ptr = NULL;
  g_audioTimerEvent.sigev_value.sival_int = 0;

  // Spec
  g_audioTimerSpec.it_interval.tv_sec = 0;
  g_audioTimerSpec.it_interval.tv_nsec = 0;
  g_audioTimerSpec.it_value.tv_sec = 0;
  g_audioTimerSpec.it_value.tv_nsec = 0;

  // Create
  int res = timer_create(CLOCK_REALTIME, &g_audioTimerEvent, &g_audioTimer);
  assert(res == 0);
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

  // Nothing else to do
}

void TZXCompat_timerStart(unsigned long periodUs) {
  // Stop the timer if it is running
  TZXCompat_timerStop();

  // Start the timer for a period in microseconds
  g_nAudioTimerPeriodNs = (uint64_t)periodUs * 1000ul;

  // Configure and start the timer
  g_audioTimerSpec.it_value.tv_sec = g_nAudioTimerPeriodNs / NSEC_PER_SEC;
  g_audioTimerSpec.it_value.tv_nsec = g_nAudioTimerPeriodNs % NSEC_PER_SEC;

  timer_settime(g_audioTimer, 0, &g_audioTimerSpec, NULL);

  g_bAudioTimerRunning = true;
}

void TZXCompat_timerStop(void) {
  // Stop the timer
  g_bAudioTimerRunning = false;
  g_nAudioTimerPeriodNs = 0;

  g_audioTimerSpec.it_value.tv_sec = 0;
  g_audioTimerSpec.it_value.tv_nsec = 0;

  timer_settime(g_audioTimer, 0, &g_audioTimerSpec, NULL);
}

void onTimer() {
  // Fire the timer event in the TZXCompat layer
  TZXCompat_waveOrBuffer();
}

unsigned int TZXCompat_getTickMs(void) {
  // Get the current timer value in milliseconds
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);

  unsigned int ms = (unsigned int)((spec.tv_sec * 1000) + (spec.tv_nsec / NSEC_PER_SEC));

  return ms;
}

// TODO - not to be implemetned but to be called on timer interrupt
// void TZXCompat_waveOrBuffer(bool bBuffer, unsigned int nBufferLen, unsigned long nBufferPeriodUs) {
//   // Function to call on Timer interrupt
// }

// Set the GPIO output pin low
void TZXCompat_setAudioLow() {
  // TZXCompat_log("LowWrite");
  // pZxTape->wave_set_low();
  // m_GpioOutputPin.Write(LOW);

  int loops = g_nAudioTimerPeriodNs / 10000;
  for (int i = 0; i < loops; i++) {
    printf("_");
  }
  printf("%d\n", g_nAudioTimerPeriodNs);
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
  // Not necessary as timer is in same thread as the main zxtape thread
}

/**
 * Re-enable interrupts
 */
void TZXCompat_interrupts() {
  // Not necessary as timer is in same thread as the main zxtape thread
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

// void createAudioThread(pthread_t thread) {
//   if (g_bAudioThreadRunning) return;

//   g_bAudioThreadRunning = true;

//   int res = pthread_create(&thread, NULL, audioThread, NULL);
//   assert(res == 0);

//   // Join the thread - no, as we want to run the tape in the background
//   // pthread_join(thread, NULL);

//   // Set the thread priority for realtime audio
//   setPriorityRealtimeAudio();
// }

// void destroyAudioThread(pthread_t thread) {
//   if (!g_bAudioThreadRunning) return;

//   g_bAudioThreadRunning = false;

//   // Cancel the thread
//   pthread_cancel(thread);

//   // Join the thread
//   pthread_join(thread, NULL);
// }

// void *audioThread(void *arg) {
//   while (g_bAudioThreadRunning) {
//     if (g_bAudioTimerRunning) {
//       // Wait for the timer period
//       struct timespec ts;
//       ts.tv_sec = g_nAudioTimerPeriodNs / NSEC_PER_SEC;
//       ts.tv_nsec = g_nAudioTimerPeriodNs % NSEC_PER_SEC;
//       nanosleep(&ts, NULL);

//       if (!g_bAudioThreadRunning) break;  // Check if the thread is still running

//       // Call the timer function
//       if (g_bAudioTimerRunning) {
//         onTimer();
//       }

//       // Reset the timer
//       g_bAudioTimerRunning = false;
//       g_nAudioTimerPeriodNs = 0;
//     } else {
//       // Wait for the 'AUDIO_THREAD_IDLE_SLEEP_NS' period
//       struct timespec ts;
//       ts.tv_sec = AUDIO_THREAD_IDLE_SLEEP_NS / NSEC_PER_SEC;
//       ts.tv_nsec = AUDIO_THREAD_IDLE_SLEEP_NS % NSEC_PER_SEC;
//       nanosleep(&ts, NULL);
//     }
//   }

//   return (void *)0;
// }
