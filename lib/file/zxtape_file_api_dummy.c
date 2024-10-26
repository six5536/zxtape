#include "zxtape_file_api_dummy.h"

//
// Dummy File API
//

/* Forward declarations */
static bool open(TZX_FILETYPE *dir, uint32_t index, TZX_oflag_t oflag);
static void close();
static int read(void *buf, unsigned long count);
static bool seekSet(uint64_t pos);

void zxtapeFileApiDummy_initialize(TZX_FILETYPE *pFileType) {
  pFileType->open = open;
  pFileType->close = close;
  pFileType->read = read;
  pFileType->seekSet = seekSet;
}

static bool open(TZX_FILETYPE *dir, uint32_t index, TZX_oflag_t oflag) { return true; }

static void close() {
  //
}

static int read(void *buf, unsigned long count) { return 0; }

static bool seekSet(uint64_t pos) { return true; }