#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>
enum {
  FX_CROSSFADE = 1,
  FX_ECHO = 2,
  FX_FILTER = 4,
  FX_BASS_SWAP = 8,
  FX_QUICK_CUT = 16
};
typedef struct {
  double start_a, start_b, duration;
  unsigned effects;
  int preset_id;        /* zero = original manual FX */
  float bpm, intensity; /* BPM and effects only; intensity 0..1 */
  float sample_speed;   /* 0.5..2.0; pitch follows speed */
  int sample_reverse;   /* reverse bundled WAV transition sound */
} MixSettings;
typedef int (*Progress)(uint32_t done, uint32_t total);
extern char render_error[160];
int inspect_wav(const char *path, uint32_t *frames, uint32_t *offset);
/* Single renderer instance. All buffers are fixed-size; call from one thread.
 */
int render_mix(const char *a, const char *b, const char *out,
               const MixSettings *settings, int preview, Progress progress);
#endif
