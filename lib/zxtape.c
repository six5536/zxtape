#include "../include/zxtape.h"

// #include <zxtape/zxtape.h>

// Maximum length for long filename support (ideally as large as possible to support very long filenames)
#define ZX_TAPE_MAX_FILENAME_LEN 1023

#define ZX_TAPE_CONTROL_UPDATE_MS 100       // 3 seconds (could be 0)
#define ZX_TAPE_END_PLAYBACK_DELAY_MS 3000  // 3 seconds (could be longer by up to ZX_TAPE_CONTROL_UPDATE_MS)

typedef struct _ZXTAPE_T {
  ZXTAPE_HANDLE_T handle;
  ZX_TAPE_STATUS_T status;
  bool bLoaded;
  bool bRunning;
  bool bButtonPlayPause;
  bool bButtonStop;
  bool bEndPlayback;
  unsigned nEndPlaybackDelay;
  const unsigned char *pGame;
  u32 nGameSize;

  unsigned m_nLastControlTicks;
} ZXTAPE_T;

typedef struct _INSTANCE_LIST_T {
  ZXTAPE_T *pInstance;
  struct _INSTANCE_LIST_T *pNext;
} INSTANCE_LIST_T;

/* Imported global variables */
bool TZX_PauseAtStart;  // Set to true to pause at start of file
u8 TZX_currpct;         // Current percentage of file played (in file bytes, so not 100% accurate)

/* Exported global variables */
char TZX_fileName[ZX_TAPE_MAX_FILENAME_LEN + 1];  // Current filename
uint16_t TZX_fileIndex;           // Index of current file, relative to current directory (generally set to 0)
TZX_FILETYPE TZX_entry, TZX_dir;  // SD card current file (=entry) and current directory (=dir) objects
size_t TZX_filesize;              // filesize used for dimensioning files
TZX_TIMER TZX_Timer;              // Timer configure a timer to fire interrupts to control the output wave (call wave())
bool TZX_pauseOn;                 // Control pause state

/* Local global variables */
static INSTANCE_LIST_T *g_pInstanceList = NULL;

// External functions
// extern zxtape_log(const char *pMessage);

/* Forward function declarations */

/* Exported functions */

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

    pInstance->m_nLastControlTicks = 0;

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

void zxtape_init(ZXTAPE_HANDLE_T *pInstance) {
  //
  zxtape_log_info("Initializing ZX TAPE");
  assert(pInstance != NULL);

  TZX_fileIndex = 0;
  TZX_initializeFileType(&TZX_dir);
  TZX_initializeFileType(&TZX_entry);
  TZX_filesize = 0;
  TZX_initializeTimer(&TZX_Timer);
  TZX_pauseOn = false;
  TZX_currpct = 0;
  TZX_PauseAtStart = false;
}
