#ifndef CIRCUIT_STEM_MIX_H
#define CIRCUIT_STEM_MIX_H
#include "prepared_stems.h"
#include "render.h"

extern char stem_mix_error[160];

/* Mix a bit-selected set of aligned 44.1 kHz stereo PCM stems. Gains are
 * 0..2.0. A first bounded-memory pass measures peak level; output is only
 * attenuated as needed to keep peaks below 30000, never boosted. The output
 * path must be fresh. Cancellation removes any partial output. */
int stem_mix_wav(const char *const paths[STEM_COUNT], const char *output,
                 const float gains[STEM_COUNT], unsigned active_mask,
                 Progress progress);
#endif
