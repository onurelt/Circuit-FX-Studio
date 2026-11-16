#include "automix.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static double limit(double x, double low, double high) {
  return x < low ? low : x > high ? high : x;
}

/* Existing cached peak envelopes are enough for coarse intensity analysis.
 * This is an energy estimate, never vocal/key/drop recognition. */
static double energy(const uint16_t *peaks, unsigned bins, double length,
                     double at, double width) {
  if (!peaks || bins < 2 || length <= 0 || width <= 0) return 0;
  double end = limit(at + width, 0, length);
  at = limit(at, 0, length);
  if (end <= at) return 0;
  unsigned lo = (unsigned)(at * bins / length);
  unsigned hi = (unsigned)ceil(end * bins / length);
  if (hi > bins) hi = bins;
  if (lo >= bins) lo = bins - 1;
  if (hi <= lo) hi = lo + 1;
  double sum = 0;
  for (unsigned i = lo; i < hi; ++i) sum += sqrt((double)peaks[i]);
  return sum / (double)(hi - lo);
}

/* Estimate the strongest four-beat accent from the cached envelope. If
 * accents do not distinguish a bar phase, keep the beat analyzer's phase. */
static double estimate_bar_phase(const uint16_t *peaks, unsigned bins,
                                 double length, double bpm, double phase,
                                 float *confidence) {
  double beat = 60.0 / bpm;
  double scores[4] = {0, 0, 0, 0};
  unsigned counts[4] = {0, 0, 0, 0};
  for (int offset = 0; offset < 4; ++offset) {
    for (double t = phase + offset * beat; t < length; t += 4.0 * beat) {
      double before = energy(peaks, bins, length, t - .12 * beat, .12 * beat);
      double onset = energy(peaks, bins, length, t, .12 * beat);
      scores[offset] += fmax(0, onset - before);
      counts[offset]++;
    }
  }
  int best = 0;
  double highest = -1, second = -1;
  for (int i = 0; i < 4; ++i) {
    if (!counts[i]) continue;
    double average = scores[i] / counts[i];
    if (average > highest) {
      second = highest;
      highest = average;
      best = i;
    } else if (average > second) {
      second = average;
    }
  }
  if (second < 0) second = highest;
  double relative = highest > 0 ? (highest - second) / (highest + .001) : 0;
  if (confidence) *confidence = (float)limit(relative, 0, 1);
  if (relative < .04) best = 0;
  return phase + best * beat;
}

static double envelope_spread(double a, double b, double c) {
  double low = fmin(a, fmin(b, c));
  double high = fmax(a, fmax(b, c));
  double mean = (a + b + c) / 3.0;
  return (high - low) / (1.0 + mean);
}

static double grid_at(double time, double phase, double step, double max_time) {
  if (!(step > 0) || !isfinite(step)) return limit(time, 0, max_time);
  double candidate = phase + round((time - phase) / step) * step;
  if (candidate < 0) candidate += ceil(-candidate / step) * step;
  if (candidate > max_time)
    candidate -= ceil((candidate - max_time) / step) * step;
  return limit(candidate, 0, max_time);
}

static double cue_a(const uint16_t *peaks, unsigned bins, double length,
                    double overlap, double bpm, double phase, int grid,
                    int variation, unsigned revision, float *confidence) {
  double max_time = length - overlap;
  if (max_time <= 0) return 0;
  double beat = 60.0 / bpm;
  double phrase = 16.0 * beat;
  double offset = variation == AUTOMIX_SMOOTH ? 4.0 * beat :
                  variation == AUTOMIX_ENERGY ? 8.0 * beat : 6.0 * beat;
  double desired = limit(max_time - offset -
                         (double)(revision % 3) * phrase, 0, max_time);
  if (!grid) return desired;
  double lower = limit(max_time - 8.0 * phrase, 0, max_time);
  double chosen = grid_at(desired, phase, phrase, max_time);
  double best = -1e30, spread = 0;
  for (double t = grid_at(lower, phase, phrase, max_time);
       t <= max_time + .00001; t += phrase) {
    if (t < lower - .00001) continue;
    double before = energy(peaks, bins, length, t - phrase, phrase);
    double first = energy(peaks, bins, length, t, overlap * .25);
    double middle = energy(peaks, bins, length, t + overlap * .25,
                           overlap * .25);
    double tail = energy(peaks, bins, length, t + overlap * .65,
                         overlap * .25);
    double contrast = fabs(first - before) / (1.0 + first + before);
    double volatility = envelope_spread(first, middle, tail);
    double closeness = 1.0 - fabs(t - desired) / (1.0 + max_time - lower);
    double score;
    if (variation == AUTOMIX_ENERGY) {
      /* Favor a strong outgoing section that resolves through the overlap. */
      double controlled_drop = (first + .35 * middle - tail) /
                               (1.0 + first + middle + tail);
      double launch = first / (1.0 + first);
      score = .18 * closeness + .52 * controlled_drop + .30 * launch;
    } else if (variation == AUTOMIX_VOCAL_BLEND) {
      /* Energy density is only a proxy; this does not detect vocals. */
      double density = (first + middle) * .5;
      score = .38 * closeness + .62 / (1.0 + density);
    } else {
      /* Smooth prefers a stable envelope and avoids abrupt changes. */
      score = .42 * closeness + .32 * (1.0 - contrast) +
              .26 / (1.0 + volatility);
    }
    if (score > best) {
      best = score;
      chosen = t;
      spread = contrast;
    }
  }
  if (confidence)
    *confidence = (float)limit(spread * 2.5, 0, 1);
  return chosen;
}

static double cue_b(const uint16_t *peaks, unsigned bins, double length,
                    double consumed, double bpm, double phase, int grid,
                    int variation, unsigned revision, float *confidence) {
  double max_time = length - consumed;
  double window = fmin(max_time, 40.0 * 60.0 / bpm);
  if (window <= 0) return 0;
  double desired = fmin(window, (double)(revision % 3) * 16.0 * 60.0 / bpm);
  if (!grid) return desired;
  double phrase = 16.0 * 60.0 / bpm;
  double chosen = grid_at(desired, phase, phrase, max_time);
  double best = -1e30, rise_best = 0;
  for (double t = grid_at(0, phase, phrase, max_time);
       t <= window + .00001; t += phrase) {
    double before = energy(peaks, bins, length, t, consumed * .22);
    double late = energy(peaks, bins, length, t + consumed * .73,
                         consumed * .20);
    double rise = (late - before) / (1.0 + late + before);
    double early = 1.0 - t / (window + 1.0);
    double contrast = fabs(late - before) / (1.0 + late + before);
    double score;
    if (variation == AUTOMIX_ENERGY) {
      /* Start below the outgoing energy and build toward the drop. */
      score = .55 * rise + .20 * early + .25 / (1.0 + before);
    } else if (variation == AUTOMIX_VOCAL_BLEND) {
      /* Low energy is a conservative fallback, never vocal recognition. */
      score = .55 / (1.0 + before) + .30 * rise + .15 * early;
    } else {
      /* Smooth favors a gentle rise without a sudden level jump. */
      score = .48 * rise + .27 * early + .25 * (1.0 - contrast);
    }
    if (score > best) {
      best = score;
      chosen = t;
      rise_best = rise;
    }
  }
  if (confidence)
    *confidence = (float)limit(fabs(rise_best) * 2.5, 0, 1);
  return chosen;
}

int automix_plan_variation(const uint16_t *a, const uint16_t *b,
                           unsigned bins, double seconds_a, double seconds_b,
                           const BeatInfo *beat_a, const BeatInfo *beat_b,
                           AutoMixVariation variation, unsigned revision,
                           AutoMixPlan *out) {
  if (!out || !a || !b || bins < 256 || bins > 8192 ||
      !isfinite(seconds_a) || !isfinite(seconds_b) ||
      seconds_a < 8 || seconds_b < 8 ||
      seconds_a > 48000 || seconds_b > 48000 ||
      (int)variation < 0 || variation >= AUTOMIX_VARIATION_COUNT)
    return -1;

  memset(out, 0, sizeof(*out));
  /* A plausible number alone is not enough to warp the song. Require the
   * envelope autocorrelation to clear its measured reliability floor; the UI
   * marks manually entered BPM as confidence 1.0. */
  double bpm_a = beat_a && isfinite(beat_a->bpm) &&
                 isfinite(beat_a->confidence) && beat_a->confidence >= .12f &&
                 beat_a->bpm >= 70 && beat_a->bpm <= 180 ? beat_a->bpm : 0;
  double bpm_b = beat_b && isfinite(beat_b->bpm) &&
                 isfinite(beat_b->confidence) && beat_b->confidence >= .12f &&
                 beat_b->bpm >= 70 && beat_b->bpm <= 180 ? beat_b->bpm : 0;
  double bpm = bpm_a ? bpm_a : 125.0;
  double ratio = (bpm_a > 0 && bpm_b > 0) ? bpm_a / bpm_b : 1.0;
  /* Keep pitch-preserving time stretch in a conservative range. If the songs
   * differ more, retain original pitch and use a clean crossfade. */
  int sync = bpm_a > 0 && bpm_b > 0 && ratio >= .89 && ratio <= 1.12;
  if (!sync) ratio = 1.0;

  double phase_a = beat_a && isfinite(beat_a->phase_seconds) ?
                   beat_a->phase_seconds : 0;
  double phase_b = beat_b && isfinite(beat_b->phase_seconds) ?
                   beat_b->phase_seconds : 0;
  phase_a = limit(phase_a, 0, 60.0 / bpm);
  phase_b = limit(phase_b, 0, 60.0 / (bpm_b > 0 ? bpm_b : bpm));
  float downbeat_a = 0, downbeat_b = 0;
  double bar_phase_a = estimate_bar_phase(a, bins, seconds_a, bpm, phase_a,
                                           &downbeat_a);
  double bar_phase_b = estimate_bar_phase(b, bins, seconds_b,
                                           bpm_b > 0 ? bpm_b : bpm, phase_b,
                                           &downbeat_b);

  /* Smooth is 32 beats, Energy is 16, Vocal Blend fallback is 24. The
   * fallback remains a normal crossfade; it does not isolate vocals. */
  double beats = variation == AUTOMIX_SMOOTH ? 32.0 :
                 variation == AUTOMIX_ENERGY ? 16.0 : 24.0;
  double overlap = beats * 60.0 / bpm;
  double common = fmin(seconds_a, seconds_b / ratio);
  if (overlap > common * .72)
    overlap = fmin(16.0 * 60.0 / bpm, common * .62);
  overlap = limit(overlap, 2.0, fmin(120.0, common * .8));
  if (!isfinite(overlap) || overlap < 2.0) return -1;

  double consumed = overlap * ratio;
  float conf_a = 0, conf_b = 0;
  double start_a = cue_a(a, bins, seconds_a, overlap, bpm, bar_phase_a, sync,
                         variation, revision, &conf_a);
  double start_b = cue_b(b, bins, seconds_b, consumed,
                         bpm_b > 0 ? bpm_b : bpm, bar_phase_b, sync,
                         variation, revision, &conf_b);
  start_a = limit(start_a, 0, seconds_a - overlap);
  start_b = limit(start_b, 0, seconds_b - consumed);

  out->mix.start_a = floor(start_a * 1000.0 + .5) / 1000.0;
  out->mix.start_b = floor(start_b * 1000.0 + .5) / 1000.0;
  out->mix.duration = overlap;
  out->mix.bpm = (float)bpm;
  out->mix.effects = FX_CROSSFADE | FX_BASS_SWAP;
  out->mix.intensity = variation == AUTOMIX_ENERGY ? .90f : .75f;
  out->mix.sample_speed = 1.0f;
  out->tempo_ratio = ratio;
  out->use_beatmatch = sync;
  out->matched_phrases = sync;
  out->variation = variation;
  out->vocal_blend_fallback = variation == AUTOMIX_VOCAL_BLEND;
  out->section_confidence = .5f * (conf_a + conf_b);
  out->downbeat_confidence = .5f * (downbeat_a + downbeat_b);
  /* Regeneration explores the adjacent phrase without losing grid alignment. */
  if (sync && revision % 3) {
    double shift = (double)(revision % 3) * 16.0 * 60.0 / bpm;
    double max_a = seconds_a - overlap;
    double max_b = fmin(seconds_b - consumed,
                        40.0 * 60.0 / (bpm_b > 0 ? bpm_b : bpm));
    out->mix.start_a = floor(grid_at(out->mix.start_a - shift, bar_phase_a,
                                     16.0 * 60.0 / bpm, max_a) * 1000.0 + .5)
                       / 1000.0;
    out->mix.start_b = floor(grid_at(out->mix.start_b + shift, bar_phase_b,
                                     16.0 * 60.0 /
                                         (bpm_b > 0 ? bpm_b : bpm),
                                     max_b) * 1000.0 + .5) / 1000.0;
  }
  return 0;
}

int automix_plan(const uint16_t *a, const uint16_t *b, unsigned bins,
                 double seconds_a, double seconds_b,
                 const BeatInfo *beat_a, const BeatInfo *beat_b,
                 AutoMixPlan *out) {
  return automix_plan_variation(a, b, bins, seconds_a, seconds_b,
                                beat_a, beat_b, AUTOMIX_SMOOTH, 0, out);
}
