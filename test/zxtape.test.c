
#include <assert.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <stdio.h>
#include <zxtape.h>

#include "./games/starquake.h"

#define SLEEP_WHEN_IDLE_MS 500
#define SLEEP_WHEN_PLAYING_MS 500

/* Forward declarations */
static void createTapeThread(pthread_t thread, ZXTAPE_HANDLE_T* pZxTape);
static void destroyTapeThread(pthread_t thread);
static void* tapeThread(void* arg);
static int setRealtime(uint32_t period, uint32_t computation, uint32_t constraint);
static int setPriorityRealtimeAudio();

/* Local variables */
static volatile bool g_bZxtapeThreadRunning = false;

int main(int argc, char* argv[]) {
  pthread_t zxtapeThread;

  // Create a new ZXTape instance
  ZXTAPE_HANDLE_T* pZxTape = zxtape_create();

  // Initialize the ZXTape instance
  zxtape_init(pZxTape);

  // Create the tape thread
  createTapeThread(zxtapeThread, pZxTape);

  // Load a buffer into the ZXTape instance
  zxtape_loadBuffer(pZxTape, "starquake.tzx", Starquake, sizeof(Starquake));

  // Play the ZXTape instance
  zxtape_playPause(pZxTape);

  // Wait for the user to press a key
  printf("Press Ctrl-D key to stop the tape\n");

  while (getchar() == EOF) {
    // Wait for the user to press a key
    pthread_yield_np();
  }

  // Destroy the ZXTape instance
  // zxtape_destroy(pZxTape);

  return 0;
}

static void createTapeThread(pthread_t thread, ZXTAPE_HANDLE_T* pZxTape) {
  if (g_bZxtapeThreadRunning) return;

  g_bZxtapeThreadRunning = true;

  int res = pthread_create(&thread, NULL, tapeThread, pZxTape);
  assert(res == 0);

  // Join the thread - no, as we want to run the tape in the background
  // pthread_join(thread, NULL);
}

static void destroyTapeThread(pthread_t thread) {
  if (!g_bZxtapeThreadRunning) return;

  g_bZxtapeThreadRunning = false;

  // Cancel the thread
  pthread_cancel(thread);

  // Join the thread
  pthread_join(thread, NULL);
}

static void* tapeThread(void* arg) {
  ZXTAPE_HANDLE_T* pZxTape = (ZXTAPE_HANDLE_T*)arg;

  while (g_bZxtapeThreadRunning) {
    zxtape_run(pZxTape);

    if (!g_bZxtapeThreadRunning) break;  // Check if the thread is still running

    // Different sleep rate for stopped / playing
    time_t sleepTimeMs = zxtape_isPlaying(pZxTape) ? SLEEP_WHEN_PLAYING_MS : SLEEP_WHEN_IDLE_MS;

    // Wait for the sleep period
    struct timespec ts;
    ts.tv_sec = sleepTimeMs / NSEC_PER_MSEC;
    ts.tv_nsec = sleepTimeMs % NSEC_PER_MSEC;
    nanosleep(&ts, NULL);
  }

  return (void*)0;
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