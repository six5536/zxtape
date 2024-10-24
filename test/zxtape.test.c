
#include <stdio.h>
#include <zxtape.h>

int main(int argc, char* argv[]) {
  // Create a new ZXTape instance
  ZXTAPE_HANDLE_T* pZxTape = zxtape_create();

  // Initialize the ZXTape instance
  zxtape_init(pZxTape);

  // Destroy the ZXTape instance
  zxtape_destroy(pZxTape);

  return 0;
}