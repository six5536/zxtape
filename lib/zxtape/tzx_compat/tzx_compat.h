
#ifndef _tzx_compat_h_
#define _tzx_compat_h_

#ifdef __ZX_TAPE_MACOS__
#include "./macos/tzx_compat_macos_os_headers.h"
#endif  // __ZX_TAPE_MACOS__

#ifdef __ZX_TAPE_CIRCLE__
#include "./circle/tzx_compat_circle_os_headers.h"
#endif  // __ZX_TAPE_CIRCLE__

// Maximum length for long filename support (ideally as large as possible to support very long filenames)
#define ZX_TAPE_MAX_FILENAME_LEN 1023

// TZX Compat Lang definitions
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
  bool (*open)(struct _TZX_FILETYPE* dir, u32 index, TZX_oflag_t oflag);
  void (*close)();
  int (*read)(void* buf, unsigned long count);
  bool (*seekSet)(u64 pos);
} TZX_FILETYPE;

/* External Variables (to be implemented by zxtape consumer) */
extern char TZX_fileName[];              // Current filename
extern u16 TZX_fileIndex;                // Index of current file, relative to current directory (generally set to 0)
extern TZX_FILETYPE TZX_entry, TZX_dir;  // SD card current file (=entry) and current directory (=dir) objects
extern size_t TZX_filesize;              // filesize used for dimensioning files
extern TZX_TIMER TZX_Timer;  // Timer configure a timer to fire interrupts to control the output wave (call wave())
extern bool TZX_pauseOn;     // Control pause state

/* External Variables (implemented in TZX library) */
extern bool TZX_PauseAtStart;      // Set to true to pause at start of file
extern unsigned char TZX_currpct;  // Current percentage of file played (in file bytes, so not 100% accurate)

/* External functions */
extern void TZXCompat_create(void);
extern void TZXCompat_destroy(void);
extern void TZXCompat_timerStart(unsigned long periodUs);
extern void TZXCompat_buffer(unsigned long periodUs);
extern void TZXCompat_delay(unsigned long time);
extern void TZXCompat_noInterrupts();                  // Disable interrupts
extern void TZXCompat_interrupts();                    // Enable interrupts
extern void TZXCompat_setAudioLow();                   // Set the GPIO output pin low
extern void TZXCompat_setAudioHigh();                  // Set the GPIO output pin high
extern void TZXCompat_log(const char* pMessage, ...);  // Log a message

/* Exported functions and macros */

// TZX Compat APIs
void TZXCompatInternal_initialize(void* pControllerInstance, TZX_CALLBACKS_T* pCallbacks);

/* TZX APIs */
void TZXSetup();
void TZXLoop();
void TZXPlay();
void TZXPause();
void TZXStop();

// ZxTape Logging API
#define zxtape_log_info(...) zxtape_log("INFO", __VA_ARGS__)
#define zxtape_log_debug(...) zxtape_log("DEBUG", __VA_ARGS__)
#define zxtape_log_warn(...) zxtape_log("WARN", __VA_ARGS__)
#define zxtape_log_error(...) zxtape_log("ERROR", __VA_ARGS__)
#define zxtape_log_fatal(...) zxtape_log("FATAL", __VA_ARGS__)
extern void zxtape_log(const char* pLevel, const char* pFormat, ...);

#endif  // _tzx_compat_h_