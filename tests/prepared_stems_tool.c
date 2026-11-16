#include "prepared_stems.h"
#include <stdio.h>
int main(int argc, char **argv) {
  if (argc != 5)
    return 2;
  char vocals[1024], instrumental[1024];
  int result = prepared_vocal_handoff_paths(
      argv[1], argv[2], argv[3], argv[4],
      vocals, sizeof(vocals), instrumental, sizeof(instrumental));
  printf("%d\n", result);
  if (result == 1)
    printf("%s\n%s\n", vocals, instrumental);
  return result < 0 ? 2 : 0;
}
