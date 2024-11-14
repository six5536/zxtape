#include "zxtape_info.h"

#include "../tzx/tzx.h"
#include "../tzx_compat/tzx_compat_internal.h"
#include "../utils/zxtape_utils.h"

#define PROGNAME_LENGTH 11                 // 10 + 1 for the terminator
#define STANDARD_STRING_BUFFER_LENGTH 256  // 255 + 1 for the terminator

/* Forward declarations */
static bool processTZX(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processTAP(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processStandardSpeedDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo, bool isTzx, bool *pIsProgramHeader);
static bool processTurboSpeedDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processPureToneBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processPulseSequenceBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processPureDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processDirectRecordingBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processGeneralizedDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processPauseOrStopBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo, bool *pIsStopTape);
static bool processGroupStartBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processGroupEndBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processLoopStartBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processLoopEndBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processStopTape48KBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processSetSignalLevelBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processTextDescriptionBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processMessageBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processArchiveInfoBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processHardwareTypeBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool processCustomInfoBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo);
static bool readHeader(unsigned long *pos, ZXTAPE_FILETYPE_T *pFileType);
static bool checkForTap(char *filename);
static bool readByte(unsigned long *pos, byte *pValue);
static bool readBytes(unsigned long *pos, byte *pBuffer, unsigned long length);
static bool readWord(unsigned long *pos, word *pValue);
static bool readLong(unsigned long *pos, unsigned long *pValue);
static bool readDword(unsigned long *pos, unsigned long *pValue);
static bool readString(unsigned long *pos, char *pBuffer, unsigned long length, bool bTrimStart, bool bTrimEnd);
static void printBlockInfo(unsigned int blockIndex, byte id, unsigned long start, unsigned long length,
                           const char *pExtraInfo);
static void createTapeSectionIfRequired(ZXTAPE_INFO_T *pInfo, byte id, unsigned int offset, unsigned int length,
                                        char *pName, unsigned int nameLength, bool force);
static void createTapeSectionInfo(ZXTAPE_INFO_T *pInfo, byte id, unsigned int offset);
static void destroyTapeSectionInfos(ZXTAPE_INFO_T *pInfo);
static void stripNonPlayableSectionInfos(ZXTAPE_INFO_T *pInfo);

/* Imported variables */
extern const char TZXTape[];

/* Local variables */
static char m_pNameBuffer[STANDARD_STRING_BUFFER_LENGTH];
static ZXTAPE_INFO_T m_info;

int zxtapeInfo_loadInfo(ZXTAPE_INFO_T **ppInfo) {
  bool result = false;
  ZXTAPE_FILETYPE_T fileType;

  // Initialize the info structure
  *ppInfo = &m_info;
  ZXTAPE_INFO_T *pInfo = *ppInfo;
  pInfo->filetype = ZXTAPE_FILETYPE_UNKNOWN;
  pInfo->sectionCount = 0;
  pInfo->blockCount = 0;
  destroyTapeSectionInfos(pInfo);

  // Clear the name buffer
  memset(m_pNameBuffer, 0, sizeof(m_pNameBuffer));

  // Load the info from the tape file / buffer
  TZX_entry.close();
  TZX_entry.open(&TZX_dir, 0, 0);
  unsigned long pos = 0;

  // Read the file header
  readHeader(&pos, &pInfo->filetype);

  // Process the file
  if (pInfo->filetype == ZXTAPE_FILETYPE_TZX) {
    // Process the TZX file
    result = processTZX(&pos, pInfo);
  } else if (pInfo->filetype == ZXTAPE_FILETYPE_TAP) {
    // Process the TAP file
    result = processTAP(&pos, pInfo);
  } else {
    // Unknown file type
    result = false;
  }

  // Strip out sections without any playable blocks
  stripNonPlayableSectionInfos(pInfo);

  if (!result) {
    pInfo->filetype = ZXTAPE_FILETYPE_UNKNOWN;
    pInfo->sectionCount = 0;
    pInfo->blockCount = 0;
    return -1;
  }

  return 0;
}

void zxtapeInfo_printInfo(ZXTAPE_INFO_T *pInfo) {
  zxtape_log_debug("====== Tape Info ======");
  zxtape_log_debug("Filetype: %u", pInfo->filetype);
  zxtape_log_debug("Section Count: %u", pInfo->sectionCount);
  zxtape_log_debug("Block Count: %u", pInfo->blockCount);
  ZXTAPE_SECTION_INFO_T *pSection = pInfo->pSections;
  while (pSection != NULL) {
    zxtape_log_debug("-- Section %02u --", pSection->index);
    zxtape_log_debug("Id: %02x", pSection->id);
    zxtape_log_debug("Start Block Index: %u", pSection->blockIndex);
    zxtape_log_debug("Block Count: %u", pSection->blockCount);
    zxtape_log_debug("Playable Block Count: %u", pSection->playableBlockCount);
    zxtape_log_debug("Name: %s", pSection->pName);
    zxtape_log_debug("Program Header: %u", pSection->hasProgramHeader);
    zxtape_log_debug("Group: %u", pSection->hasGroup);
    zxtape_log_debug("Description: %u", pSection->hasDescription);
    zxtape_log_debug("Stop Tape: %u", pSection->hasStopTape);
    zxtape_log_debug("Stop Tape (48K): %u", pSection->hasStopTape48K);
    zxtape_log_debug("Offset/Length: %u,%u", pSection->offset, pSection->length);
    pSection = pSection->pNext;
  }

  zxtape_log_debug("==== End Tape Info ====");
}

static bool processTZX(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  unsigned long startBlockPos = *pos;
  pInfo->blockCount = 0;

  zxtape_log_debug("======= TZX Info ======");

  // Process the TZX file
  while (1) {
    // Check for end of file
    if (*pos >= TZX_filesize) break;

    byte id = 0;
    byte byteValue = 0;
    word wordValue = 0;
    unsigned long dwordValue = 0;
    bool isPlayableBlock = false;
    bool isProgramHeader = false;
    bool isGroup = false;
    bool isDescription = false;
    bool isStopTape = false;
    bool isStopTape48K = false;
    startBlockPos = *pos;

    if (!readByte(pos, &id)) return false;

    switch (id) {
      // Standard Speed Data Block
      case ID10:
        if (!processStandardSpeedDataBlock(pos, pInfo, true, &isProgramHeader)) return false;
        isPlayableBlock = true;
        break;

      // Turbo Speed Data Block
      case ID11:
        if (!processTurboSpeedDataBlock(pos, pInfo)) return false;
        isPlayableBlock = true;
        break;

      // Pure Tone
      case ID12:
        if (!processPureToneBlock(pos, pInfo)) return false;
        isPlayableBlock = true;
        break;

      // Pulse sequence
      case ID13:
        if (!processPulseSequenceBlock(pos, pInfo)) return false;
        isPlayableBlock = true;
        break;

      // Pure Data Block
      case ID14:
        if (!processPureDataBlock(pos, pInfo)) return false;
        isPlayableBlock = true;
        break;

      // Direct Recording
      case ID15:
        if (!processDirectRecordingBlock(pos, pInfo)) return false;
        isPlayableBlock = true;
        break;

      // Generalized Data Block
      case ID19:
        if (!processGeneralizedDataBlock(pos, pInfo)) return false;
        isPlayableBlock = true;
        break;

      // Pause (silence) or 'Stop the Tape' command
      case ID20:
        if (!processPauseOrStopBlock(pos, pInfo, &isStopTape)) return false;
        break;

      // Group start
      case ID21:
        if (!processGroupStartBlock(pos, pInfo)) return false;
        isGroup = true;
        break;

      // Group end
      case ID22:
        if (!processGroupEndBlock(pos, pInfo)) return false;
        break;

      // Loop start
      case ID24:
        if (!processLoopStartBlock(pos, pInfo)) return false;
        break;

      // Loop end
      case ID25:
        if (!processLoopEndBlock(pos, pInfo)) return false;
        break;

      // Stop the tape if in 48K mode
      case ID2A:
        if (!processStopTape48KBlock(pos, pInfo)) return false;
        isStopTape48K = true;
        break;

      // Set signal level
      case ID2B:
        if (!processSetSignalLevelBlock(pos, pInfo)) return false;
        break;

      // Text description
      case ID30:
        if (!processTextDescriptionBlock(pos, pInfo)) return false;
        isDescription = true;
        break;

      // Message block
      case ID31:
        if (!processMessageBlock(pos, pInfo)) return false;
        break;

      // Archive info
      case ID32:
        if (!processArchiveInfoBlock(pos, pInfo)) return false;
        break;

      // Hardware type
      case ID33:
        if (!processHardwareTypeBlock(pos, pInfo)) return false;
        break;

      // Custom info block
      case ID35:
        if (!processCustomInfoBlock(pos, pInfo)) return false;
        break;

      // Kansas City Block (MSX specific implementation only)
      case ID4B:
        // ???
        break;

      // "Glue" block
      case ID5A:
        // Skip Data block ...
        *pos += 9;
        break;

      // CSW Recording - unsupported
      case ID18:
      // Jump to block - unsupported
      case ID23:
      // Call sequence - unsupported
      case ID26:
      // Return from sequence - unsupported
      case ID27:
      // Select block - unsupported
      case ID28:
      default:
        // TODO: Unsupported block
        break;
    }

    unsigned int length = *pos - startBlockPos;

    // Section Info
    bool hasStopTape = pInfo->pCurrentSection ? pInfo->pCurrentSection->hasStopTape : false;
    bool hasStopTape48K = pInfo->pCurrentSection ? pInfo->pCurrentSection->hasStopTape48K : false;
    createTapeSectionIfRequired(pInfo, id, startBlockPos, 0, m_pNameBuffer, STANDARD_STRING_BUFFER_LENGTH,
                                isGroup || isDescription || hasStopTape || hasStopTape48K);
    if (isProgramHeader) pInfo->pCurrentSection->hasProgramHeader = true;
    if (isGroup) pInfo->pCurrentSection->hasGroup = true;
    if (isDescription) pInfo->pCurrentSection->hasDescription = true;
    if (isStopTape) pInfo->pCurrentSection->hasStopTape = true;
    if (isStopTape48K) pInfo->pCurrentSection->hasStopTape48K = true;
    if (isPlayableBlock) pInfo->pCurrentSection->playableBlockCount++;
    pInfo->pCurrentSection->length += length;
    pInfo->pCurrentSection->blockCount++;

    // Debug
    printBlockInfo(pInfo->blockCount, id, startBlockPos, length, NULL);

    // Info
    pInfo->blockCount++;
  }

  zxtape_log_debug("Filesize: %u, TZX data size: %u", TZX_filesize, *pos);

  zxtape_log_debug("==== End TZX Info ====");

  return true;
}

static bool processTAP(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  unsigned long startBlockPos = *pos;
  pInfo->blockCount = 0;

  zxtape_log_debug("======= TAP Info ======");

  // Process the TAP file
  while (1) {
    if (*pos >= TZX_filesize) break;

    bool isProgramHeader = false;
    startBlockPos = *pos;

    if (!processStandardSpeedDataBlock(pos, pInfo, false, &isProgramHeader)) return false;

    unsigned int length = *pos - startBlockPos;

    // Section Info
    createTapeSectionIfRequired(pInfo, ID10, startBlockPos, length, m_pNameBuffer, STANDARD_STRING_BUFFER_LENGTH,
                                false);
    if (isProgramHeader) pInfo->pCurrentSection->hasProgramHeader = true;
    pInfo->pCurrentSection->playableBlockCount++;
    pInfo->pCurrentSection->length += length;
    pInfo->pCurrentSection->blockCount++;

    // Debug
    printBlockInfo(pInfo->blockCount, ID10, startBlockPos, length, NULL);

    // Info
    pInfo->blockCount++;
  }

  zxtape_log_debug("Filesize: %u, TZX data size: %u", TZX_filesize, *pos);

  zxtape_log_debug("==== End TAP Info ====");

  return true;
}

static bool readHeader(unsigned long *pos, ZXTAPE_FILETYPE_T *pFileType) {
  char tzxHeader[11];

  TZX_entry.seekSet(0);
  memset(tzxHeader, 0, sizeof(tzxHeader));
  int i = TZX_entry.read(tzxHeader, 10);
  if (memcmp_P(tzxHeader, TZXTape, 7) != 0) {
    // If not a TZX file, check for TAP file
    bool isTap = checkForTap(TZX_fileName);
    if (isTap) {
      *pFileType = ZXTAPE_FILETYPE_TAP;
    } else {
      *pFileType = ZXTAPE_FILETYPE_UNKNOWN;
    }
    *pos = 0;
    TZX_entry.seekSet(0);
    return true;
  }

  // Get the file type from the tape file / buffer
  *pFileType = ZXTAPE_FILETYPE_TZX;
  *pos = i;

  TZX_entry.seekSet(i);

  return true;
}

static bool checkForTap(char *filename) {
  // Check for TAP file extensions as these have no header
  byte len = strlen(filename);
  if (strstr_P(strlwr(filename + (len - 4)), PSTR(".tap"))) {
    return true;
  }
  return false;
}

static bool processStandardSpeedDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo, bool isTzx,
                                          bool *pIsProgramHeader) {
  word pause = 0;
  word length = 0;
  word wordData = 0;
  char pNameBuffer[PROGNAME_LENGTH];
  *pIsProgramHeader = false;

  // Pause after this block (ms.) {1000}
  if (isTzx) {
    if (!readWord(pos, &pause)) return false;
  }
  // Length of data that follow
  if (!readWord(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  if (length == 19) {
    // First 2 bytes are 0x00 0x00
    if (!readWord(pos, &wordData)) return false;
    if (wordData == 0) {
      // Probably a standard program header, extract the program name
      if (m_pNameBuffer[0] == 0) {
        if (!readString(pos, m_pNameBuffer, PROGNAME_LENGTH, true, true)) return false;
        printf("Program Name: %s\n", m_pNameBuffer);
      } else {
        if (!readString(pos, pNameBuffer, PROGNAME_LENGTH, true, true)) return false;
        printf("Program Name: %s\n", pNameBuffer);
      }

      *pIsProgramHeader = true;
    }
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processTurboSpeedDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  word pilotPulse = 0;
  word sync1Pulse = 0;
  word sync2Pulse = 0;
  word zeroPulse = 0;
  word onePulse = 0;
  word pilotTone = 0;
  byte usedBits = 0;
  word pause = 0;
  unsigned long length = 0;

  // Length of PILOT pulse (T-states) {2168}
  if (!readWord(pos, &pilotPulse)) return false;
  // Length of SYNC1 pulse (T-states) {667}
  if (!readWord(pos, &sync1Pulse)) return false;
  // Length of SYNC2 pulse (T-states) {735}
  if (!readWord(pos, &sync2Pulse)) return false;
  // Length of ZERO bit pulse (T-states) {855}
  if (!readWord(pos, &zeroPulse)) return false;
  // Length of ONE bit pulse (T-states) {1710}
  if (!readWord(pos, &onePulse)) return false;
  // Length of PILOT tone (number of pulses) {8063 header (flag<128), 3223 data (flag>=128)}
  if (!readWord(pos, &pilotTone)) return false;
  // Used bits in the last byte (other bits should be 0) {8}
  // (e.g. if this is 6, then the bits used (x) in the last byte are: xxxxxx00, where MSb is the leftmost bit, LSb
  // is the rightmost bit)
  if (!readByte(pos, &usedBits)) return false;
  // Pause after this block (ms.) {1000}
  if (!readWord(pos, &pause)) return false;
  // Length of data that follow
  if (!readLong(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  for (unsigned long i = 0; i < length; i++) {
    byte value;
    if (!readByte(pos, &value)) return false;
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processPureToneBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  word pulseLength = 0;
  word pulseCount = 0;

  // Length of one pulse in T-states
  if (!readWord(pos, &pulseLength)) return false;
  // Number of pulses
  if (!readWord(pos, &pulseCount)) return false;

  return true;
}

static bool processPulseSequenceBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  byte pulseCount = 0;

  // Number of pulses
  if (!readByte(pos, &pulseCount)) return false;
  unsigned long end = *pos + pulseCount * 2;

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processPureDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  word zeroPulse = 0;
  word onePulse = 0;
  byte usedBits = 0;
  word pause = 0;
  unsigned long length = 0;

  // Length of ZERO bit pulse (T-states) {855}
  if (!readWord(pos, &zeroPulse)) return false;
  // Length of ONE bit pulse (T-states) {1710}
  if (!readWord(pos, &onePulse)) return false;
  // Used bits in last byte (other bits should be 0)
  // (e.g. if this is 6, then the bits used (x) in the last byte are: xxxxxx00, where MSb is the leftmost bit, LSb
  // is the rightmost bit)
  if (!readByte(pos, &usedBits)) return false;
  // Pause after this block (ms.) {1000}
  if (!readWord(pos, &pause)) return false;
  // Length of data that follow
  if (!readLong(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  for (unsigned long i = 0; i < length; i++) {
    byte value;
    if (!readByte(pos, &value)) return false;
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processDirectRecordingBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  word tStatesPerSample = 0;
  word pause = 0;
  byte usedBits = 0;
  unsigned long length = 0;

  // Number of T-states per sample (bit of data) 79 or 158 - 22.6757uS for 44.1KHz
  if (!readWord(pos, &tStatesPerSample)) return false;
  // Pause after this block in milliseconds
  if (!readWord(pos, &pause)) return false;
  // Used bits (samples) in last byte of data (1-8)
  // (e.g. if this is 2, only first two samples of the last byte will be played)
  if (!readByte(pos, &usedBits)) return false;
  // Length of samples' data
  if (!readLong(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  for (unsigned long i = 0; i < length; i++) {
    byte value;
    if (!readByte(pos, &value)) return false;
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processGeneralizedDataBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  unsigned long length = 0;

  // Block length (without these four bytes)
  if (!readDword(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  for (unsigned long i = 0; i < length; i++) {
    byte value;
    if (!readByte(pos, &value)) return false;
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processPauseOrStopBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo, bool *pIsStopTape) {
  word pause = 0;

  // Pause duration (ms.)
  if (!readWord(pos, &pause)) return false;

  *pIsStopTape = pause == 0;

  return true;
}

static bool processGroupStartBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  byte length = 0;

  // Length of the group name string
  if (!readByte(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  if (!readString(pos, m_pNameBuffer, length, true, true)) return false;
  printf("Group Name: %s\n", m_pNameBuffer);

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processGroupEndBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  // No body
  return true;
}

static bool processLoopStartBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  word repetitions = 0;
  // Number of repetitions (greater than 1)
  if (!readWord(pos, &repetitions)) return false;

  return true;
}

static bool processLoopEndBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  // No body
  return true;
}

static bool processStopTape48KBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  unsigned long length = 0;
  // Length of the block without these four bytes (0)
  if (!readDword(pos, &length)) return false;

  return true;
}

static bool processSetSignalLevelBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  unsigned long length = 0;
  byte signalLevel = 0;

  // Block length (without these four bytes)
  if (!readDword(pos, &length)) return false;
  // Signal level (0=low, 1=high)
  if (!readByte(pos, &signalLevel)) return false;

  return true;
}

static bool processTextDescriptionBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  byte length = 0;

  // Length of the text description
  if (!readByte(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  if (!readString(pos, m_pNameBuffer, length, true, true)) return false;
  printf("Description: %s\n", m_pNameBuffer);

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processMessageBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  byte displayTime = 0;
  byte length = 0;

  // Time (in seconds) for which the message should be displayed
  if (!readByte(pos, &displayTime)) return false;
  // Length of the text message
  if (!readByte(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  if (!readString(pos, m_pNameBuffer, length, true, true)) return false;
  printf("Message: %s\n", m_pNameBuffer);

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processArchiveInfoBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  word length = 0;

  // Length of the whole block (without these two bytes)
  if (!readWord(pos, &length)) return false;
  unsigned long end = *pos + length;
  // Number of text strings
  // if (!readByte(pos, &byteValue)) return false;

  // Process data
  for (word i = 0; i < length; i++) {
    byte value;
    if (!readByte(pos, &value)) return false;
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processHardwareTypeBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  byte length = 0;

  // Number of machines and hardware types for which info is supplied
  if (!readByte(pos, &length)) return false;
  unsigned long end = *pos + length * 3;

  // Skip Remaining Data
  *pos = end;

  return true;
}

static bool processCustomInfoBlock(unsigned long *pos, ZXTAPE_INFO_T *pInfo) {
  unsigned long length = 0;

  // Identification string (in ASCII)
  *pos += 10;
  // Length of the custom info
  if (!readDword(pos, &length)) return false;
  unsigned long end = *pos + length;

  // Process data
  for (unsigned long i = 0; i < length; i++) {
    byte value;
    if (!readByte(pos, &value)) return false;
  }

  // Skip Remaining Data
  *pos = end;

  return true;
}

// static void tapeToBytes(byte *pTapeBuffer, unsigned int nTapeBufferLength, byte *pBytesBuffer,
//                         unsigned int nBytesBufferLength, unsigned int *pBytesLength) {
//   // Convert the tape buffer to bytes
//   unsigned int nBytesIndex = 0;
//   unsigned int nTapeIndex = 0;
//   byte nByte = 0;
//   byte nBit = 0;
//   byte nValue = 0;

//   while (nTapeIndex < nTapeBufferLength) {
//     nValue = pTapeBuffer[nTapeIndex];
//     for (nBit = 0; nBit < 8; nBit++) {
//       nByte = (nValue & (1 << nBit)) >> nBit;
//       pBytesBuffer[nBytesIndex] = nByte;
//       nBytesIndex++;
//     }
//     nTapeIndex++;
//   }
// }

static bool readByte(unsigned long *pos, byte *pValue) {
  // Read a byte from the file, and move file position on one if successful
  byte out[1];
  int i = 0;
  if (TZX_entry.seekSet(*pos)) {
    i = TZX_entry.read(out, 1);
    if (i == 1) *pos += 1;
  }
  *pValue = out[0];

  return (i == 1);
}

static bool readBytes(unsigned long *pos, byte *pBuffer, unsigned long length) {
  // Read a set of bytes from the file into a buffer and move file position on the number of bytes read
  byte *out = pBuffer;
  int i = 0;
  if (TZX_entry.seekSet(*pos)) {
    i = TZX_entry.read(out, length);
    if (i == length) *pos += length;
  }

  return (i == length);
}

static bool readWord(unsigned long *pos, word *pValue) {
  // Read 2 bytes from the file, and move file position on two if successful
  byte out[2];
  int i = 0;
  if (TZX_entry.seekSet(*pos)) {
    i = TZX_entry.read(out, 2);
    if (i == 2) *pos += 2;
  }
  *pValue = TZX_word(out[1], out[0]);

  return (i == 2);
}

static bool readLong(unsigned long *pos, unsigned long *pValue) {
  // Read 3 bytes from the file, and move file position on three if successful
  byte out[3];
  int i = 0;
  if (TZX_entry.seekSet(*pos)) {
    i = TZX_entry.read(out, 3);
    if (i == 3) *pos += 3;
  }
  *pValue = ((unsigned long)TZX_word(out[2], out[1]) << 8) | out[0];

  return (i == 3);
}

static bool readDword(unsigned long *pos, unsigned long *pValue) {
  // Read 4 bytes from the file, and move file position on four if successful
  byte out[4];
  int i = 0;
  if (TZX_entry.seekSet(*pos)) {
    i = TZX_entry.read(out, 4);
    if (i == 4) *pos += 4;
  }
  *pValue = ((unsigned long)TZX_word(out[3], out[2]) << 16) | TZX_word(out[1], out[0]);

  return (i == 4);
}

/**
 * Read a string from the file
 *
 * @param pos File position
 * @param pBuffer Buffer to store the string
 * @param length Length of buffer, including null terminator
 */
static bool readString(unsigned long *pos, char *pBuffer, unsigned long length, bool bTrimStart, bool bTrimEnd) {
  if (!readBytes(pos, (byte *)pBuffer, length - 1)) return false;
  pBuffer[length - 1] = '\0';

  zxtapeUtils_trimString(pBuffer, length, bTrimStart, bTrimEnd);

  return true;
}

static const char *getTzxBlockName(byte id) {
  switch (id) {
    case ID10:
      return "Standard Speed Data";
    case ID11:
      return "Turbo Speed Data   ";
    case ID12:
      return "Pure Tone          ";
    case ID13:
      return "Pulse Sequence     ";
    case ID14:
      return "Pure Data          ";
    case ID15:
      return "Direct Recording   ";
    case ID18:
      return "CSW Recording      ";
    case ID19:
      return "Generalized Data   ";
    case ID20:
      return "Pause / Stop Tape  ";
    case ID21:
      return "Group Start        ";
    case ID22:
      return "Group End          ";
    case ID24:
      return "Loop Start         ";
    case ID25:
      return "Loop End           ";
    case ID2A:
      return "Stop Tape (48K)    ";
    case ID2B:
      return "Set Signal Level   ";
    case ID30:
      return "Text Description   ";
    case ID31:
      return "Message Block      ";
    case ID32:
      return "Archive Info       ";
    case ID33:
      return "Hardware Type      ";
    case ID35:
      return "Custom Info        ";
    case ID4B:
      return "Kansas City        ";
    default:
      return "Unsupported        ";
  }
}

static void printBlockInfo(unsigned int index, byte id, unsigned long start, unsigned long length,
                           const char *pExtraInfo) {
  if (pExtraInfo == NULL) pExtraInfo = "";
  zxtape_log_debug("%03d [0x%02x]: %s [%u,%u] %s", index, id, getTzxBlockName(id), start, length, pExtraInfo);
}

static void createTapeSectionIfRequired(ZXTAPE_INFO_T *pInfo, byte id, unsigned int offset, unsigned int length,
                                        char *pName, unsigned int nameLength, bool force) {
  // Create a new section
  ZXTAPE_SECTION_INFO_T *pSection = pInfo->pCurrentSection;
  if (force || pSection == NULL) {
    createTapeSectionInfo(pInfo, id, offset);
    ZXTAPE_SECTION_INFO_T *pSection = pInfo->pCurrentSection;
    if (pName != NULL) memcpy((void *)pSection->pName, pName, nameLength);
  }
}

static void createTapeSectionInfo(ZXTAPE_INFO_T *pInfo, byte id, unsigned int offset) {
  // Create the tape section info
  ZXTAPE_SECTION_INFO_T *pNewSection = (ZXTAPE_SECTION_INFO_T *)malloc(sizeof(ZXTAPE_SECTION_INFO_T));
  assert(pNewSection != NULL);

  // Set the section info
  pNewSection->id = id;
  pNewSection->index = pInfo->sectionCount;
  pNewSection->blockIndex = pInfo->blockCount;
  pNewSection->blockCount = 0;
  pNewSection->playableBlockCount = 0;
  memset((char *)pNewSection->pName, 0, ZXTAPE_INFO_STRING_BUFFER_LENGTH);
  pNewSection->offset = offset;
  pNewSection->hasProgramHeader = 0;
  pNewSection->hasGroup = 0;
  pNewSection->hasDescription = 0;
  pNewSection->hasStopTape = 0;
  pNewSection->hasStopTape48K = 0;
  pNewSection->pNext = 0;

  // Add the section to the list
  ZXTAPE_SECTION_INFO_T *pSection = pInfo->pSections;
  while (pSection != 0) {
    if (pSection->pNext == 0) break;
    pSection = pSection->pNext;
  }
  if (pSection == 0) {
    pInfo->pSections = pNewSection;
  } else {
    pSection->pNext = pNewSection;
  }
  pInfo->pCurrentSection = pNewSection;
  pInfo->sectionCount++;
}

static void destroyTapeSectionInfos(ZXTAPE_INFO_T *pInfo) {
  // Delete the tape section info
  ZXTAPE_SECTION_INFO_T *pSection = pInfo->pSections;
  while (pSection != 0) {
    ZXTAPE_SECTION_INFO_T *pNext = pSection->pNext;
    free(pSection);
    pSection = pNext;
  }
  pInfo->pSections = 0;
  pInfo->sectionCount = 0;
}

static void stripNonPlayableSectionInfos(ZXTAPE_INFO_T *pInfo) {
  ZXTAPE_SECTION_INFO_T *pSection = pInfo->pSections;
  ZXTAPE_SECTION_INFO_T *pPrevSection = NULL;
  unsigned int index = 0;

  while (pSection != NULL) {
    // Remove sections without any playable blocks
    if (pSection->playableBlockCount == 0) {
      ZXTAPE_SECTION_INFO_T *pSectionToDelete = pSection;
      if (pPrevSection == NULL) {
        pInfo->pSections = pSectionToDelete->pNext;
      } else {
        pPrevSection->pNext = pSectionToDelete->pNext;
      }
      pSection = pSectionToDelete->pNext;
      free(pSectionToDelete);
      pInfo->sectionCount--;
    } else {
      // Reindex the sections
      pSection->index = index++;

      pPrevSection = pSection;
      pSection = pSection->pNext;
    }
  }
}