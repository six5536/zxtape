#include "tzx_compat_macos.h"

#include "tzx_compat_internal_macos.h"

// Maximum length for long filename support (ideally as large as possible to support very long filenames)
#define ZX_TAPE_MAX_FILENAME_LEN 1023

#define ZX_TAPE_CONTROL_UPDATE_MS 100       // 3 seconds (could be 0)
#define ZX_TAPE_END_PLAYBACK_DELAY_MS 3000  // 3 seconds (could be longer by up to ZX_TAPE_CONTROL_UPDATE_MS)

/* External global variables */
extern bool TZX_PauseAtStart;  // Set to true to pause at start of file
extern byte TZX_currpct;       // Current percentage of file played (in file bytes, so not 100% accurate)

/* Exported global variables */
char TZX_fileName[ZX_TAPE_MAX_FILENAME_LEN + 1];  // Current filename
uint16_t TZX_fileIndex;           // Index of current file, relative to current directory (generally set to 0)
TZX_FILETYPE TZX_entry, TZX_dir;  // SD card current file (=entry) and current directory (=dir) objects
unsigned long TZX_filesize;       // filesize used for dimensioning files
TZX_TIMER TZX_Timer;              // Timer configure a timer to fire interrupts to control the output wave (call wave())
bool TZX_pauseOn;                 // Control pause state

/* Forward declarations */
// static void _zxtape_vlog(const char *pLevel, va_list args);

/**
 * Disable interrupts
 */
void TZX_noInterrupts() {
  //
}

/**
 * Re-enable interrupts
 */
void TZX_interrupts() {
  //
}

/**
 * Delay a number of milliseconds in a busy loop
 */
void TZX_delay(unsigned long ms) {
  //
}

/**
 * Change the mode of a GPIO pin
 */
void TZX_pinMode(unsigned pin, unsigned mode) {
  //
}

//
// Utility functions
//

// Basic (good enough) implementation of strlwr()
char *TZX_strlwr(char *str) {
  int i = 0;

  while (str[i] != '\0') {
    if (str[i] >= 'A' && str[i] <= 'Z') {
      str[i] = str[i] + 32;
    }
    i++;
  }

  return str;
}

int readfile(byte bytes, unsigned long p) {
  // Only relevant for ORIC tap file, so just do nothing
  return 0;
}

void OricBitWrite() {
  // Only relevant for ORIC tap file, so just do nothing
}

void OricDataBlock() {
  // Only relevant for ORIC tap file, so just do nothing
}

void ReadUEFHeader() {
  // Only relevant for UEF file, so just do nothing
}

void writeUEFData() {
  // Only relevant for UEF file, so just do nothing
}

void UEFCarrierToneBlock() {
  // Only relevant for UEF file, so just do nothing
}

//
// ZXTape functions
//

void _zxtape_log(const char *pLevel, const char *pFormat, ...) {
  va_list args;

  // Log a message
  va_start(args, pFormat);
  fprintf(stdout, "%s [%s] ", "ZxTape", pLevel);
  vfprintf(stdout, pFormat, args);
  fprintf(stdout, "\n");
  va_end(args);
}
