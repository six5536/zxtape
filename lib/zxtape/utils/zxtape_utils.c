
#include "zxtape_utils.h"

#define FIRST_VALID_CHAR 33  // '!'
#define LAST_VALID_CHAR 126  // '~'

/* Forward declarations */
// NONE

/* Imported variables */
// NONE

/* Local variables */
// NONE

/**
 * @brief Trim a string, removing whitespace from both ends of the string
 *
 * The string is trimmed in place, the bytes are shifted to the start of the buffer when trimming the start of
 * the string.
 *
 * @param pStr String to trim, null terminated
 * @param length Length of the string buffer including the null terminator
 */
void zxtapeUtils_trimStringBoth(char *pStr, unsigned int length) {
  //
  zxtapeUtils_trimString(pStr, length, true, true);
}

/**
 * @brief Trim a string, removing whitespace from the start of the string
 *
 * The string is trimmed in place, the bytes are shifted to the start of the buffer when trimming the start of
 * the string.
 *
 * @param pStr String to trim, null terminated
 * @param length Length of the string buffer including the null terminator
 */
void zxtapeUtils_trimStringStart(char *pStr, unsigned int length) {
  //
  zxtapeUtils_trimString(pStr, length, true, false);
}

/**
 * @brief Trim a string, removing whitespace from the end of the string
 *
 * The string is trimmed in place.
 *
 * @param pStr String to trim, null terminated
 * @param length Length of the string buffer including the null terminator
 */
void zxtapeUtils_trimStringEnd(char *pStr, unsigned int length) {
  //
  zxtapeUtils_trimString(pStr, length, false, true);
}

/**
 * @brief Trim a string
 *
 * The string is trimmed in place, the bytes are shifted to the start of the buffer when trimming the start of
 * the string.
 *
 * @param pStr String to trim, null terminated
 * @param length Length of the string buffer including the null terminator
 * @param bTrimStart Trim the start of the string
 * @param bTrimEnd Trim the end of the string
 */
void zxtapeUtils_trimString(char *pStr, unsigned int length, bool bTrimStart, bool bTrimEnd) {
  if (pStr == 0 || length == 0) return;

  char *pEnd = pStr + length - 1;

  // Trim end the string
  if (bTrimEnd) {
    // Find and skip the string terminator
    while (pEnd >= pStr && *pEnd != '\0') pEnd--;
    pEnd--;
    // Trim the end of the string, and write new terminator
    while (pEnd >= pStr && (*pEnd < FIRST_VALID_CHAR || *pEnd > LAST_VALID_CHAR)) pEnd--;
    pEnd++;
    *pEnd = '\0';
  }

  if (bTrimStart) {
    // Trim start of the string
    // Calculate the shift
    char *pStart = pStr;
    while (pStart < pEnd && (*pStart < FIRST_VALID_CHAR || *pStart > LAST_VALID_CHAR)) pStart++;
    unsigned long nShift = pStart - pStr;
    // Shift the string
    if (nShift > 0) {
      char *pDest = pStr;
      char *pSrc = pStr + nShift;
      while (*pSrc != '\0') {
        *pDest = *pSrc;
        pDest++;
        pSrc++;
      }
      *pDest = '\0';
    }
  }
}
