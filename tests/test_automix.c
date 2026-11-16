#include "../src/automix.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define BINS 8192
static uint16_t a[BINS], b[BINS];

static void song(uint16_t *p, double seconds, double bpm, int start_loud) {
  for (unsigned i = 0; i < BINS; ++i) {
    double t = (double)i * seconds / BINS;
    double beat = fmod(t * bpm / 60, 1.0);
    double section = fmod(t * bpm / 60 / 16, 4.0);
    int loud = (section < 1.5) == start_loud;
    p[i] = loud ? 22000 : 3000;
    if (beat < .10) p[i] += loud ? 12000 : 2500;
  }
}

static void check_bounds(const AutoMixPlan *p, double a_len, double b_len) {
  assert(isfinite(p->mix.start_a) && isfinite(p->mix.start_b));
  assert(isfinite(p->mix.duration) && p->mix.duration >= 2);
  assert(p->mix.start_a >= 0 &&
         p->mix.start_a + p->mix.duration <= a_len + .001);
  assert(p->mix.start_b >= 0 &&
         p->mix.start_b + p->mix.duration * p->tempo_ratio <= b_len + .001);
}

int main(void) {
  AutoMixPlan smooth, energy, vocal, revised;
  BeatInfo first = {128, 0, .4f}, second = {124, 0, .35f};
  song(a, 180, 128, 1);
  song(b, 170, 124, 0);

  assert(!automix_plan_variation(a, b, BINS, 180, 170, &first, &second,
                                 AUTOMIX_SMOOTH, 0, &smooth));
  assert(!automix_plan_variation(a, b, BINS, 180, 170, &first, &second,
                                 AUTOMIX_ENERGY, 0, &energy));
  assert(!automix_plan_variation(a, b, BINS, 180, 170, &first, &second,
                                 AUTOMIX_VOCAL_BLEND, 0, &vocal));
  assert(smooth.use_beatmatch && smooth.matched_phrases);
  assert(smooth.tempo_ratio > 1 && smooth.tempo_ratio < 1.05);
  check_bounds(&smooth, 180, 170);
  check_bounds(&energy, 180, 170);
  check_bounds(&vocal, 180, 170);
  assert(smooth.mix.duration > energy.mix.duration);
  assert(energy.mix.duration != vocal.mix.duration);
  assert(smooth.variation == AUTOMIX_SMOOTH);
  assert(energy.variation == AUTOMIX_ENERGY);
  assert(vocal.variation == AUTOMIX_VOCAL_BLEND &&
         vocal.vocal_blend_fallback);
  assert(fabs(smooth.mix.start_a / (960.0 / 128.0) -
              round(smooth.mix.start_a / (960.0 / 128.0))) < .005);
  assert(fabs(smooth.mix.start_b / (960.0 / 124.0) -
              round(smooth.mix.start_b / (960.0 / 124.0))) < .005);
  assert(isfinite(smooth.section_confidence) &&
         smooth.section_confidence >= 0 && smooth.section_confidence <= 1);
  assert(isfinite(smooth.downbeat_confidence) &&
         smooth.downbeat_confidence >= 0 && smooth.downbeat_confidence <= 1);
  assert(smooth.mix.effects == (FX_CROSSFADE | FX_BASS_SWAP) &&
         smooth.mix.preset_id == 0);
  assert(!automix_plan_variation(a, b, BINS, 180, 170, &first, &second,
                                 AUTOMIX_SMOOTH, 1, &revised));
  check_bounds(&revised, 180, 170);
  assert(revised.mix.start_a != smooth.mix.start_a ||
         revised.mix.start_b != smooth.mix.start_b);

  BeatInfo wrong = {80, 0, .3f};
  assert(!automix_plan(a, b, BINS, 180, 170, &wrong, &second, &smooth));
  assert(!smooth.use_beatmatch && smooth.tempo_ratio == 1);
  check_bounds(&smooth, 180, 170);

  BeatInfo missing = {0, 0, 0};
  assert(!automix_plan(a, b, BINS, 180, 170, &missing, &second, &smooth));
  assert(!smooth.use_beatmatch);
  BeatInfo weak = {128, 0, .04f};
  assert(!automix_plan(a, b, BINS, 180, 170, &weak, &second, &smooth));
  assert(!smooth.use_beatmatch && smooth.tempo_ratio == 1);
  assert(!automix_plan(a, b, BINS, 18, 18, &first, &second, &smooth));
  check_bounds(&smooth, 18, 18);
  assert(automix_plan(a, b, BINS, 5, 180, &first, &second, &smooth) != 0);
  assert(automix_plan(NULL, b, BINS, 180, 170, &first, &second, &smooth) != 0);
  assert(automix_plan_variation(a, b, BINS, 180, 170, &first, &second,
                                (AutoMixVariation)99, 0, &smooth) != 0);
  puts("PASS: three distinct transitions, regeneration, phrase-grid alignment, "
       "safe tempo fallback, and short-song bounds.");
  return 0;
}
