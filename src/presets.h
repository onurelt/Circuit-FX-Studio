#ifndef PRESETS_H
#define PRESETS_H
#define PRESET_COUNT 80
#define BANK_COUNT 5
enum {
  MOD_HIGH_PASS = 1,
  MOD_GATE = 2,
  MOD_ROLL = 4,
  MOD_RING = 8,
  MOD_NOISE = 16,
  MOD_DIP = 32,
  MOD_PINGPONG = 64
};
typedef struct {
  char name[64];
  const char *description;
  int bank, beats;
  int sample_id; /* 1..16 bundled WAV, zero = processing only */
  unsigned effects, mods;
  float curve, delay_beats, feedback, wet, filter_end, bass_pivot, cut_at,
      gate_beats, gate_depth, roll_beats, ring_hz, ring_mix, noise_mix;
} TransitionPreset;
extern const char *preset_banks[BANK_COUNT];
int preset_get(
    int id, TransitionPreset *p); /* 1..64; zero is the original manual stack */
#endif
