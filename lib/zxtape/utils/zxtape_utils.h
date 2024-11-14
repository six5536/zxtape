

#ifndef _zx_tape_utils_h_
#define _zx_tape_utils_h_

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

/* Exported functions */
void zxtapeUtils_trimStringBoth(char *pStr, unsigned int length);
void zxtapeUtils_trimStringStart(char *pStr, unsigned int length);
void zxtapeUtils_trimStringEnd(char *pStr, unsigned int length);
void zxtapeUtils_trimString(char *pStr, unsigned int length, bool bTrimStart, bool bTrimEnd);

#endif  // _zx_tape_utils_h_
