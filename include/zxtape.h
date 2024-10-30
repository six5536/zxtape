
#ifndef _zxtape_h_
#define _zxtape_h_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

#ifndef u8
typedef unsigned char u8;  // 8-bit unsigned integer
#endif

#ifndef u16
typedef unsigned short u16;  // 16-bit unsigned integer
#endif

#ifndef u32
typedef unsigned int u32;  // 32-bit unsigned integer
#endif

#ifndef u64
typedef unsigned long long u64;  // 64-bit unsigned integer
#endif

#ifndef i8
typedef char i8;  // 8-bit signed integer
#endif

#ifndef i16
typedef short i16;  // 16-bit signed integer
#endif

#ifndef i32
typedef int i32;  // 32-bit signed integer
#endif

#ifndef i64
typedef long long i64;  // 64-bit signed integer
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