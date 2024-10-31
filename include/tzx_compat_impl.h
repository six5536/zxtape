
#ifndef _tzx_compat_impl_h_
#define _tzx_compat_impl_h_

#ifdef __ZX_TAPE_MACOS__
#include "../lib/zxtape/tzx_compat/macos/tzx_compat_macos_os_headers.h"
#endif  // __ZX_TAPE_MACOS__

#ifdef __ZX_TAPE_CIRCLE__
#include "../lib/zxtape/tzx_compat/circle/tzx_compat_circle_os_headers.h"
#endif  // __ZX_TAPE_CIRCLE__

// TZX Compat APIs
void TZXCompat_create(void);
void TZXCompat_destroy(void);
void TZXCompat_start(void);
void TZXCompat_stop(void);
void TZXCompat_pause(bool bPause);

void TZXCompat_timerInitialize(void);
void TZXCompat_timerStart(unsigned long periodUs);
void TZXCompat_timerStop(void);
extern void TZXCompat_onTimer(void);  // Function to call on Timer interrupt

void TZXCompat_setAudioLow(void);   // Set the GPIO output pin low
void TZXCompat_setAudioHigh(void);  // Set the GPIO output pin high

unsigned int TZXCompat_getTickMs(void);
void TZXCompat_delay(unsigned long time);
void TZXCompat_noInterrupts(void);  // Disable interrupts
void TZXCompat_interrupts(void);    // Enable interrupts

// TZX Compat File APIs
bool TZXCompat_fileOpen(void* dir, unsigned int index, unsigned oflag);
void TZXCompat_fileClose();
int TZXCompat_fileRead(void* buf, unsigned long count);
bool TZXCompat_fileSeekSet(unsigned long long pos);

// Logging
void TZXCompat_log(const char* pMessage, ...);                  // Log a message (TZX)
void zxtape_log(const char* pLevel, const char* pFormat, ...);  // Log a message (ZxTape)

#endif  // _tzx_compat_impl_h_