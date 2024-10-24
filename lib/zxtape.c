#include "../include/zxtape.h"

// #include <zxtape/zxtape.h>

static const char *EMPTY_STRING = "";

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

static INSTANCE_LIST_T *g_pInstanceList = NULL;

// External functions
// extern zxtape_log(const char *pMessage);

ZXTAPE_HANDLE_T *zxtape_create() {
  zxtape_log_info("Creating ZX TAPE instance");

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
    pInstance->status.pFilename = EMPTY_STRING;
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
}

void say_hello() {
  //
  printf("Hello, from zxtape!\n");
}
