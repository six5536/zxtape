
#include "zxtape_file_api_file.h"

#include "../../../include/tzx_compat_impl.h"

//
// File API, platform specific
//

void zxtapeFileApiFile_initialize(TZX_FILETYPE *pFileType) {
  // Implementation is in tzx_compat_<platform>.c
  pFileType->open = (bool (*)(struct _TZX_FILETYPE *, u32, TZX_oflag_t))TZXCompat_fileOpen;
  pFileType->close = TZXCompat_fileClose;
  pFileType->read = TZXCompat_fileRead;
  pFileType->seekSet = TZXCompat_fileSeekSet;
}
