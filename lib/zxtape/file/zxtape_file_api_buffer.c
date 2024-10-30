
#include "zxtape_file_api_buffer.h"

#include "../tzx_compat/tzx_compat.h"

//
// Simple Buffer File API
//

/* Local global variables */
static u8 *g_pBuffer = NULL;   // Pointer to buffer
static u64 g_nBufferSize = 0;  // Buffer size
static u64 g_nSeekIndex = 0;   // Current file seek position

/* Forward declarations */
static bool open(TZX_FILETYPE *dir, u32 index, TZX_oflag_t oflag);
static void close();
static int read(void *buf, unsigned long count);
static bool seekSet(u64 pos);

void zxtapeFileApiBuffer_initialize(TZX_FILETYPE *pFileType, const u8 *pBuffer, u64 nBufferSize) {
  pFileType->open = open;
  pFileType->close = close;
  pFileType->read = read;
  pFileType->seekSet = seekSet;

  // Set the buffer pointer
  g_pBuffer = pBuffer;
  assert(g_pBuffer != NULL);  // Ensure memory was allocated

  // Set the buffer size and seek index
  g_nBufferSize = nBufferSize;
  g_nSeekIndex = 0;
}

static bool open(TZX_FILETYPE *dir, u32 index, TZX_oflag_t oflag) {
  zxtape_log_debug("open");

  g_nSeekIndex = 0;

  zxtape_log_debug("filesize: %d", g_nBufferSize);

  return true;
}

static void close() {
  zxtape_log_debug("close");

  g_nSeekIndex = 0;
}

static int read(void *buf, unsigned long count) {
  // zxtape_log_debug("read(%lu)", count);

  if (g_nSeekIndex + count > g_nBufferSize) {
    count = g_nBufferSize - g_nSeekIndex;
  }

  // for (unsigned long i = 0; i < count; i++) {
  //   zxtape_log_debug("%02x ", *(pFile + g_nSeekIndex + i));
  // }

  memcpy(buf, g_pBuffer + g_nSeekIndex, count);
  g_nSeekIndex += count;

  return count;
}

static bool seekSet(u64 pos) {
  // zxtape_log_debug("seekSet(%lu)", pos);

  if (pos >= g_nBufferSize) return false;

  g_nSeekIndex = pos;

  return true;
}