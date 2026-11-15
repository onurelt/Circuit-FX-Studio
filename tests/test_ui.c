#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  View v = {0};
  const int main_controls[][3]={{100,20,0},{600,20,1},{50,338,10},
      {145,338,101},{210,338,102},{300,338,2},{420,338,3},
      {545,338,4},{670,338,103},{730,338,104},{790,338,105},
      {860,338,11},{100,385,110},{430,385,111},{760,385,112}};
  for (size_t i=0;i<sizeof(main_controls)/sizeof(main_controls[0]);i++)
    assert(ui_hit_test_for_tab(&v,main_controls[i][0],main_controls[i][1])==main_controls[i][2]);
  v.songs[0]="Track A";
  v.songs[1]="Track B";
  assert(ui_hit_test_for_tab(&v,400,35)==160);
  assert(ui_hit_test_for_tab(&v,890,35)==161);
  assert(ui_hit_test_for_tab(&v,760,520)==162);
  v.auto_beatmatch=1;
  assert(ui_hit_test_for_tab(&v,760,520)==162);
  for (int i=0;i<5;i++)
    assert(ui_hit_test_for_tab(&v,20+188*i,450)==120+i);
  v.tab=1;
  assert(ui_hit_test_for_tab(&v,730,517)==-1);
  v.auto_beatmatch=0;
  v.settings.preset_id=65;
  for (int i=0;i<6;i++)
    assert(ui_hit_test_for_tab(&v,20+155*i,450)==120+i);
  v.effect_index=5;
  v.vocal_enabled=1;
  v.vocal_strength=70;
  v.vocal_target=2;
  assert(ui_hit_test_for_tab(&v,740,517)==163);
  v.effect_index=0;
  const int effects[][3]={{50,517,150},{360,517,151},{490,517,152},
                           {730,517,153},{890,517,154}};
  for (int i=0;i<5;i++) assert(ui_hit_test_for_tab(&v,effects[i][0],effects[i][1])==effects[i][2]);
  v.tab=2;
  v.auto_beatmatch=1;
  assert(ui_hit_test_for_tab(&v,580,520)==-1);
  v.auto_beatmatch=0;
  for (int i=0;i<8;i++) assert(ui_hit_test_for_tab(&v,20+116*i,430)==130+i);
  for (int i=0;i<5;i++) assert(ui_hit_test_for_tab(&v,20+188*i,470)==120+i);
  assert(ui_hit_test_for_tab(&v,30,520)==140);
  assert(ui_hit_test_for_tab(&v,760,520)==142);
  assert(ui_hit_test_for_tab(&v,900,520)==141);
  assert(ui_hit_test_for_tab(&v,580,520)==153);
  assert(ui_hit_test_for_tab(&v,680,520)==154);
  assert(ui_hit_test_for_tab(&v,0,0)==-1);
  assert(ui_hit_test_for_tab(&v,960,544)==-1);
  for (int i = 0; i < 12; i++)
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
      .start_a = 155, .start_b = 48, .duration = 15, .effects = 31};
  v.settings.bpm = 128;
  v.settings.intensity = 1;
  v.lengths[0] = 360;
  v.lengths[1] = 420;
  assert(ui_timeline_pre(&v) == 4);
  assert(ui_timeline_span(&v) == 23);
  v.songs[0] = "A long filename to check clipping and bounds.wav";
  for (int mode = 0; mode < 8; mode++) {
    v.tab=mode%3;
    v.row = mode;
    v.progress = mode == 1 ? 0 : mode == 2 ? 100 : -1;
    v.editing = mode == 3 ? 1 : 0;
    v.browser = mode == 4;
    v.confirm = mode == 5;
    v.playing = mode == 6;
    v.play_position = 23;
    snprintf(v.entry, sizeof(v.entry), "155.250");
    ui_draw(memory + 1, &v);
    assert(memory[0] == 0x12345678 && memory[960 * 544 + 1] == 0x87654321);
  }
  v.settings.start_a = 0;
  v.settings.duration = .1;
  v.progress = -1;
  v.editing = v.browser = v.confirm = 0;
  ui_draw(memory + 1, &v);
  for (int id = 1; id <= PRESET_COUNT; id++) {
    v.preset_browser = 1;
    v.selected_preset = id;
    ui_draw(memory + 1, &v);
    assert(memory[0] == 0x12345678 && memory[960 * 544 + 1] == 0x87654321);
  }
  v.preset_browser = 0;
  for (int mode = 4; mode <= 5; mode++) {
    v.editing = mode;
    ui_draw(memory + 1, &v);
  }
  for (int mode = 6; mode <= 7; mode++) {
    v.editing = mode;
    ui_draw(memory + 1, &v);
    assert(memory[0] == 0x12345678 && memory[960 * 544 + 1] == 0x87654321);
  }
  free(memory);
  puts("PASS: three-tab touch targets, keypad keys, waveform bounds and modal render "
       "paths.");
  return 0;
}
