
#ifndef _zxtape_compat_internal_h_
#define _zxtape_compat_internal_h_

#include "tzx_compat.h"

// Lang definitions
typedef unsigned char byte;   // 8-bit
typedef unsigned short word;  // 16-bit
// typedef unsigned short uint16_t;      // 16-bit
// typedef unsigned int uint32_t;        // 32-bit
// typedef unsigned long long uint64_t;  // 64-bit

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

/* API function prototypes */
void TZXSetup();
int TZX_readfile(byte bytes, unsigned long p);
void TZX_pinMode(unsigned pin, unsigned mode);  // Set the mode of a GPIO pin (i.e. set correct pin to output)
// void TZX_Wave();                                // Function to call on Timer interrupt
void TZX_stopFile();  // Stop the current file playback
void TZX_lcdTime();   // Called to display the playback percent (at start)
void TZX_Counter2();  // Called to display the playback percent (during playback)
void TZX_ReadUEFHeader();
void TZX_writeUEFData();
void TZX_UEFCarrierToneBlock();
void TZX_OricBitWrite();
void TZX_OricDataBlock();

#endif  // _zxtape_compat_internal_h_