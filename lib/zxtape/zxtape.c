#include "../../include/zxtape.h"

// #include <zxtape/zxtape.h>

#include "../../include/tzx_compat_impl.h"
#include "./file/zxtape_file_api_buffer.h"
#include "./file/zxtape_file_api_dummy.h"
#include "./file/zxtape_file_api_file.h"
#include "./tzx_compat/tzx_compat.h"

// Maximum length for long filename support (ideally as large as possible to support very long filenames)
// #define ZX_TAPE_MAX_FILENAME_LEN 1023

#define ZX_TAPE_CONTROL_UPDATE_MS 100       // 100 ms (could be 0)
#define ZX_TAPE_END_PLAYBACK_DELAY_MS 3000  // 3 seconds (could be longer by up to ZX_TAPE_CONTROL_UPDATE_MS)

typedef struct _ZXTAPE_T {
  ZXTAPE_HANDLE_T handle;
  ZXTAPE_STATUS_T status;
  bool bLoaded;
  bool bRunning;
  bool bButtonPlayPause;
  bool bButtonStop;
  bool bEndPlayback;
  unsigned nEndPlaybackDelay;
  const unsigned char *pGame;
  u32 nGameSize;

  unsigned nlastTimerMs;

  // Callbacks
  TZX_CALLBACKS_T callbacks;
} ZXTAPE_T;

typedef struct _INSTANCE_LIST_T {
  ZXTAPE_T *pInstance;
  struct _INSTANCE_LIST_T *pNext;
} INSTANCE_LIST_T;

/* Imported global variables */

/* Exported global variables */
// char TZX_fileName[ZX_TAPE_MAX_FILENAME_LEN + 1];  // Current filename
// u16 TZX_fileIndex;                // Index of current file, relative to current directory (generally set to 0)
// TZX_FILETYPE TZX_entry, TZX_dir;  // SD card current file (=entry) and current directory (=dir) objects
// size_t TZX_filesize;              // filesize used for dimensioning files
// TZX_TIMER TZX_Timer;              // Timer configure a timer to fire interrupts to control the output wave (call
// wave()) bool TZX_pauseOn;                 // Control pause state

/* Local global variables */
static INSTANCE_LIST_T *g_pInstanceList = NULL;
static u32 g_nInstanceId = 0;

// External functions
// extern zxtape_log(const char *pMessage);

/* Forward function declarations */
static void endPlayback(ZXTAPE_T *pZxTape);
static void loopPlayback(ZXTAPE_T *pZxTape);
static void loopControl(ZXTAPE_T *pZxTape);
static void playFile(ZXTAPE_T *pZxTape);
static void stopFile(ZXTAPE_T *pZxTape);
static bool checkButtonPlayPause(ZXTAPE_T *pZxTape);
static bool checkButtonStop(ZXTAPE_T *pZxTape);

/* Exported functions */

/**
 * Create a new ZxTape instance
 *
 * Only one instance is allowed
 *
 * @return ZXTAPE_HANDLE_T* Pointer to the new ZxTape instance
 */
ZXTAPE_HANDLE_T *zxtape_create() {
  zxtape_log_info("Creating ZX TAPE instance");

  // Only one instance is allowed
  if (g_pInstanceList != NULL) {
    zxtape_log_error("Only one ZX TAPE instance is allowed");
    assert(g_pInstanceList == NULL);
  }

  ZXTAPE_T *pInstance = (ZXTAPE_T *)malloc(sizeof(ZXTAPE_T));
  assert(pInstance != NULL);  // Ensure memory was allocated

  if (pInstance) {
    // Set instance ID
    pInstance->handle.nInstanceId = g_nInstanceId++;

    // Initialize the instance
    pInstance->status.bLoaded = false;
    pInstance->status.bRewound = false;
    pInstance->status.bPlaying = false;
    pInstance->status.bPaused = false;
    pInstance->status.nTrack = 0;
    pInstance->status.nPosition = 0;
    pInstance->status.pFilename = "";
    pInstance->status.nTrackCount = 0;
    pInstance->status.nLength = 0;

    pInstance->bLoaded = false;
    pInstance->bRunning = false;
    pInstance->bButtonPlayPause = false;
    pInstance->bButtonStop = false;
    pInstance->bEndPlayback = false;
    pInstance->nEndPlaybackDelay = 0;
    pInstance->pGame = NULL;
    pInstance->nGameSize = 0;

    pInstance->nlastTimerMs = 0;

    // Set the callbacks
    pInstance->callbacks.endPlayback = (void (*)(void *))endPlayback;

    // Add the instance to the list
    INSTANCE_LIST_T *pNewListItem = (INSTANCE_LIST_T *)malloc(sizeof(INSTANCE_LIST_T));
    assert(pNewListItem != NULL);  // Ensure memory was allocated

    if (pNewListItem) {
      pNewListItem->pInstance = pInstance;
      pNewListItem->pNext = g_pInstanceList;
      g_pInstanceList = pNewListItem;
    }
  }

  return (ZXTAPE_HANDLE_T *)pInstance;
}

/**
 * Destroy a ZxTape instance
 *
 * @param pInstance Pointer to the ZxTape instance
 */
void zxtape_destroy(ZXTAPE_HANDLE_T *pInstance) {
  zxtape_log_info("Destroying ZX TAPE instance");
  assert(pInstance != NULL);

  // Check if the instance is in the list
  ZXTAPE_HANDLE_T *pFoundInstance = NULL;
  INSTANCE_LIST_T *pListItem = g_pInstanceList;
  INSTANCE_LIST_T *pListPrev = g_pInstanceList;
  while (pListItem) {
    if (pListItem->pInstance == (ZXTAPE_T *)pInstance) {
      pFoundInstance = pInstance;
      break;
    } else {
      pListItem = pListItem->pNext;
    }
    pListPrev = pListItem;
  }

  // Ensure the instance was found
  assert(pFoundInstance != NULL);

  // If the instance was found, free it, and remove it from the list

  // Free instance
  free(pListItem->pInstance);

  // Remove from list
  pListPrev->pNext = pListItem->pNext;
  free(pListItem);
}

/**
 * Initialize the ZxTape instance
 *
 * @param pInstance Pointer to the ZxTape instance
 */
void zxtape_init(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  //
  zxtape_log_info("Initializing ZX TAPE");
  assert(pInstance != NULL);

  TZXCompatInternal_initialize(pInstance, &pZxTape->callbacks);
}

void zxtape_status(ZXTAPE_HANDLE_T *pInstance, ZXTAPE_STATUS_T *pStatus) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  // Update the status
  pZxTape->status.bLoaded = zxtape_isLoaded(pInstance);
  pZxTape->status.bRewound = zxtape_isRewound(pInstance);
  pZxTape->status.bPlaying = zxtape_isPlaying(pInstance);
  pZxTape->status.bPaused = zxtape_isPaused(pInstance);
  pZxTape->status.pFilename = TZX_fileName;

  // Return a copy of the current status
  memcpy(pStatus, &pZxTape->status, sizeof(ZXTAPE_STATUS_T));
}

// TODO - how to handle loading from a file or buffer?
void zxtape_loadBuffer(ZXTAPE_HANDLE_T *pInstance, const char *pFilename, const unsigned char *pTapeBuffer,
                       unsigned long nTapeBufferLen) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  assert(pFilename != 0);
  assert(pTapeBuffer != 0);

  zxtape_log_debug("Loading TAPE (buffer): %s", pFilename);

  // Stop the tape if it it playing
  pZxTape->bButtonStop = true;

  // TZX_fileName, TZX_filesize are externs used by tzx
  strncpy(TZX_fileName, pFilename, ZX_TAPE_MAX_FILENAME_LEN);
  TZX_filesize = nTapeBufferLen;

  // Initialise TZX_dir and TZX_entry
  zxtapeFileApiDummy_initialize(&TZX_dir);
  zxtapeFileApiBuffer_initialize(&TZX_entry, pTapeBuffer, nTapeBufferLen);

  // TODO - check if the file is a valid TAP/TZX file
  // (NOTE, is it possible to check TAP files for validity?)

  // Set the loaded flag
  pZxTape->bLoaded = true;
}

bool zxtape_loadFile(ZXTAPE_HANDLE_T *pInstance, const char *pFilename) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  assert(pFilename != 0);

  zxtape_log_debug("Loading TAPE (file): %s", pFilename);

  // Stop the tape if it it playing
  pZxTape->bButtonStop = true;

  // TZX_fileName, TZX_filesize are externs used by tzx
  strncpy(TZX_fileName, pFilename, ZX_TAPE_MAX_FILENAME_LEN);

  // Initialise TZX_dir and TZX_entry
  zxtapeFileApiDummy_initialize(&TZX_dir);
  zxtapeFileApiFile_initialize(&TZX_entry);

  // Open the file, will set the filesize
  bool res = TZXCompat_fileOpen(NULL, 0, 0);
  if (!res) {
    zxtape_log_error("Failed to open file: %s", pFilename);
    return false;
  }

  // TODO - check if the file is a valid TAP/TZX file
  // (NOTE, is it possible to check TAP files for validity?)

  // Set the loaded flag
  pZxTape->bLoaded = true;

  return true;
}

void zxtape_playPause(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  zxtape_log_debug("Starting/Pausing TAPE");

  if (pZxTape->bLoaded) {
    // Start the tape / pause the tape / unpause the tape
    pZxTape->bButtonStop = false;
    pZxTape->bButtonPlayPause = true;
  } else {
    // No tape loaded, so do nothing
  }
}

void zxtape_previous(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  // ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  // TODO
}

void zxtape_next(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  // ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  // TODO
}

void zxtape_rewind(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  zxtape_log_debug("Stopping/Rewinding TAPE");

  // Stop the tape
  pZxTape->bButtonStop = true;
}

/**
 * The tape is started (it may still be paused)
 *
 */
bool zxtape_isLoaded(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  return pZxTape->bLoaded;
}

/**
 * The tape is loaded, rewound and not playing (TODO - check if loaded)
 *
 */
bool zxtape_isRewound(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  return pZxTape->bLoaded && !pZxTape->bRunning;
}

/**
 * The tape is started (it may still be paused)
 *
 */
bool zxtape_isStarted(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  return pZxTape->bRunning;
}

/**
 * The tape is playing (started and NOT paused)
 *
 */
bool zxtape_isPlaying(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  return pZxTape->bRunning && !TZX_pauseOn;
}

/**
 * The tape is paused (started AND paused)
 *
 */
bool zxtape_isPaused(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;
  return pZxTape->bRunning && TZX_pauseOn;
}

/**
 * Run the ZxTape loop. Call this in the main loop of the application
 *
 * @param pInstance Pointer to the ZxTape instance
 */
void zxtape_run(ZXTAPE_HANDLE_T *pInstance) {
  assert(pInstance != NULL);
  ZXTAPE_T *pZxTape = (ZXTAPE_T *)pInstance;

  // Playback loop
  loopPlayback(pZxTape);

  //  Control loop
  loopControl(pZxTape);
}

//
// Private TZX callbacks
//

/**
 * Called by the TZX library when it ends playback
 */
static void endPlayback(ZXTAPE_T *pZxTape) {
  pZxTape->bEndPlayback = true;
  pZxTape->nEndPlaybackDelay = ZX_TAPE_END_PLAYBACK_DELAY_MS;

  // Ensure last bits are written
  // This is not handled correctly in the TZXDuino code. 'count=255;' is decremented, and the buffer filled with junk,
  // but this happens much faster than the buffer can empty itself, so the last bytes/bits are not played back.
  // Ideally the buffer would notify the code here when it is empty, but the TZXDuino code is not structured that way.

  // Rather than implementing a simple delay here, we count down in the update loop a delay until the buffer should
  // be empty, and then stop the tape.
  // This avoids blocking any thread.
}

//
// Private functions
//

/**
 * Handle playback loop
 */
static void loopPlayback(ZXTAPE_T *pZxTape) {
  if (pZxTape->bRunning && !pZxTape->bEndPlayback) {
    // If tape is running, and we are not ending playback, then run the TZX loop

    // TZXLoop only runs if a file is playing, and keeps the buffer full.
    TZXLoop();

    // Hacked in here for testing
    // wave();
  }
}

/**
 * Handle control loop
 */
static void loopControl(ZXTAPE_T *pZxTape) {
  const unsigned lastTimerMs = TZXCompat_timerGetMs();
  const unsigned elapsedMs = (lastTimerMs - pZxTape->nlastTimerMs);

  // Only run every 100ms
  if (elapsedMs < ZX_TAPE_CONTROL_UPDATE_MS) return;
  // zxtape_log_debug("loop_control: %d", elapsedMs);

  pZxTape->nlastTimerMs = lastTimerMs;

  // Handle Play / pause button
  if (checkButtonPlayPause(pZxTape)) {
    if (!pZxTape->bRunning) {
      // Play pressed, and stopped, so start playing
      playFile(pZxTape);
      // delay(200);
    } else {
      // Play pressed, and playing, so pause / unpause
      if (TZX_pauseOn) {
        // Unpause playback
        TZX_pauseOn = false;
      } else {
        // Pause playback
        TZX_pauseOn = true;
      }
    }
  }

  // Handle stop button
  if (checkButtonStop(pZxTape)) {
    if (pZxTape->bRunning) {
      // Playing and stop pressed, so stop playing
      stopFile(pZxTape);
    }
  }

  // Handle end of playback (allowing buffer to empty)
  if (pZxTape->bEndPlayback) {
    if (pZxTape->nEndPlaybackDelay <= 0) {
      // End of playback delay has expired, buffer should be empty, so stop playing
      stopFile(pZxTape);
    }
    pZxTape->nEndPlaybackDelay -= elapsedMs;
  }
}

/**
 * Start a file playing (initial playback)
 */
static void playFile(ZXTAPE_T *pZxTape) {
  // Ensure stopped
  stopFile(pZxTape);

  // Set initial playback state
  TZX_pauseOn = false;
  TZX_currpct = 100;

  // Notify compatibility layer on start (so can enable audio output, etc)
  TZXCompat_start();

  // Initialise (reset) the output timer
  TZXCompat_timerInitialize();

  TZXPlay();
  pZxTape->bRunning = true;

  if (TZX_PauseAtStart) {
    TZX_pauseOn = true;
    TZXPause();
  }
}

/**
 * Stop the currently playing file
 */
static void stopFile(ZXTAPE_T *pZxTape) {
  zxtape_log_debug("stopFile");

  // Notify compatibility layer on stop (so can disable audio output, etc)
  TZXCompat_stop();

  if (pZxTape->bRunning) {
    zxtape_log_debug("TZXCompat_timerStop()");

    // Stop the output timer (only if it was initialised)
    TZXCompat_timerStop();
  }

  // Stop tzx library
  TZXStop();

  pZxTape->bRunning = false;
  pZxTape->bEndPlayback = false;
  pZxTape->nEndPlaybackDelay = 0;
}

/**
 * Check if the play/pause button has been pressed
 */
static bool checkButtonPlayPause(ZXTAPE_T *pZxTape) {
  if (pZxTape->bButtonPlayPause) {
    pZxTape->bButtonPlayPause = false;
    return true;
  }
  return false;
}

/**
 * Check if the stop button has been pressed
 */
static bool checkButtonStop(ZXTAPE_T *pZxTape) {
  if (pZxTape->bButtonStop) {
    pZxTape->bButtonStop = false;
    return true;
  }
  return false;
}