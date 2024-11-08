

#include <pthread.h>
#include <sys/param.h>
#include <time.h>

#include "../../../../include/tzx_compat_impl.h"
#include "audio_macos.h"
#include "timer_macos.h"

// Real-time threads, see
// - https://developer.apple.com/library/archive/technotes/tn2169/_index.html
// -
// https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html#//apple_ref/doc/uid/TP30000905-CH211-BABHGEFA
// - https://chromium.googlesource.com/chromium/src/+/refs/heads/main/base/threading/platform_thread_apple.mm <== GOOD

#define AUDIO_BUFFER_MULTIPLE 8
#define AUDIO_BUFFER_LENGTH 1024 * 64  // 64k buffer
#define AUDIO_BUFFER_EQUALIBRIUM_PERCENT 1
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
//

/* Forward declarations */
static void onTimer();
static void transferAudioBuffer(void *buffer, unsigned int bufferSize);
// static void createAudioThread(pthread_t thread);
// static void destroyAudioThread(pthread_t thread);
// static void *audioThread(void *arg);
static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint, boolean_t preemptible);
static int setPriorityRealtimeAudio();

/* Local variables */
static pthread_mutex_t g_interruptMutex;
static pthread_mutexattr_t g_interruptMutexAttr;
static pthread_t g_audioThread = NULL;
static macos_timer_t g_audioTimer = NULL;
static struct sigevent g_audioTimerEvent;
static struct itimerspec g_audioTimerSpec;
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

static uint32_t g_pinState = 0;

static FILE *g_pFile = NULL;

//
// TZX Compat Implemetation
//

void TZXCompat_create(void) {
  // Create the interrupt mutex
  pthread_mutexattr_init(&g_interruptMutexAttr);
  pthread_mutexattr_settype(&g_interruptMutexAttr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&g_interruptMutex, &g_interruptMutexAttr);

  // Create the timer

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

void TZXCompat_destroy(void) {
  // Destroy the audio thread
  timer_delete(g_audioTimer);

  // Deinit MACOS audio
  MacStopSound();
  DeinitMacSound();

  // Free the audio buffer
  free(g_audioBuffer);
  g_audioBuffer = NULL;

  // Destroy the interrupt mutex
  pthread_mutex_destroy(&g_interruptMutex);
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

void TZXCompat_timerInitialize(void) {
  // Initialise / reset the timer

  // Stop the timer if it is running
  TZXCompat_timerStop();

  // Nothing else to do
}

void TZXCompat_timerStart(unsigned long periodUs) {
  // Stop the timer if it is running
  TZXCompat_timerStop();

  // If the period is the EOF period, then stop the tape
  if (periodUs == TZXCompat_EOF_PERIOD) {
    // Stop the tape
    TZX_stopFile();
    return;
  }

  // Calculate the period in audio samples
  uint32_t periodSamples = ((uint64_t)periodUs * AudioPlaybackRate / 1000000ull);

  // Check for pauses, don't fill the buffer for a pause!

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

  bool bufferLevelGood = (bufferCount * 100) / g_audioBufferLength > AUDIO_BUFFER_EQUALIBRIUM_PERCENT;

  // Remove a little from the wait period so this thread does not get ahead of the audio thread
  uint64_t waitPeriodUs = MAX(periodUs + TIMER_FIXED_OFFSET_US, 0);

  if (bufferLevelGood || TZX_pauseOn) {
    // Wait longer if buffer getting full / paused
    g_audioBufferReady = true;
    waitPeriodUs += TIMER_VARAIBLE_OFFSET_US;
    // printf("F\n");
  } else {
    if (!g_audioBufferReady) {
      // When starting up, don't wait long buffer not ready, in order to fill buffer quickly
      // However, the first wait must be honoured in order that the internal TZX buffer is filled
      waitPeriodUs = 100;
    } else {
      // Wait a little shorter to fill buffer
      waitPeriodUs = 1;  //-= TIMER_VARAIBLE_OFFSET_US;
    }
  }

  g_bAudioTimerRunning = true;
  g_nAudioTimerPeriodNs = waitPeriodUs * 1000ull;

  // Start the timer for a period in microseconds

  // Configure and start the timer
  g_audioTimerSpec.it_value.tv_sec = g_nAudioTimerPeriodNs / NSEC_PER_SEC;
  g_audioTimerSpec.it_value.tv_nsec = g_nAudioTimerPeriodNs % NSEC_PER_SEC;

  timer_settime(g_audioTimer, 0, &g_audioTimerSpec, NULL);
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
  // Lock the 'interrupt' mutex when calling the timer routine to block out the main loop thread
  pthread_mutex_lock(&g_interruptMutex);

  // Fire the timer event in the TZXCompat layer
  TZXCompat_onTimer();

  // Unlock the 'interrupt' mutex
  pthread_mutex_unlock(&g_interruptMutex);
}

// TODO - not to be implemetned but to be called on timer interrupt
// void TZXCompat_onTimer(void) {
//   // Function to call on Timer interrupt
// }

// Set the GPIO output pin low
void TZXCompat_setAudioLow() {
  // TZXCompat_log("LowWrite");
  // m_GpioOutputPin.Write(LOW);
  g_pinState = 0;
}

// Set the GPIO output pin high
void TZXCompat_setAudioHigh() {
  // TZXCompat_log("HighWrite");
  // m_GpioOutputPin.Write(HIGH);
  g_pinState = 1;
}

unsigned int TZXCompat_getTickMs(void) {
  // Get the current timer value in milliseconds
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
 *
 * The timer is not on an interrupt, but is a separate thread. Use a mutex for synchronisation.
 */
inline void TZXCompat_noInterrupts() {
  // Lock the mutex
  pthread_mutex_lock(&g_interruptMutex);
}

/**
 * Re-enable interrupts
 *
 * The timer is not on an interrupt, but is a separate thread. Use a mutex for synchronisation.
 */
inline void TZXCompat_interrupts() {
  // Unlock the mutex
  pthread_mutex_unlock(&g_interruptMutex);

  // Allow other threads to run when interrupt is released
  // sched_yield();
}

//
// File API
//

unsigned char TZXCompat_fileOpen(void *dir, unsigned int index, unsigned oflag) {
  const char *pF = TZX_fileName;

  g_pFile = fopen(pF, "rb");
  if (g_pFile == NULL) {
    TZX_filesize = 0;
    return 0;
  }

  // Must set TZX_filesize
  fseek(g_pFile, 0, SEEK_END);
  TZX_filesize = ftell(g_pFile);
  fseek(g_pFile, 0, SEEK_SET);

  return 1;
}

void TZXCompat_fileClose() {
  if (g_pFile != NULL) {
    fclose(g_pFile);
    g_pFile = NULL;
  }
}

int TZXCompat_fileRead(void *buf, unsigned long count) {
  if (g_pFile != NULL) {
    return fread(buf, 1, count, g_pFile);
  }

  return 0;
}

unsigned char TZXCompat_fileSeekSet(unsigned long long pos) {
  if (g_pFile != NULL) {
    fseek(g_pFile, pos, SEEK_SET);
    return 1;
  }

  return 0;
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
