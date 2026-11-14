#include "render.h"
#include "wav.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define RATE 44100u
#define BLOCK 1024u
char render_error[160];
typedef struct {
  FILE *f;
  uint32_t frames, offset;
} Source;
static int16_t ab[BLOCK * 2], bb[BLOCK * 2], mix[BLOCK * 2];
static float echo[11025 * 2];
static uint32_t done, total;
static Progress progress;
static int fail(const char *s) {
  snprintf(render_error, sizeof(render_error), "%s", s);
  return -1;
}
static unsigned u16(const unsigned char *p) {
  return p[0] | ((unsigned)p[1] << 8);
}
static uint32_t u32(const unsigned char *p) {
  return u16(p) | ((uint32_t)u16(p + 2) << 16);
}
static int open_source(Source *s, const char *path) {
  s->f = fopen(path, "rb");
  if (!s->f)
    return fail("Cannot open selected song.");
  if (fseek(s->f, 0, SEEK_END))
    return fail("Cannot seek input.");
  long length = ftell(s->f);
  rewind(s->f);
  unsigned char h[16];
  if (length < 44 || fread(h, 1, 12, s->f) != 12 || memcmp(h, "RIFF", 4) ||
      memcmp(h + 8, "WAVE", 4))
    return fail("Not a RIFF WAV file.");
  uint64_t end = (uint64_t)u32(h + 4) + 8;
  if (end > (uint64_t)length || end < 12)
    return fail("Truncated RIFF file.");
  int fmt = 0, data = 0;
  while ((uint64_t)ftell(s->f) + 8 <= end) {
    if (fread(h, 1, 8, s->f) != 8)
      return fail("Truncated chunk header.");
    uint32_t n = u32(h + 4);
    long pos = ftell(s->f);
    uint64_t next = (uint64_t)pos + n + (n & 1);
    if (next > end || next > LONG_MAX)
      return fail("Invalid WAV chunk size.");
    if (!memcmp(h, "fmt ", 4)) {
      if (n < 16 || fread(h, 1, 16, s->f) != 16)
        return fail("Invalid WAV format chunk.");
      if (u16(h) != 1 || u16(h + 2) != 2 || u32(h + 4) != RATE ||
          u32(h + 8) != RATE * 4 || u16(h + 12) != 4 || u16(h + 14) != 16)
        return fail("Use 44.1kHz / stereo / 16-bit PCM WAV.");
      fmt = 1;
    } else if (!memcmp(h, "data", 4) && !data) {
      if (n % 4)
        return fail("Invalid WAV frame alignment.");
      s->offset = pos;
      s->frames = n / 4;
      data = 1;
    }
    if (fseek(s->f, (long)next, SEEK_SET))
      return fail("Cannot seek WAV chunk.");
  }
  return fmt && data && s->frames
             ? 0
             : fail("WAV has no PCM format or audio data.");
}
static int seek_frame(Source *s, uint32_t frame) {
  return fseek(s->f, s->offset + (long)frame * 4, SEEK_SET);
}
static int write_block(FILE *f, int16_t *p, uint32_t n) {
  if (wav_append(f, p, n))
    return fail("Write failed. Check free storage space.");
  done += n;
  if (progress && progress(done, total))
    return fail("Cancelled; originals preserved.");
  return 0;
}
static int copy(Source *s, FILE *out, uint32_t start, uint32_t count) {
  if (seek_frame(s, start))
    return fail("Input seek failed.");
  while (count) {
    uint32_t n = count > BLOCK ? BLOCK : count;
    if (fread(ab, 4, n, s->f) != n)
      return fail("Input read failed.");
    if (write_block(out, ab, n))
      return -1;
    count -= n;
  }
  return 0;
}
int render_mix(const char *ap, const char *bp, const char *op,
               const MixSettings *cfg, int preview, Progress cb) {
  double seconds = cfg->duration;
  unsigned effects = cfg->effects;
  Source a = {0}, b = {0};
  FILE *out = 0;
  int rc = -1;
  render_error[0] = 0;
  progress = cb;
  done = 0;
  if (!strcasecmp(ap, op) || !strcasecmp(bp, op))
    return fail("Output cannot overwrite a song.");
  if (open_source(&a, ap) || open_source(&b, bp))
    goto finish;
  if (!isfinite(seconds) || seconds < 0.1 || seconds > 120 ||
      !isfinite(cfg->start_a) || !isfinite(cfg->start_b) || cfg->start_a < 0 ||
      cfg->start_b < 0 || cfg->start_a > 48000 || cfg->start_b > 48000 ||
      effects > 31) {
    fail("Invalid transition settings.");
    goto finish;
  }
  uint32_t overlap = (uint32_t)(seconds * RATE);
  if (overlap > a.frames || overlap > b.frames) {
    fail("Transition is longer than a selected song.");
    goto finish;
  }
  uint32_t pre = (uint32_t)llround(cfg->start_a * RATE),
           cue = (uint32_t)llround(cfg->start_b * RATE);
  if ((uint64_t)pre + overlap > a.frames ||
      (uint64_t)cue + overlap > b.frames) {
    fail("Cue plus duration exceeds a song. Move cue earlier.");
    goto finish;
  }
  uint32_t tail = b.frames - cue - overlap, start = 0;
  if (preview) {
    start = pre > RATE * 4 ? pre - RATE * 4 : 0;
    if (tail > RATE * 4)
      tail = RATE * 4;
  }
  uint64_t frames = (uint64_t)pre - start + overlap + tail;
  if (frames * 4 + 44 > INT32_MAX) {
    fail("Output exceeds the beta's 2GB WAV limit.");
    goto finish;
  }
  total = frames;
  /* Caller supplies a unique path; refuse even accidental existing-file
   * overwrite. */
  out = fopen(op, "rb");
  if (out) {
    fclose(out);
    out = 0;
    fail("Output already exists; choose a fresh filename.");
    goto finish;
  }
  out = fopen(op, "wb");
  if (!out) {
    fail("Cannot create output file.");
    goto finish;
  }
  if (wav_begin(out, RATE)) {
    fail("Cannot write WAV header.");
    goto finish;
  }
  if (copy(&a, out, start, pre - start) || seek_frame(&a, pre) ||
      seek_frame(&b, cue))
    goto finish;
  memset(echo, 0, sizeof(echo));
  float low_a[2] = {0}, low_b[2] = {0}, filter[2] = {0};
  unsigned echo_at = 0;
  for (uint32_t at = 0; at < overlap;) {
    uint32_t n = overlap - at;
    if (n > BLOCK)
      n = BLOCK;
    if (fread(ab, 4, n, a.f) != n || fread(bb, 4, n, b.f) != n) {
      fail("Input read failed during transition.");
      goto finish;
    }
    for (uint32_t i = 0; i < n; i++) {
      float t = (float)(at + i) / (overlap - 1), ga = 1 - t, gb = t;
      if (!(effects & FX_CROSSFADE)) {
        float width = fminf(.02f, seconds * .2f) / seconds;
        gb = fmaxf(0, fminf(1, (t - .5f) / width + .5f));
        ga = 1 - gb;
      }
      /* Quick cut overrides the amplitude envelope, other processing still
       * stacks. */
      if (effects & FX_QUICK_CUT) {
        float width = fminf(.02f, seconds * .2f) / seconds;
        gb = fmaxf(0, fminf(1, (t - .5f) / width + .5f));
        ga = 1 - gb;
      }
      float alpha =
          1 - expf(-6.2831853f * (18000 * powf(220.0f / 18000, t)) / RATE);
      for (int c = 0; c < 2; c++) {
        float x = ab[i * 2 + c], y = bb[i * 2 + c];
        low_a[c] += .02114f * (x - low_a[c]);
        low_b[c] += .02114f * (y - low_b[c]);
        if (effects & FX_BASS_SWAP) {
          float swap = t * t * (3 - 2 * t);
          x -= low_a[c] * swap;
          y -= low_b[c] * (1 - swap);
        }
        filter[c] += alpha * (x - filter[c]);
        if (effects & FX_FILTER)
          x = x * (1 - t) + filter[c] * t;
        if (effects & FX_ECHO) {
          float delayed = echo[echo_at * 2 + c];
          echo[echo_at * 2 + c] = x * .5f + delayed * .35f;
          float wet = .3f * t;
          x = x * (1 - wet) + delayed * wet;
        }
        float v = x * ga + y * gb;
        mix[i * 2 + c] = (int16_t)lrintf(fmaxf(-32768, fminf(32767, v)));
      }
      echo_at = (echo_at + 1) % 11025;
    }
    if (write_block(out, mix, n))
      goto finish;
    at += n;
  }
  if (copy(&b, out, cue + overlap, tail))
    goto finish;
  if (wav_finish(out)) {
    fail("Cannot finalize WAV header.");
    goto finish;
  }
  rc = 0;
finish:
  if (out) {
    if (fclose(out)) {
      rc = -1;
      fail("Failed to flush output.");
    }
    if (rc)
      remove(op);
  }
  if (a.f)
    fclose(a.f);
  if (b.f)
    fclose(b.f);
  if (rc && !render_error[0])
    fail("Audio seek failed.");
  return rc;
}
int inspect_wav(const char *path, uint32_t *frames, uint32_t *offset) {
  Source s = {0};
  int r = open_source(&s, path);
  if (!r) {
    *frames = s.frames;
    *offset = s.offset;
  }
  if (s.f)
    fclose(s.f);
  return r;
}
#ifdef HOST_TEST
#include <stdlib.h>
static int cancel_cb(uint32_t d, uint32_t t) {
  (void)t;
  return d > 1024;
}
int main(int argc, char **argv) {
  if (argc != 9)
    return 2;
  MixSettings c = {atof(argv[4]), atof(argv[5]), atof(argv[6]),
                   (unsigned)atoi(argv[7])};
  int r = render_mix(argv[1], argv[2], argv[3], &c, atoi(argv[8]),
                     getenv("CANCEL_TEST") ? cancel_cb : 0);
  if (r)
    fprintf(stderr, "%s\n", render_error);
  return r ? 1 : 0;
}
#endif
