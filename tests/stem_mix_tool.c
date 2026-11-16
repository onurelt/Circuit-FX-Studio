#include "stem_mix.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned cancel_at;
static int cancel_progress(uint32_t done, uint32_t total) {
  (void)total;
  return cancel_at && done >= cancel_at;
}

int main(int argc, char **argv) {
  if (argc != 12)
    return 2;
  const char *output = argv[1];
  unsigned mask = (unsigned)strtoul(argv[2], NULL, 0);
  cancel_at = (unsigned)strtoul(argv[3], NULL, 0);
  float gains[STEM_COUNT];
  const char *paths[STEM_COUNT];
  for (int i = 0; i < STEM_COUNT; ++i) {
    gains[i] = strtof(argv[4 + i], NULL);
    paths[i] = argv[8 + i];
  }
  int rc = stem_mix_wav(paths, output, gains, mask, cancel_progress);
  if (rc)
    fprintf(stderr, "%s\n", stem_mix_error);
  printf("%d\n", rc);
  return rc ? 1 : 0;
}
