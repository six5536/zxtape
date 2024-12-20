

#ifndef _tzx_compat_macos_os_headers_h_
#define _tzx_compat_macos_os_headers_h_

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

// #define TZX_buffsize 1536  // 1.5k buffer
#define TZX_buffsize 1024 * 16  // 16k buffer

#ifdef __cplusplus
extern "C" {
#endif

// No functions to declare

#ifdef __cplusplus
}
#endif

#endif  // _tzx_compat_macos_os_headers_h_
