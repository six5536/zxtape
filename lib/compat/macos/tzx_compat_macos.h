
// Source: https://github.com/sadken/TZXDuino/blob/master/TZXDuino.h

#ifndef _tzx_compat_macos_h_
#define _tzx_compat_macos_h_

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TZX Compat Lang definitions
#ifdef __cplusplus
#else
typedef char bool;
#define false 0
#define true 1
#endif

typedef unsigned char u8;        // 8-bit unsigned integer
typedef unsigned short u16;      // 16-bit unsigned integer
typedef unsigned long u32;       // 32-bit unsigned integer
typedef unsigned long long u64;  // 64-bit unsigned integer
typedef char i8;                 // 8-bit signed integer
typedef short i16;               // 16-bit signed integer
typedef long i32;                // 32-bit signed integer
typedef long long i64;           // 64-bit signed integer

// TZX Compat Callbacks
typedef struct _TZX_CALLBACKS_T {
  void (*endPlayback)(void* pInstance);
} TZX_CALLBACKS_T;

// TZX Compat Timer
typedef struct _TZX_TIMER {
  void (*initialize)();
  void (*stop)();
  void (*setPeriod)(unsigned long period);
} TZX_TIMER;

// TZX Compat Files
// typedef char FsBaseFile;
typedef unsigned TZX_oflag_t;
typedef struct _TZX_FILETYPE {
  // Implementation instance
  void* pImplementation;

  // File API
  bool (*open)(struct _TZX_FILETYPE* dir, uint32_t index, TZX_oflag_t oflag);
  void (*close)();
  int (*read)(void* buf, unsigned long count);
  bool (*seekSet)(uint64_t pos);
} TZX_FILETYPE;

/* Exported functions and macros */

// TZX Compat APIs
void TZXCompat_Initialize(void* pControllerInstance, TZX_CALLBACKS_T* pCallbacks);
void TZXCompat_Start(void);
void TZXCompat_Stop(void);
void TZXCompat_TimerInitialize(void);
void TZXCompat_TimerStop(void);
unsigned int TZXCompat_TimerMs(void);

// TZX Compat File APIs
bool TZXCompat_fileOpen(TZX_FILETYPE* dir, uint32_t index, TZX_oflag_t oflag);
void TZXCompat_fileClose();
int TZXCompat_fileRead(void* buf, unsigned long count);
bool TZXCompat_fileSeekSet(uint64_t pos);

/* TZX APIs */
void TZXSetup();
void TZXLoop();
void TZXPlay();
void TZXPause();
void TZXStop();

// ZxTape Logging API
#define zxtape_log_info(...) _zxtape_log("INFO", __VA_ARGS__)
#define zxtape_log_debug(...) _zxtape_log("DEBUG", __VA_ARGS__)
#define zxtape_log_warn(...) _zxtape_log("WARN", __VA_ARGS__)
#define zxtape_log_error(...) _zxtape_log("ERROR", __VA_ARGS__)
#define zxtape_log_fatal(...) _zxtape_log("FATAL", __VA_ARGS__)
void _zxtape_log(const char* pLevel, const char* pFormat, ...);

#endif  // _tzx_compat_macos_h_
