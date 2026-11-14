#include "ui.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void){
 const int points[15][2]={{100,90},{600,90},{150,295},{450,295},{770,295},{150,367},{150,397},{150,427},{150,457},{150,487},{650,473},{850,473},{700,365},{700,393},{700,421}};
 for(int i=0;i<15;i++)assert(ui_hit_test(points[i][0],points[i][1])==i);
 assert(ui_hit_test(0,0)==-1);assert(ui_hit_test(960,544)==-1);
 for(int i=0;i<12;i++)assert(ui_key_test(300+(i%3)*144,210+(i/3)*48)==i);
 assert(ui_key_test(320,415)==12);assert(ui_key_test(550,415)==13);assert(ui_key_test(403,210)==-1);
 uint32_t *memory=calloc(960*544+2,sizeof(uint32_t));assert(memory);
 memory[0]=0x12345678;memory[960*544+1]=0x87654321;
 View v={0};v.progress=-1;v.settings=(MixSettings){155,48,15,31};v.lengths[0]=360;v.lengths[1]=420;
 assert(ui_timeline_pre(&v)==4);assert(ui_timeline_span(&v)==23);
 v.songs[0]="A long filename to check clipping and bounds.wav";
 for(int mode=0;mode<8;mode++){
  v.row=mode;v.progress=mode==1?0:mode==2?100:-1;v.editing=mode==3?1:0;v.browser=mode==4;v.confirm=mode==5;v.playing=mode==6;v.play_position=23;
  snprintf(v.entry,sizeof(v.entry),"155.250");ui_draw(memory+1,&v);
  assert(memory[0]==0x12345678&&memory[960*544+1]==0x87654321);
 }
 v.settings.start_a=0;v.settings.duration=.1;v.progress=-1;v.editing=v.browser=v.confirm=0;ui_draw(memory+1,&v);
 free(memory);puts("PASS: touch targets, keypad keys, timeline bounds and all modal render paths.");return 0;
}
