
#ifndef _zxtape_compat_internal_h_
#define _zxtape_compat_internal_h_

#ifdef __ZX_TAPE_MACOS__
#include "./macos/tzx_compat_internal_macos.h"
#endif  // __ZX_TAPE_MACOS__

#ifdef __ZX_TAPE_CIRCLE__
#include "./circle/tzx_compat_internal_circle.h"
#endif  // __ZX_TAPE_CIRCLE__

#endif  // _zxtape_compat_internal_h_