#ifndef CIRCUIT_VOCAL_H
#define CIRCUIT_VOCAL_H
#include <stdint.h>
typedef int (*VocalProgress)(uint32_t done, uint32_t total);
extern char vocal_error[160];
/* Input and optional sidecars are 44.1 kHz stereo 16-bit PCM WAV. A sidecar
 * must have exactly the input's frame count to preserve cue alignment. */
int vocal_reduce_wav(const char *input, const char *output,
                     const char *instrumental, const char *vocals,
                     unsigned strength, VocalProgress progress);
#endif
