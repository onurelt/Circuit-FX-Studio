#ifndef UI_H
#define UI_H
#include "render.h"
#include <stdint.h>
typedef struct {
  const char *songs[2];
  double lengths[2];
  MixSettings settings;
  int row, progress, playing, browser, browser_pos, browser_count, confirm;
  const char *entries[10];
  int first;
  char status[180];
  unsigned short peaks[2][440];
} View;
void ui_draw(uint32_t *pixels, const View *view);
#endif
