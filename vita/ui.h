#ifndef UI_H
#define UI_H
#include "automix.h"
#include "presets.h"
#include "render.h"
#include <stdint.h>
#define WAVE_BINS 8192
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
  unsigned short peaks[2][WAVE_BINS];
  double wave_pan, wave_window, wave_origin[2];
  float track_bpm[2];
  int bpm_manual[2], auto_beatmatch;
  int vocal_enabled, vocal_strength, vocal_target;
  int tab, effect_index, preset_bank, preset_page;
  double play_position, preview_length;
  int preset_browser, selected_preset;
  int editing;
  int home_screen, workflow; /* home / 0=Auto Mix / 1=Vocal Remover */
  int mix_variation, candidates_ready, mix_manual_edit;
  double candidate_start_a[3], candidate_start_b[3], candidate_duration[3];
  char entry[20];
  char edit_error[90];
} View;
void ui_draw(uint32_t *pixels, const View *view);
#ifndef HOST_UI
int ui_gpu_init(void);
void ui_gpu_fini(void);
unsigned ui_gpu_fallback_count(void);
#endif
/* Shared with native touch routing and host layout tests. */
int ui_hit_test_for_tab(const View *v, int x, int y);
int ui_key_test(int x, int y);
double ui_timeline_pre(const View *v);
double ui_timeline_span(const View *v);
#endif
