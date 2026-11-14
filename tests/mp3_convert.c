#include "mp3_import.h"
#include <stdio.h>
#include <stdlib.h>

static int cancel_conversion(uint32_t done, uint32_t total) {
  (void)total;
  return getenv("CANCEL_MP3_IMPORT") && done > 0;
}

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  Mp3ImportProgress cb = getenv("CANCEL_MP3_IMPORT") ? cancel_conversion : 0;
  if (mp3_import_to_wav(argv[1], argv[2], cb)) {
    fprintf(stderr, "%s\n", mp3_import_error);
    return 1;
  }
  return 0;
}
