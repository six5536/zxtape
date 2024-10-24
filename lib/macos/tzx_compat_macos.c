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
size_t TZX_filesize;              // filesize used for dimensioning files
TZX_TIMER TZX_Timer;              // Timer configure a timer to fire interrupts to control the output wave (call wave())
bool TZX_pauseOn;                 // Control pause state

/* Local variables */
static unsigned char *pFile = NULL;  // Pointer to current file
static uint64_t nFileSeekIdx = 0;    // Current file seek position
static unsigned tzxLoopCount = 0;    // HACK to call wave less than loop count at start

// Contains the current file
static const unsigned char *pGAME = 0;
static unsigned long GAME_SIZE = 0;

/* Private function forward declarations */
// static void _zxtape_vlog(const char *pLevel, va_list args);
// File API
static bool filetype_open(TZX_FILETYPE *dir, uint32_t index, TZX_oflag_t oflag);
static void filetype_close();
static int filetype_read(void *buf, unsigned long count);
static bool filetype_seekSet(uint64_t pos);

// Timer API
static void timer_initialize();
static void timer_stop();
static void timer_setPeriod(unsigned long periodUs);

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

void TZX_initializeFileType(TZX_FILETYPE *pFileType) {
  pFileType->open = filetype_open;
  pFileType->close = filetype_close;
  pFileType->read = filetype_read;
  pFileType->seekSet = filetype_seekSet;
}

void TZX_initializeTimer(TZX_TIMER *pTimer) {
  pTimer->initialize = timer_initialize;
  pTimer->stop = timer_stop;
  pTimer->setPeriod = timer_setPeriod;
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

//
// File API
//

static bool filetype_open(TZX_FILETYPE *dir, uint32_t index, TZX_oflag_t oflag) {
  zxtape_log_debug("filetype_open");

  pFile = (unsigned char *)pGAME;
  TZX_filesize = GAME_SIZE;

  nFileSeekIdx = 0;

  zxtape_log_debug("filesize: %d", TZX_filesize);

  return true;
}

static void filetype_close() {
  zxtape_log_debug("filetype_close");

  pFile = NULL;
  nFileSeekIdx = 0;
}

static int filetype_read(void *buf, unsigned long count) {
  // LOGDBG("filetype_read(%lu)", count);

  if (nFileSeekIdx + count > TZX_filesize) {
    count = TZX_filesize - nFileSeekIdx;
  }

  // for (unsigned long i = 0; i < count; i++) {
  //   LOGDBG("%02x ", *(pFile + nFileSeekIdx + i));
  // }

  memcpy(buf, pFile + nFileSeekIdx, count);
  nFileSeekIdx += count;

  return count;
}

static bool filetype_seekSet(uint64_t pos) {
  // LOGDBG("filetype_seekSet(%lu)", pos);

  if (pos >= TZX_filesize) return false;

  nFileSeekIdx = pos;

  return true;
}

//
// Timer API
//

static void timer_initialize() {
  // LOGDBG("timer_initialize");
  // Ignore
}

static void timer_stop() {
  // LOGDBG("timer_stop");
  // Ignore
}

static void timer_setPeriod(unsigned long periodUs) {
  // LOGDBG("timer_setPeriod(%lu)", periodUs);
  // TODO
  // pZxTape->wave_isr_set_period(periodUs);
}
