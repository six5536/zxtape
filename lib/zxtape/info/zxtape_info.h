

#ifndef _zx_tape_info_h_
#define _zx_tape_info_h_

#define ZXTAPE_INFO_STRING_BUFFER_LENGTH 256  // 255 + 1 for the terminator

typedef enum _ZX_TAPE_FILETYPE_T {
  TZX_FILETYPE_UNKNOWN = 0,
  TZX_FILETYPE_TZX = 1,
  TZX_FILETYPE_TAP = 2
} ZX_TAPE_FILETYPE_T;

typedef struct _ZX_TAPE_SECTION_INFO_T {
  unsigned char id;
  unsigned int index;
  unsigned int blockIndex;
  const char pName[ZXTAPE_INFO_STRING_BUFFER_LENGTH];
  unsigned int offset;
} ZX_TAPE_SECTION_INFO_T;

typedef struct _ZX_TAPE_INFO_T {
  ZX_TAPE_FILETYPE_T filetype;
  unsigned int sectionCount;
  unsigned int blockCount;
  ZX_TAPE_SECTION_INFO_T *pSections;
} ZX_TAPE_INFO_T;

/* Exported functions */
int zxtapeInfo_loadInfo();

#endif  // _zx_tape_info_h_
