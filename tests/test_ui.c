#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  View v = {0};
  v.home_screen = 1;
  assert(ui_hit_test_for_tab(&v, 100, 200) == 300);
  assert(ui_hit_test_for_tab(&v, 500, 350) == 301);
  assert(ui_hit_test_for_tab(&v, 100, 300) == -1);
  v.home_screen = 0;

  v.songs[0] = "Track A";
  v.songs[1] = "Track B";
  const int controls[][3] = {
      {100, 20, 0}, {600, 20, 1}, {400, 35, 160}, {890, 35, 161},
      {50, 338, 10}, {145, 338, 101}, {210, 338, 102},
      {300, 338, 2}, {420, 338, 3}, {545, 338, 4},
      {670, 338, 103}, {730, 338, 104}, {790, 338, 105},
      {860, 338, 11}, {50, 400, 170}, {400, 400, 171},
      {800, 400, 172}, {100, 500, 162}, {300, 500, 173},
      {550, 500, 174}};
  for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
    assert(ui_hit_test_for_tab(&v, controls[i][0], controls[i][1]) ==
           controls[i][2]);
  assert(ui_hit_test_for_tab(&v, 720, 500) == -1);
  assert(ui_hit_test_for_tab(&v, 0, 0) == -1);
  assert(ui_hit_test_for_tab(&v, 960, 544) == -1);

  v.workflow = 1;
  v.stem_solo = -1;
  for (int i = 0; i < 4; ++i) v.stem_gain[i] = 1.0f;
  const int stem_controls[][3] = {
      {100, 20, 0}, {800, 20, 175},
      {100, 170, 460}, {200, 170, 500},
      {550, 170, 400}, {620, 170, 401}, {720, 170, 402}, {840, 170, 403},
      {100, 236, 461}, {200, 236, 501},
      {550, 236, 404}, {620, 236, 405}, {720, 236, 406}, {840, 236, 407},
      {100, 302, 462}, {200, 302, 502},
      {550, 302, 408}, {620, 302, 409}, {720, 302, 410}, {840, 302, 411},
      {100, 368, 463}, {200, 368, 503},
      {550, 368, 412}, {620, 368, 413}, {720, 368, 414}, {840, 368, 415},
      {100, 440, 440}, {300, 440, 441}, {450, 440, 442},
      {700, 440, 443}, {850, 440, 175}};
  for (size_t i = 0; i < sizeof(stem_controls) / sizeof(stem_controls[0]); ++i)
    assert(ui_hit_test_for_tab(&v, stem_controls[i][0], stem_controls[i][1]) ==
           stem_controls[i][2]);
  v.workflow = 0;
  for (int i = 0; i < 12; ++i)
    assert(ui_key_test(300 + (i % 3) * 144, 210 + (i / 3) * 48) == i);
  assert(ui_key_test(320, 415) == 12);
  assert(ui_key_test(550, 415) == 13);
  assert(ui_key_test(403, 210) == -1);

  uint32_t *memory = calloc(960 * 544 + 2, sizeof(uint32_t));
  assert(memory);
  memory[0] = 0x12345678;
  memory[960 * 544 + 1] = 0x87654321;
  v.progress = -1;
  v.settings = (MixSettings){
      .start_a = 155, .start_b = 48, .duration = 15, .effects = FX_CROSSFADE};
  v.settings.bpm = 128;
  v.settings.intensity = 1;
  v.lengths[0] = 360;
  v.lengths[1] = 420;
  assert(ui_timeline_pre(&v) == 4);
  assert(ui_timeline_span(&v) == 23);
  v.songs[0] = "A long filename to check clipping and bounds.wav";
  v.candidates_ready = 1;
  v.candidate_start_a[0] = 155.125;
  v.candidate_start_b[0] = 48.250;
  v.candidate_duration[0] = 15;
  v.mix_variation = 0;
  for (int mode = 0; mode < 8; ++mode) {
    v.row = mode;
    v.progress = mode == 1 ? 0 : mode == 2 ? 100 : -1;
    v.editing = mode == 3 ? 1 : 0;
    v.browser = mode == 4;
    v.confirm = mode == 5;
    v.playing = mode == 6;
    v.play_position = 23;
    snprintf(v.entry, sizeof(v.entry), "155.250");
    ui_draw(memory + 1, &v);
    assert(memory[0] == 0x12345678 &&
           memory[960 * 544 + 1] == 0x87654321);
  }
  v.workflow = 1;
  v.stem_present = 5;
  v.stem_solo = -1;
  for (int i = 0; i < 4; ++i) v.stem_gain[i] = .75f;
  ui_draw(memory + 1, &v);
  v.workflow = 0;
  v.home_screen = 1;
  ui_draw(memory + 1, &v);
  v.home_screen = 0;
  v.candidates_ready = 0;
  ui_draw(memory + 1, &v);
  v.progress = -1;
  v.browser = 0;
  v.confirm = 0;
  free(memory);
  puts("PASS: two-tool home, Auto Mix controls/candidates, touch hit targets, "
       "waveform bounds, and modal rendering.");
  return 0;
}
