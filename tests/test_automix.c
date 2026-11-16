#include "../src/automix.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define BINS 8192
static uint16_t a[BINS],b[BINS];

static void song(uint16_t *p,double seconds,double bpm,int start_loud) {
  for (unsigned i=0;i<BINS;i++) {
    double t=(double)i*seconds/BINS,beat=fmod(t*bpm/60,1.0);
    double section=fmod(t*bpm/60/16,4.0);
    int loud=(section<1.5)==start_loud;
    p[i]=loud?22000:3000;
    if (beat<.10) p[i]+=loud?12000:2500;
  }
}
int main(void) {
  AutoMixPlan p;
  BeatInfo first={128,0,.4f},second={124,0,.35f};
  song(a,180,128,1);
  song(b,170,124,0);
  assert(!automix_plan(a,b,BINS,180,170,&first,&second,&p));
  assert(p.use_beatmatch && p.matched_phrases);
  assert(p.tempo_ratio>1 && p.tempo_ratio<1.05);
  assert(p.mix.start_a>=0 && p.mix.start_a+p.mix.duration<=180);
  assert(p.mix.start_b>=0 &&
         p.mix.start_b+p.mix.duration*p.tempo_ratio<=170);
  assert(fabs(p.mix.start_a/(960./128) -
              round(p.mix.start_a/(960./128)))<.005);
  assert(fabs(p.mix.start_b/(960./124) -
              round(p.mix.start_b/(960./124)))<.005);
  assert(p.mix.effects==FX_CROSSFADE && p.mix.preset_id==0);
  BeatInfo wrong={80,0,.3f};
  assert(!automix_plan(a,b,BINS,180,170,&wrong,&second,&p));
  assert(!p.use_beatmatch && p.tempo_ratio==1);
  assert(p.mix.duration>=2 && p.mix.effects==FX_CROSSFADE);
  BeatInfo missing={0,0,0};
  assert(!automix_plan(a,b,BINS,180,170,&missing,&second,&p));
  assert(!p.use_beatmatch);
  assert(!automix_plan(a,b,BINS,18,18,&first,&second,&p));
  assert(p.mix.start_a+p.mix.duration<=18+.0001);
  assert(p.mix.start_b+p.mix.duration*p.tempo_ratio<=18+.0001);
  assert(automix_plan(a,b,BINS,5,180,&first,&second,&p)!=0);
  assert(automix_plan(NULL,b,BINS,180,170,&first,&second,&p)!=0);
  puts("PASS: automatic cue planning, 16-beat grid, safe tempo fallback, short songs.");
  return 0;
}
