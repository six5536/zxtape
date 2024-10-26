
// Source: https://github.com/sadken/TZXDuino/blob/master/TZXDuino.h

#ifndef _tzx_compat_circle_internal_h_
#define _tzx_compat_circle_internal_h_

#include "tzx_compat_circle.h"

//
// Required types (should be in Arduino compatability layer?)
//

// Lang definitions
typedef unsigned char byte;           // 8-bit
typedef unsigned short word;          // 16-bit
typedef unsigned short uint16_t;      // 16-bit
typedef unsigned int uint32_t;        // 32-bit
typedef unsigned long long uint64_t;  // 64-bit

// Undefine as redefined
#undef EOF

#define TZX_PROGMEM  // nothing
#define TZX_PSTR(x) x

#define TZX_HIGH 1
#define TZX_LOW 0
#define TZX_INPUT 0x0
#define TZX_OUTPUT 0x1
#define TZX_O_RDONLY 0

#define TZX_outputPin 0  // Audio Output PIN - Ignore this value and set in pinMode callback

#define TZX_bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define TZX_bitSet(value, bit) ((value) |= (1UL << (bit)))
#define TZX_bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define TZX_bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define TZX_bitWrite(value, bit, bitvalue) ((bitvalue) ? TZX_bitSet(value, bit) : TZX_bitClear(value, bit))

#define TZX_word(x0, x1) (((x0 << 8) & 0xFF00) | (x1 & 0xFF))
#define TZX_long(x) ((x) & 0xFFFFFFFF)
#define TZX_highByte(x) ((x >> 8) & 0xFF)
#define TZX_lowByte(x) ((x) & 0xFF)
#define TZX_pgm_read_byte(x) (*(x))

#define TZX_strlen(str) strlen(str)
char* TZX_strlwr(char* str);
#define TZX_strstr(x, y) strstr(x, y)
#define TZX_memcmp(x, y, z) memcmp(x, y, z)
#define TZX_printtextF(...)  // nothing as of yet (could print to log)

void TZX_delay(unsigned long time);
int TZX_readfile(byte bytes, unsigned long p);
void TZX_noInterrupts();                        // Disable interrupts
void TZX_interrupts();                          // Enable interrupts
void TZX_pinMode(unsigned pin, unsigned mode);  // Set the mode of a GPIO pin (i.e. set correct pin to output)
void TZX_LowWrite();                            // Set the GPIO output pin low
void TZX_HighWrite();                           // Set the GPIO output pin high
void TZX_Wave();                                // Function to call on Timer interrupt
void TZX_stopFile();                            // Stop the current file playback
void TZX_lcdTime();                             // Called to display the playback percent (at start)
void TZX_Counter2();                            // Called to display the playback percent (during playback)
void TZX_ReadUEFHeader();
void TZX_writeUEFData();
void TZX_UEFCarrierToneBlock();
void TZX_OricBitWrite();
void TZX_OricDataBlock();
void TZX_Log(const char* pMessage, ...);  // Log a message

/* API function prototypes */
void TZXSetup();

/* External Variables (to be implemented by consumer) */
extern char TZX_fileName[];              // Current filename
extern uint16_t TZX_fileIndex;           // Index of current file, relative to current directory (set to 0)
extern TZX_FILETYPE TZX_entry, TZX_dir;  // SD card current file (=entry) and current directory (=dir) objects
extern size_t TZX_filesize;              // filesize used for dimensioning AY files
extern TZX_TIMER TZX_Timer;              // Timer configure a timer to fire interrupts to control the output wave (call
extern bool TZX_pauseOn;                 // Control pause state

/* External Variables (implemented in library) */
extern byte TZX_currpct;       // Current percentage of file played (in file bytes, so not 100% accurate)
extern bool TZX_PauseAtStart;  // Set to true to pause at start of file

#endif  // _tzx_compat_circle_internal_h_
