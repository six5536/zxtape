

#ifndef _tzx_compat_circle_os_headers_h_
#define _tzx_compat_circle_os_headers_h_

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// util.h
//

extern void *memset(void *pBuffer, int nValue, size_t nLength);

extern void *memcpy(void *pDest, const void *pSrc, size_t nLength);
#define memcpyblk memcpy

extern void *memmove(void *pDest, const void *pSrc, size_t nLength);

extern int memcmp(const void *pBuffer1, const void *pBuffer2, size_t nLength);

size_t strlen(const char *pString);

extern int strcmp(const char *pString1, const char *pString2);
extern int strcasecmp(const char *pString1, const char *pString2);
extern int strncmp(const char *pString1, const char *pString2, size_t nMaxLen);
extern int strncasecmp(const char *pString1, const char *pString2, size_t nMaxLen);

extern char *strcpy(char *pDest, const char *pSrc);

extern char *strncpy(char *pDest, const char *pSrc, size_t nMaxLen);

extern char *strcat(char *pDest, const char *pSrc);

extern char *strchr(const char *pString, int chChar);
extern char *strstr(const char *pString, const char *pNeedle);

extern char *strtok_r(char *pString, const char *pDelim, char **ppSavePtr);

extern unsigned long strtoul(const char *pString, char **ppEndPtr, int nBase);
extern unsigned long long strtoull(const char *pString, char **ppEndPtr, int nBase);
extern int atoi(const char *pString);

extern int char2int(char chValue);  // with sign extension

//
// alloc.h
//

extern void *malloc(size_t nSize);  // resulting block is always 16 bytes aligned
extern void *memalign(size_t nAlign, size_t nSize);
extern void free(void *pBlock);

extern void *calloc(size_t nBlocks, size_t nSize);
extern void *realloc(void *pBlock, size_t nSize);

extern void *palloc(void);  // returns aligned page (AArch32: 4K, AArch64: 64K)
extern void pfree(void *pPage);

#ifdef __cplusplus
}
#endif

#endif  // _tzx_compat_circle_os_headers_h_
