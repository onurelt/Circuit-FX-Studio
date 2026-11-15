#ifndef CIRCUIT_BEATMATCH_H
#define CIRCUIT_BEATMATCH_H
#include <stdint.h>

typedef struct {
  float bpm;
  float phase_seconds;
  float confidence;
} BeatInfo;

typedef int (*BeatProgress)(uint32_t done, uint32_t total);
extern char beatmatch_error[160];

/* Analyze a compact peak envelope, without decoding the song again. A zero
 * BPM means that a stable beat was not found. */
BeatInfo beat_analyze_peaks(const uint16_t *peaks, unsigned bins,
                            double seconds);
double beat_nearest(double time, const BeatInfo *beat);

/* Offline WSOLA-like overlap/add, stereo 44.1kHz 16-bit PCM in/out. Tempo
 * ratio is target BPM / source BPM. Output pitch is not resampled. The caller
 * supplies a fresh temporary path and renames it only on success. */
int beat_stretch_wav(const char *input, const char *output,
                     double tempo_ratio, BeatProgress progress);
#endif
