#include "stem_mix.h"
#include "wav.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define STEM_BLOCK 256u
char stem_mix_error[160];

static int fail(const char *message) {
  snprintf(stem_mix_error, sizeof(stem_mix_error), "%s", message);
  return -1;
}

int stem_mix_wav(const char *const paths[STEM_COUNT], const char *output,
                 const float gains[STEM_COUNT], unsigned active_mask,
                 Progress progress) {
  FILE *inputs[STEM_COUNT] = {0};
  FILE *out = NULL;
  uint32_t offsets[STEM_COUNT] = {0}, frames = 0;
  int result = -1, created = 0;
  int16_t samples[STEM_COUNT][STEM_BLOCK * 2];
  float mixed[STEM_BLOCK * 2];
  int16_t encoded[STEM_BLOCK * 2];
  double peak = 0;
  stem_mix_error[0] = '\0';

  if (!paths || !output || !gains || !(active_mask & 15u) ||
      (active_mask & ~15u))
    return fail("Choose at least one separated stem.");
  if (!strcmp(output, "") || strlen(output) >= 512)
    return fail("Invalid stem mix output path.");

  out = fopen(output, "rb");
  if (out) {
    fclose(out);
    return fail("Stem mix output already exists.");
  }

  for (int i = 0; i < STEM_COUNT; ++i) {
    if (!(active_mask & (1u << i)))
      continue;
    if (!paths[i] || !paths[i][0] || !isfinite(gains[i]) ||
        gains[i] < 0 || gains[i] > 2.0f)
      return fail("Stem path or volume is invalid.");
    uint32_t this_frames = 0;
    if (inspect_wav(paths[i], &this_frames, &offsets[i]))
      return fail("A stem is not 44.1 kHz stereo 16-bit PCM WAV.");
    if (!frames)
      frames = this_frames;
    else if (frames != this_frames)
      return fail("Selected stems are not sample-aligned.");
    if (!strcmp(paths[i], output))
      return fail("Stem mix cannot overwrite an input stem.");
  }
  if (!frames || frames > (0x7fffffffu - 44u) / 4u)
    return fail("Stem audio is empty or exceeds WAV limits.");
  uint32_t progress_total = frames * 2u;

  for (int i = 0; i < STEM_COUNT; ++i) {
    if (active_mask & (1u << i)) {
      inputs[i] = fopen(paths[i], "rb");
      if (!inputs[i]) {
        fail("Cannot open an imported stem.");
        goto done;
      }
    }
  }

  /* Pass one finds the maximum combined sample with fixed-size buffers.
   * Pass two writes the gain-adjusted sum and preserves exact frame alignment. */
  for (unsigned pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < STEM_COUNT; ++i)
      if ((active_mask & (1u << i)) &&
          fseek(inputs[i], (long)offsets[i], SEEK_SET)) {
        fail("Cannot seek an imported stem.");
        goto done;
      }
    if (pass == 1) {
      out = fopen(output, "wb");
      if (!out) {
        fail("Cannot create stem mix WAV; check free storage.");
        goto done;
      }
      created = 1;
      if (wav_begin(out, 44100)) {
        fail("Cannot write stem mix WAV; check free storage.");
        goto done;
      }
    }
    for (uint32_t at = 0; at < frames;) {
      uint32_t count = frames - at < STEM_BLOCK ? frames - at : STEM_BLOCK;
      memset(mixed, 0, count * 2u * sizeof(mixed[0]));
      for (int stem = 0; stem < STEM_COUNT; ++stem) {
        if (!(active_mask & (1u << stem)))
          continue;
        if (fread(samples[stem], 4, count, inputs[stem]) != count) {
          fail("Imported stem read failed.");
          goto done;
        }
        float gain = gains[stem];
        for (uint32_t frame = 0; frame < count * 2u; ++frame)
          mixed[frame] += (float)samples[stem][frame] * gain;
      }

      if (pass == 0) {
        for (uint32_t i = 0; i < count * 2u; ++i) {
          double value = fabs((double)mixed[i]);
          if (value > peak)
            peak = value;
        }
      } else {
        double scale = peak > 30000.0 ? 30000.0 / peak : 1.0;
        for (uint32_t i = 0; i < count * 2u; ++i) {
          double value = mixed[i] * scale;
          if (value > 32767.0) value = 32767.0;
          if (value < -32768.0) value = -32768.0;
          encoded[i] = (int16_t)lrint(value);
        }
        if (wav_append(out, encoded, count)) {
          fail("Stem mix write failed; check free storage.");
          goto done;
        }
      }
      at += count;
      if (progress && progress(pass * frames + at, progress_total)) {
        fail("Stem mix cancelled.");
        goto done;
      }
    }
  }
  if (wav_finish(out)) {
    fail("Could not finalize stem mix WAV.");
    goto done;
  }
  result = 0;

done:
  for (int i = 0; i < STEM_COUNT; ++i)
    if (inputs[i])
      fclose(inputs[i]);
  if (out && fclose(out) && !result) {
    result = -1;
    fail("Could not close stem mix WAV.");
  }
  if (result && created)
    remove(output);
  return result;
}
