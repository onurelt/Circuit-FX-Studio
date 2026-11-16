#ifndef CIRCUIT_AUTOMIX_H
#define CIRCUIT_AUTOMIX_H

#include "beatmatch.h"
#include "render.h"
#include <stdint.h>

/* Lightweight phrase-grid/energy-based planning. This is not vocal or key
 * recognition: it produces a safe transition without requiring a preset. */
typedef struct {
  MixSettings mix;
  double tempo_ratio;  /* matched B tempo / original B tempo */
  int use_beatmatch;   /* false -> crossfade at original tempos */
  int matched_phrases; /* approximate 16-beat grid, not vocal phrases */
} AutoMixPlan;

/* Lengths and cue positions are in source-song seconds. For a stretched
 * track B, the renderer is given start_b / tempo_ratio. */
int automix_plan(const uint16_t *a, const uint16_t *b, unsigned bins,
                 double seconds_a, double seconds_b,
                 const BeatInfo *beat_a, const BeatInfo *beat_b,
                 AutoMixPlan *out);

#endif
