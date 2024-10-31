
#include <assert.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/param.h>
#include <time.h>

#include "../../../../include/tzx_compat_impl.h"
#include "audio_macos.h"
#include "posix_timer_macos.h"

// Real-time threads, see
// - https://developer.apple.com/library/archive/technotes/tn2169/_index.html
// -
// https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html#//apple_ref/doc/uid/TP30000905-CH211-BABHGEFA
// - https://chromium.googlesource.com/chromium/src/+/refs/heads/main/base/threading/platform_thread_apple.mm <== GOOD

#define AUDIO_BUFFER_MULTIPLE 8
#define AUDIO_BUFFER_LENGTH 2048
#define TIMER_FIXED_OFFSET_US 50
#define TIMER_VARAIBLE_OFFSET_US 150

/* structs */
typedef struct AudioPinSample_ {
  uint32_t state;
  uint64_t samples;
} AudioPinSample;

/* Imported global variables */
extern uint32_t AudioPlaybackRate;
extern uint32_t AudioIntervalMs;

/* Exported global variables */
uint32_t g_pinState = 0;

/* Forward declarations */
static void onTimer();
static void transferAudioBuffer(void *buffer, unsigned int bufferSize);
static void createAudioThread(pthread_t thread);
static void destroyAudioThread(pthread_t thread);
static void *audioThread(void *arg);
static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint, boolean_t preemptible);
static int setPriorityRealtimeAudio();

/* Local variables */
static pthread_t g_audioThread = NULL;
static macos_timer_t g_audioTimer = NULL;
static struct sigevent g_audioTimerEvent;
static struct itimerspec g_audioTimerSpec;
static sem_t *g_audioSemaphore;
static volatile bool g_bAudioThreadRunning = false;
static volatile bool g_bAudioTimerRunning = false;
static volatile uint64_t g_nAudioTimerPeriodNs = 0;

static uint32_t g_audioBufferLengthMs = 0;
static uint32_t g_audioBufferLength = 0;
static uint32_t g_audioBufferReadIndex = 0;
static uint32_t g_audioBufferWriteIndex = 0;
static AudioPinSample *g_audioBuffer = NULL;
static int8_t g_audioBufferLastValue = 0;
static bool g_audioBufferReady = false;

//
// TZX Compat Implemetation
//

void TZXCompat_create(void) {
  //
  g_bAudioTimerRunning = false;
  g_nAudioTimerPeriodNs = 0;

  // Allocate the audio buffer
  // g_audioBufferLengthMs = AudioIntervalMs * AUDIO_BUFFER_MULTIPLE;
  // g_audioBufferLength = g_audioBufferLengthMs * AudioPlaybackRate / 1000;
  g_audioBufferLength = AUDIO_BUFFER_LENGTH;
  g_audioBuffer = (AudioPinSample *)malloc(g_audioBufferLength * sizeof(AudioPinSample));  // Freed in TZXCompat_destroy
  assert(g_audioBuffer != NULL);
  g_audioBufferReadIndex = 0;
  g_audioBufferWriteIndex = 0;
  g_audioBufferLastValue = 0;

  // Initialise MACOS audio
  InitMacSound(transferAudioBuffer);
  MacStartSound();

  // Create the audio thread
  createAudioThread(g_audioThread);
}

void TZXCompat_destroy(void) {
  // Destroy the audio thread
  destroyAudioThread(g_audioThread);

  // Deinit MACOS audio
  MacStopSound();
  DeinitMacSound();

  // Free the audio buffer
  free(g_audioBuffer);
  g_audioBuffer = NULL;
}

void TZXCompat_start(void) {
  // Set GPIO pin to output mode (ensuring it is LOW)
  // m_GpioOutputPin.Write(LOW);
  // m_GpioOutputPin.SetMode(GPIOModeOutput);

  // Clear the audio buffer
  g_audioBufferReadIndex = 0;
  g_audioBufferWriteIndex = 0;
  g_audioBufferLastValue = 0;
  g_audioBufferReady = false;

  // Unmute the audio
  SetMute(false);
}

void TZXCompat_stop(void) {
  // Set GPIO pin to input mode
  // m_GpioOutputPin.SetMode(GPIOModeInput);

  // Mute the audio
  SetMute(true);

  // Clear the audio buffer
  g_audioBufferReadIndex = 0;
  g_audioBufferWriteIndex = 0;
  g_audioBufferLastValue = 0;
  g_audioBufferReady = false;
}

void TZXCompat_pause(unsigned char bPause) {
  //
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

  // Calculate the period in audio samples
  uint32_t periodSamples = ((uint64_t)periodUs * AudioPlaybackRate / 1000000ull) + 1;

  // Check for pauses, don't fill the buffer for a pause!
  bool isPause = periodUs > 1500;  // Period longer than 1.5ms is a pause
  if (isPause) {
    // g_pinState = 0;
    //   periodSamples = 1;
  }

  // Fill the audio buffer with the pin state and period
  AudioPinSample *s = &g_audioBuffer[g_audioBufferWriteIndex];
  s->state = g_pinState;
  s->samples = periodSamples;

  if (g_audioBufferWriteIndex + 1 != g_audioBufferReadIndex) {
    g_audioBufferWriteIndex = (g_audioBufferWriteIndex + 1) % g_audioBufferLength;
  } else {
    // Buffer has overflowed
    // printf("Audio buffer overflow\n");
    // assert(false);
    printf("^\n");
  }

  // printf("+");

  uint32_t bufferCount = (g_audioBufferWriteIndex - g_audioBufferReadIndex);
  if (bufferCount > g_audioBufferLength - 1) {
    bufferCount = (g_audioBufferLength - g_audioBufferReadIndex) + g_audioBufferWriteIndex;
  }

  bool bufferLevelGood = (bufferCount * 100) / g_audioBufferLength > 50;

  // Start the timer for a period in microseconds
  // g_bAudioTimerRunning = true;

  // if ((bufferCount * 100) / g_audioBufferLength > 50) {
  //   // If buffer over 50% full, wait for the audio thread to catch up
  //   // g_nAudioTimerPeriodNs = (uint64_t)periodUs * 1000ul;
  //   g_nAudioTimerPeriodNs = (uint64_t)AudioIntervalMs * NSEC_PER_MSEC * 2ull;  // 2x the audio interval

  //   // Yield the thread
  //   pthread_yield_np();

  //   // Signal the audio thread (TODO: use a semaphore and a mutex for protection)
  //   sem_post(g_audioSemaphore);
  // } else {
  //   // If buffer under 50% full, signal the audio thread without delay
  //   g_nAudioTimerPeriodNs = 0ull;

  //   // Yield the thread
  //   pthread_yield_np();

  //   // Signal the audio thread without delay (TODO: use a semaphore and a mutex for protection)
  //   sem_post(g_audioSemaphore);
  // }

  // Remove a little from the wait period so this thread does not get ahead of the audio thread
  uint64_t waitPeriodUs = MAX(periodUs + TIMER_FIXED_OFFSET_US, 0);

  if (bufferLevelGood) {
    // Wait longer if buffer getting full
    g_audioBufferReady = true;
    waitPeriodUs += TIMER_VARAIBLE_OFFSET_US;
    // printf("F\n");
  } else {
    if (!g_audioBufferReady) {
      // Don't wait at all initially if buffer not ready to fill buffer quickly
      g_nAudioTimerPeriodNs = 0;
    } else {
      // Wait a little shorter to fill buffer
      waitPeriodUs -= TIMER_VARAIBLE_OFFSET_US;
    }
  }

  g_bAudioTimerRunning = true;
  g_nAudioTimerPeriodNs = waitPeriodUs * 1000ull;

  // Signal the audio thread (TODO: use a semaphore and a mutex for protection)
  sem_post(g_audioSemaphore);
}

void TZXCompat_timerStop(void) {
  // Stop the timer
  g_bAudioTimerRunning = false;
  g_nAudioTimerPeriodNs = 0;
}

static void onTimer() {
  // Fire the timer event in the TZXCompat layer
  TZXCompat_onTimer();
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
  g_pinState = 0;
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
  g_pinState = 1;
}

unsigned int TZXCompat_getTickMs(void) {
  // Get the current tick/time value in milliseconds
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);

  unsigned int ms = (unsigned int)((spec.tv_sec * 1000) + (spec.tv_nsec / NSEC_PER_SEC));

  return ms;
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

static void transferAudioBuffer(void *buffer, unsigned int bufferSize) {
  // If buffer is not ready, fill with silence
  if (!g_audioBufferReady) {
    memset(buffer, 0x00, bufferSize);
    printf("W\n");
    return;
  }
  // printf("-");

  int8_t *pBufferOut = (int8_t *)buffer;
  // g_audioBufferReadIndex = 0;
  // g_audioBufferWriteIndex = 0;
  // g_audioBufferLastValue = 0;

  uint32_t bufferCount = (g_audioBufferWriteIndex - g_audioBufferReadIndex);
  if (bufferCount > g_audioBufferLength - 1) {
    bufferCount = (g_audioBufferLength - g_audioBufferReadIndex) + g_audioBufferWriteIndex;
  }
  bool bPrint = ((double)rand() / RAND_MAX) * 100 < 10;  // Print 1 in 100 times
  if (bPrint) printf("Buffer: %d\n", bufferCount);

  bool bEmpty = false;

  uint32_t i = 0;
  AudioPinSample *aps = NULL;
  int8_t value = g_audioBufferLastValue;
  bool bEndOfAps = false;

  while (i < bufferSize) {
    if (aps == NULL) {
      // If we don't have an AudioPinSample, get the next one from the buffer
      if (g_audioBufferReadIndex != g_audioBufferWriteIndex) {
        aps = &g_audioBuffer[g_audioBufferReadIndex];
        value = g_audioBufferLastValue = aps->state ? 0xFF : 0x00;
      } else {
        // Buffer is empty
        bEmpty = true;
      }
    }

    if (!bEmpty) {
      // If we have an AudioPinSample, get the value, decrement the samples and check if we need to get the next one
      if (aps->samples > 0) {
        aps->samples--;

        if (aps->samples == 0) {
          bEndOfAps = true;
        }
      } else {
        bEndOfAps = true;
      }
    }

    // Handle end of AudioPinSample: Increment the read index and set the AudioPinSample to NULL
    if (bEndOfAps) {
      g_audioBufferReadIndex = (g_audioBufferReadIndex + 1) % g_audioBufferLength;
      aps = NULL;
      bEndOfAps = false;
    }

    // Set the value in the audio buffer
    pBufferOut[i] = value;
    i++;
  }

  if (bEmpty) {
    printf("E\n");
  }

  // memset(buffer, g_pinState ? 0xFF : 0x00, bufferSize);
}

static void createAudioThread(pthread_t thread) {
  if (g_bAudioThreadRunning) return;

  g_bAudioThreadRunning = true;

  // Create the audio thread semaphore
  g_audioSemaphore = sem_open("audioSemaphore", O_CREAT, 0644, 0);

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
    // Wait for the semaphore
    sem_wait(g_audioSemaphore);
    if (!g_bAudioThreadRunning) break;  // Check if the thread is still running

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
      // Yield the thread
      pthread_yield_np();
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