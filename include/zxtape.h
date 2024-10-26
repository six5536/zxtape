
#ifndef _zxtape_h_
#define _zxtape_h_

// TODO - make dependent on platform
#include "../lib/compat/zxtape_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ZXTAPE_STATUS_T {
  bool bLoaded;
  bool bRewound;
  bool bPlaying;
  bool bPaused;
  u32 nTrack;
  u32 nPosition;
  const char *pFilename;
  u32 nTrackCount;
  u32 nLength;
} ZXTAPE_STATUS_T;

typedef struct _ZXTAPE_HANDLE_T {
  u32 nInstanceId;
} ZXTAPE_HANDLE_T;

typedef struct _ZXTAPE_FILE_API_T {
  TZX_FILETYPE *dir;  // Set 'dir' variable to this
  void (*initialize)(TZX_FILETYPE *pFileType);
} ZXTAPE_FILE_API_T;

/* Exported functions */
ZXTAPE_HANDLE_T *zxtape_create();
void zxtape_destroy(ZXTAPE_HANDLE_T *pInstance);
void zxtape_init(ZXTAPE_HANDLE_T *pInstance);
bool zxtape_loadFile(ZXTAPE_HANDLE_T *pInstance, const char *pFilename);
void zxtape_loadBuffer(ZXTAPE_HANDLE_T *pInstance, const char *pFilename, const unsigned char *pTapeBuffer,
                       unsigned long nTapeBufferLen);
void zxtape_playPause(ZXTAPE_HANDLE_T *pInstance);
void zxtape_previous(ZXTAPE_HANDLE_T *pInstance);
void zxtape_next(ZXTAPE_HANDLE_T *pInstance);
void zxtape_rewind(ZXTAPE_HANDLE_T *pInstance);
bool zxtape_isLoaded(ZXTAPE_HANDLE_T *pInstance);
bool zxtape_isRewound(ZXTAPE_HANDLE_T *pInstance);
bool zxtape_isStarted(ZXTAPE_HANDLE_T *pInstance);
bool zxtape_isPlaying(ZXTAPE_HANDLE_T *pInstance);
bool zxtape_isPaused(ZXTAPE_HANDLE_T *pInstance);
void zxtape_run(ZXTAPE_HANDLE_T *pInstance);

#ifdef __cplusplus
}
#endif

#endif  // _zxtape_h_