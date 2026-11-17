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
enum {
  MIX_STYLE_LEGACY = 0,
  MIX_STYLE_SMOOTH = 1,
  MIX_STYLE_ENERGY = 2,
  MIX_STYLE_VOCAL_HANDOFF = 3
};
typedef struct {
  double start_a, start_b, duration;
  unsigned effects;
  int preset_id;        /* zero = original manual FX */
  float bpm, intensity; /* BPM and effects only; intensity 0..1 */
  float sample_speed;   /* 0.5..2.0; pitch follows speed */
  int sample_reverse;   /* reverse bundled WAV transition sound */
  int transition_style; /* optional Auto Mix DSP profile; 0 retains legacy */
  int prepared_stems_active; /* true only when validated vocals/instrumental WAVs used */
} MixSettings;
typedef int (*Progress)(uint32_t done, uint32_t total);
extern char render_error[160];
int inspect_wav(const char *path, uint32_t *frames, uint32_t *offset);
/* Single renderer instance. All buffers are fixed-size; call from one thread.
 */
int render_mix(const char *a, const char *b, const char *out,
               const MixSettings *settings, int preview, Progress progress);
/* Optional aligned source replacements used only for the overlap. The A
 * prefix and B tail still come from a/b. NULL keeps the original source. */
int render_mix_layers(const char *a,const char *b,const char *a_overlap,
                      const char *b_overlap,const char *out,
                      const MixSettings *settings,int preview,Progress progress);
#endif
