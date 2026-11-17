#include "../src/timeline_drag.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int close_to(double a,double b) { return fabs(a-b)<.0011; }

int main(void) {
  /* 440 pixels across a 40-second viewport means 20 seconds movement. */
  assert(close_to(timeline_slide_cue(60,440,40,200,20,1),40));
  assert(close_to(timeline_slide_cue(60,-440,40,200,20,1),80));
  assert(close_to(timeline_slide_cue(2,440,40,200,20,1),0));
  assert(close_to(timeline_slide_cue(175,-440,40,200,20,1),180));
  /* Matched Track B source consumption: 15 s overlap * 1.2 = 18 s. */
  assert(close_to(timeline_slide_cue(182,-440,40,200,15,1.2),182));
  assert(close_to(timeline_slide_cue(180,-440,40,200,15,1.2),182));
  assert(close_to(timeline_slide_cue(100,88,10,200,15,1),99));
  assert(close_to(timeline_slide_cue(100,88,40,200,15,1),96));
  assert(close_to(timeline_slide_cue(15,0,40,200,15,1),15));
  assert(close_to(timeline_slide_cue(10,100,40,200,15,0),10));
  assert(close_to(timeline_slide_cue(10,100,0,200,15,1),10));
  assert(close_to(timeline_slide_cue(10,100,40,0,15,1),10));
  puts("PASS: track A/B drag direction, anchored marker, zoom precision, cue bounds.");
  return 0;
}
