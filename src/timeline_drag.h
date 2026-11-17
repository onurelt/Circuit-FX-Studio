#ifndef CIRCUIT_TIMELINE_DRAG_H
#define CIRCUIT_TIMELINE_DRAG_H

/* Slide the source waveform under its anchored transition marker.
 * Positive finger motion (right) reveals earlier source audio.
 * Returns a 1 ms quantized, bounded, non-destructive song cue.
 * tempo_ratio = matched A BPM / source B BPM, or 1 with Auto Beatmatch off.
 */
double timeline_slide_cue(double source_cue, int delta_pixels,
                          double visible_seconds, double track_seconds,
                          double overlap_seconds, double tempo_ratio);

#endif
