#ifndef CIRCUIT_AUTOMIX_H
#define CIRCUIT_AUTOMIX_H

#include "beatmatch.h"
#include "render.h"
#include <stdint.h>

typedef enum {
  AUTOMIX_SMOOTH = 0,
  AUTOMIX_ENERGY = 1,
  AUTOMIX_VOCAL_BLEND = 2,
  AUTOMIX_VARIATION_COUNT = 3
} AutoMixVariation;

/* Cached waveform envelopes guide dynamics and energy selection. They cannot
 * identify vocals, keys, drops, or semantic musical sections. Phrase and
 * downbeat estimates are confidence-gated and may fall back to a clean blend. */
typedef struct {
  MixSettings mix;
  double tempo_ratio;      /* matched B tempo / original B tempo */
  int use_beatmatch;
  int matched_phrases;      /* estimated phrase grid, not semantic phrases */
  int variation;
  int vocal_blend_fallback; /* true until real aligned vocal stems are supplied */
  float section_confidence; /* envelope contrast confidence, not recognition */
  float downbeat_confidence; /* amplitude-accent estimate; not guaranteed bar 1 */
} AutoMixPlan;

/* Each mode chooses its own overlap and energy objective. VOCAL_BLEND is
 * explicitly a clean beatmatched fallback unless real, aligned vocal stems are
 * supplied by a future stem workflow. No voice recognition occurs here. */
int automix_plan_variation(const uint16_t *a, const uint16_t *b, unsigned bins,
                           double seconds_a, double seconds_b,
                           const BeatInfo *beat_a, const BeatInfo *beat_b,
                           AutoMixVariation variation, unsigned revision,
                           AutoMixPlan *out);

/* Compatibility entry point: the default Smooth candidate. */
int automix_plan(const uint16_t *a, const uint16_t *b, unsigned bins,
                 double seconds_a, double seconds_b,
                 const BeatInfo *beat_a, const BeatInfo *beat_b,
                 AutoMixPlan *out);

#endif
