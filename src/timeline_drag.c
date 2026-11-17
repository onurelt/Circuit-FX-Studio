#include "timeline_drag.h"
#include <math.h>

double timeline_slide_cue(double source_cue, int delta_pixels,
                          double visible_seconds, double track_seconds,
                          double overlap_seconds, double tempo_ratio) {
  if (!isfinite(source_cue) || !isfinite(visible_seconds) ||
      !isfinite(track_seconds) || !isfinite(overlap_seconds) ||
      !isfinite(tempo_ratio) || visible_seconds <= 0 ||
      track_seconds <= 0 || overlap_seconds <= 0 || tempo_ratio <= 0 ||
      tempo_ratio > 4)
    return source_cue;
  double max_start=track_seconds-overlap_seconds*tempo_ratio;
  if (max_start < 0) max_start=0;
  double shifted=source_cue-(double)delta_pixels*visible_seconds/880.0;
  if (shifted < 0) shifted=0;
  if (shifted > max_start) shifted=max_start;
  return round(shifted*1000.0)/1000.0;
}
