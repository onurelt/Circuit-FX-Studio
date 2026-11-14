#ifndef UI_H
#define UI_H
#include "render.h"
#include <stdint.h>
#define TIMELINE_BINS 880
#define CONTROL_COUNT 15
typedef struct {
  const char *songs[2];
  double lengths[2];
  MixSettings settings;
  int row, progress, playing, browser, browser_pos, browser_count, confirm,
      ready;
  const char *entries[10];
  int first;
  char status[180];
  unsigned short peaks[2][440], timeline[2][TIMELINE_BINS];
  double play_position, preview_length;
  int editing;
  char entry[20];
  char edit_error[90];
} View;
void ui_draw(uint32_t *pixels, const View *view);
/* Shared with native touch routing and host layout tests. */
int ui_hit_test(int x, int y);
int ui_key_test(int x, int y);
double ui_timeline_pre(const View *v);
double ui_timeline_span(const View *v);
#endif
