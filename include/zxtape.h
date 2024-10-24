
#ifndef _zxtape_h_
#define _zxtape_h_

#include "../lib/macos/tzx_compat_macos.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ZX_TAPE_STATUS_T {
  bool bLoaded;
  bool bRewound;
  bool bPlaying;
  bool bPaused;
  u32 nTrack;
  u32 nPosition;
  const char *pFilename;
  u32 nTrackCount;
  u32 nLength;
} ZX_TAPE_STATUS_T;

typedef struct _ZXTAPE_HANDLE_T {
  u32 nInstanceId;
} ZXTAPE_HANDLE_T;

/* Exported functions */
ZXTAPE_HANDLE_T *zxtape_create();
void zxtape_destroy(ZXTAPE_HANDLE_T *pInstance);
void zxtape_init(ZXTAPE_HANDLE_T *pInstance);

#ifdef __cplusplus
}
#endif

#endif  // _zxtape_h_