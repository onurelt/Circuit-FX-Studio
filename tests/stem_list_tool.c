#include "prepared_stems.h"
#include <stdio.h>

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char paths[STEM_COUNT][STEM_PATH_CAPACITY];
  int mask = prepared_stem_paths(argv[1], argv[2], paths);
  printf("%d\n", mask);
  for (int i = 0; i < STEM_COUNT; ++i)
    printf("%s\n", paths[i]);
  return mask < 0 ? 2 : 0;
}
