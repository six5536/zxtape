
// Source: https://github.com/sadken/TZXDuino/blob/master/TZXDuino.h

#ifndef _tzx_compat_macos_h_
#define _tzx_compat_macos_h_

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Lang definitions
#ifdef __cplusplus
#else
typedef char bool;
#define false 0
#define true 1
#endif

typedef unsigned char u8;        // 8-bit unsigned integer
typedef unsigned short u16;      // 16-bit unsigned integer
typedef unsigned long u32;       // 32-bit unsigned integer
typedef unsigned long long u64;  // 64-bit unsigned integer
typedef char i8;                 // 8-bit signed integer
typedef short i16;               // 16-bit signed integer
typedef long i32;                // 32-bit signed integer
typedef long long i64;           // 64-bit signed integer

#define zxtape_log_info(...) _zxtape_log("INFO", __VA_ARGS__)
#define zxtape_log_debug(...) _zxtape_log("DEBUG", __VA_ARGS__)
#define zxtape_log_warn(...) _zxtape_log("WARN", __VA_ARGS__)
#define zxtape_log_error(...) _zxtape_log("ERROR", __VA_ARGS__)
#define zxtape_log_fatal(...) _zxtape_log("FATAL", __VA_ARGS__)

void _zxtape_log(const char *pLevel, const char *pFormat, ...);

#endif  // _tzx_compat_macos_h_
