#ifndef CIRCUIT_PREPARED_STEMS_H
#define CIRCUIT_PREPARED_STEMS_H
#include <stddef.h>
#define STEM_COUNT 4
#define STEM_PATH_CAPACITY 512

enum {
  STEM_VOCALS = 0,
  STEM_DRUMS = 1,
  STEM_BASS = 2,
  STEM_OTHER = 3
};

/* Locate optional <song>.<stem>.wav sidecars next to the original song.
 * A bit is returned only when that sidecar is a valid 44.1 kHz, stereo,
 * 16-bit PCM WAV with exactly the decoded song's frame count. The caller
 * owns no allocated memory; absent or invalid stems are simply omitted. */
int prepared_stem_paths(const char *track, const char *decoded_pcm,
                        char paths[STEM_COUNT][STEM_PATH_CAPACITY]);

/* Locate outgoing .vocals.wav and incoming .instrumental.wav sidecars.
 * Return 1 only when both are valid and exactly aligned to their decoded
 * songs; return 0 if either is unavailable or invalid, -1 for bad arguments. */
int prepared_vocal_handoff_paths(const char *track_a, const char *pcm_a,
                                 const char *track_b, const char *pcm_b,
                                 char *vocal_a, size_t vocal_a_capacity,
                                 char *instrumental_b,
                                 size_t instrumental_b_capacity);
#endif
