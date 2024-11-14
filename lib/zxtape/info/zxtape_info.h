

#ifndef _zx_tape_info_h_
#define _zx_tape_info_h_

#define ZXTAPE_INFO_STRING_BUFFER_LENGTH 256  // 255 + 1 for the terminator

typedef enum _ZXTAPE_FILETYPE_T {
  ZXTAPE_FILETYPE_UNKNOWN = 0,
  ZXTAPE_FILETYPE_TZX = 1,
  ZXTAPE_FILETYPE_TAP = 2
} ZXTAPE_FILETYPE_T;

typedef struct _ZX_TAPE_SECTION_INFO_T {
  struct _ZX_TAPE_SECTION_INFO_T *pNext;
  unsigned char id;
  unsigned int index;
  unsigned int blockIndex;
  unsigned int blockCount;
  unsigned int playableBlockCount;
  const char pName[ZXTAPE_INFO_STRING_BUFFER_LENGTH];
  unsigned int hasProgramHeader;
  unsigned int hasGroup;
  unsigned int hasDescription;
  unsigned int hasStopTape;
  unsigned int hasStopTape48K;
  unsigned int offset;
  unsigned int length;
} ZXTAPE_SECTION_INFO_T;

typedef struct _ZXTAPE_INFO_T {
  ZXTAPE_FILETYPE_T filetype;
  unsigned int sectionCount;
  unsigned int blockCount;
  ZXTAPE_SECTION_INFO_T *pSections;
  ZXTAPE_SECTION_INFO_T *pCurrentSection;
} ZXTAPE_INFO_T;

/* Exported functions */
int zxtapeInfo_loadInfo(ZXTAPE_INFO_T **ppInfo);
void zxtapeInfo_printInfo(ZXTAPE_INFO_T *pInfo);

#endif  // _zx_tape_info_h_
