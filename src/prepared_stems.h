#ifndef CIRCUIT_PREPARED_STEMS_H
#define CIRCUIT_PREPARED_STEMS_H
#include <stddef.h>

/* Locate the outgoing track's .vocals.wav and incoming track's
 * .instrumental.wav sidecars. Return 1 only when both are valid 44.1 kHz,
 * stereo, 16-bit PCM WAVs with exactly the decoded track frame counts.
 * Return 0 when missing, malformed, mismatched, or too long for the outputs;
 * return -1 for invalid API arguments. */
int prepared_vocal_handoff_paths(const char *track_a, const char *pcm_a,
                                 const char *track_b, const char *pcm_b,
                                 char *vocal_a, size_t vocal_a_capacity,
                                 char *instrumental_b,
                                 size_t instrumental_b_capacity);
#endif
