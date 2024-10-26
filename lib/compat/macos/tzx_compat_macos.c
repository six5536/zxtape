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
static void *g_pControllerInstance = NULL;
static TZX_CALLBACKS_T *g_pCallbacks = NULL;
static unsigned g_tzxLoopCount = 0;  // HACK to call wave less than loop count at start

// Contains the current file
static const unsigned char *g_pGAME = 0;
static unsigned long GAME_SIZE = 0;

/* Private function forward declarations */

// Timer API
static void initializeTimer(TZX_TIMER *pTimer);
static void timer_initialize();
static void timer_stop();
static void timer_setPeriod(unsigned long periodUs);

//
// Exported API
//

void TZXCompat_Initialize(void *pControllerInstance, TZX_CALLBACKS_T *pCallbacks) {
  TZX_Log("TZX_Initialize");

  g_pControllerInstance = pControllerInstance;
  g_pCallbacks = pCallbacks;

  TZX_fileIndex = 0;
  TZX_filesize = 0;
  TZX_pauseOn = false;
  TZX_currpct = 0;
  TZX_PauseAtStart = false;
  initializeTimer(&TZX_Timer);

  TZXSetup();
}

void TZXCompat_Start(void) {
  // Set GPIO pin to output mode (ensuring it is LOW)
  // m_GpioOutputPin.Write(LOW);
  // m_GpioOutputPin.SetMode(GPIOModeOutput);
}

void TZXCompat_Stop(void) {
  // Set GPIO pin to input mode
  // m_GpioOutputPin.SetMode(GPIOModeInput);
}

void TZXCompat_TimerInitialize(void) {
  // Initialise / reset the timer

  // TODO: Implement
}

void TZXCompat_TimerStop(void) {
  // Stop the timer
  // TODO: Implement
}

unsigned int TZXCompat_TimerMs(void) {
  // Get the current timer value in milliseconds
  // TODO: Implement
  return 0;
}

//
// File API
//

bool TZXCompat_fileOpen(TZX_FILETYPE *dir, uint32_t index, TZX_oflag_t oflag) {
  // TODO: Implement
  // char* pF = TZX_fileName; < Must be full path

  // Must set TZX_filesize

  return true;
}

void TZXCompat_fileClose() {
  // TODO: Implement
}

int TZXCompat_fileRead(void *buf, unsigned long count) {
  // TODO: Implement
  return 0;
}

bool TZXCompat_fileSeekSet(uint64_t pos) {
  // TODO: Implement

  return true;
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
// TZX API
//

// Log a message
void TZX_Log(const char *pFormat, ...) {
  va_list args;

  // Log a message
  va_start(args, pFormat);
  fprintf(stdout, "%s [%s] ", "TZX", "DEBUG");
  vfprintf(stdout, pFormat, args);
  fprintf(stdout, "\n");
  va_end(args);
}

// Set the GPIO output pin low
void TZX_LowWrite() {
  // TZX_Log("LowWrite");
  // pZxTape->wave_set_low();
  // m_GpioOutputPin.Write(LOW);
  printf("_");
}

// Set the GPIO output pin high
void TZX_HighWrite() {
  // TZX_Log("HighWrite");
  // pZxTape->wave_set_high();
  // m_GpioOutputPin.Write(HIGH);
  printf("-");
}

// End the current file playback (EOF or error)
void TZX_stopFile() {
  TZX_Log("stopFile");

  assert(g_pControllerInstance != NULL);
  assert(g_pCallbacks != NULL);

  g_pCallbacks->endPlayback(g_pControllerInstance);
}

// Called to display the playback time (at start)
void TZX_lcdTime() {
  // TZX_Log("lcdTime");
}

// Called to display the playback percent (during playback)
void TZX_Counter2() {
  // TZX_Log("Counter2");
}

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

int TZX_readfile(byte bytes, unsigned long p) {
  // Only relevant for ORIC tap file, so just do nothing
  return 0;
}

void TZX_OricBitWrite() {
  // Only relevant for ORIC tap file, so just do nothing
}

void TZX_OricDataBlock() {
  // Only relevant for ORIC tap file, so just do nothing
}

void TZX_ReadUEFHeader() {
  // Only relevant for UEF file, so just do nothing
}

void TZX_writeUEFData() {
  // Only relevant for UEF file, so just do nothing
}

void TZX_UEFCarrierToneBlock() {
  // Only relevant for UEF file, so just do nothing
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

//
// Timer API
//

static void initializeTimer(TZX_TIMER *pTimer) {
  pTimer->initialize = timer_initialize;
  pTimer->stop = timer_stop;
  pTimer->setPeriod = timer_setPeriod;
}

static void timer_initialize() {
  // zxtape_log_debug("timer_initialize");
  // Ignore - timer init handled by the controller
}

static void timer_stop() {
  // zxtape_log_debug("timer_stop");
  // Ignore
  // - stopping the timer is handled by the controller in runLoop() callback
  // - runLoop() calls TZX_TimerStop() to stop the timer
}

static void timer_setPeriod(unsigned long periodUs) {
  // zxtape_log_debug("timer_setPeriod(%lu)", periodUs);
  // TODO
  // pZxTape->wave_isr_set_period(periodUs);
  // m_OutputTimer.Start(periodUs);
}
