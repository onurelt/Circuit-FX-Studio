#include "render.h"
#include "presets.h"
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
static int16_t a_edge[441*2],b_edge[441*2];
#define MAX_SFX_FRAMES (44100u * 12u)
static int16_t sample_pcm[MAX_SFX_FRAMES * 2];
#ifdef HOST_TEST
#define SAMPLE_ROOT "assets/sfx"
#else
#define SAMPLE_ROOT "app0:sfx"
#endif
static float echo[88200 * 2];
static int16_t roll[44100 * 2];
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
int render_mix_layers(const char *ap,const char *bp,const char *avp,
                      const char *bvp,const char *op,
                      const MixSettings *cfg,int preview,Progress cb) {
  if (!cfg)
    return fail("Missing transition settings.");
  TransitionPreset preset;
  int advanced = cfg->preset_id != 0;
  if (advanced &&
      (preset_get(cfg->preset_id, &preset) || !isfinite(cfg->bpm) ||
       cfg->bpm < 60 || cfg->bpm > 200 || !isfinite(cfg->intensity) ||
       cfg->intensity < 0 || cfg->intensity > 1))
    return fail("Invalid preset, BPM or intensity.");
  if (cfg->sample_speed != 0 &&
      (!isfinite(cfg->sample_speed) || cfg->sample_speed < .5f ||
       cfg->sample_speed > 2.f))
    return fail("WAV effect speed must be 0.5x to 2.0x.");
  if (cfg->sample_reverse != 0 && cfg->sample_reverse != 1)
    return fail("Invalid WAV effect direction.");
  if (cfg->bpm != 0 &&
      (!isfinite(cfg->bpm) || cfg->bpm < 60 || cfg->bpm > 200 ||
       !isfinite(cfg->intensity) || cfg->intensity < 0 || cfg->intensity > 1))
    return fail("Invalid effect BPM or intensity.");
  double seconds = cfg->duration;
  unsigned effects = cfg->effects;
  Source a = {0}, b = {0}, av = {0}, bv = {0}, sample = {0};
  uint32_t sample_start = 0, sample_count = 0;
  FILE *out = 0;
  int rc = -1;
  render_error[0] = 0;
  progress = cb;
  done = 0;
  if (!strcasecmp(ap, op) || !strcasecmp(bp, op))
    return fail("Output cannot overwrite a song.");
  if (open_source(&a, ap) || open_source(&b, bp))
    goto finish;
  if ((avp && (open_source(&av,avp) || av.frames!=a.frames)) ||
      (bvp && (open_source(&bv,bvp) || bv.frames!=b.frames))) {
    fail("Vocal overlap must match the selected song length.");
    goto finish;
  }
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
  float sample_speed = cfg->sample_speed >= .5f && cfg->sample_speed <= 2.f
                           ? cfg->sample_speed
                           : 1.f;
  int sample_reverse = cfg->sample_reverse != 0;
  int sample_riser = 0;
  uint32_t source_count = 0, source_offset = 0;
  if (advanced && preset.sample_id) {
    char path[96];
    snprintf(path, sizeof(path), SAMPLE_ROOT "/%02d.wav", preset.sample_id);
    if (open_source(&sample, path)) {
      fail("Bundled WAV missing or damaged. Reinstall Beta 1.6.");
      goto finish;
    }
    if (sample.frames > MAX_SFX_FRAMES) {
      fail("Bundled WAV is too long to process.");
      goto finish;
    }
    uint32_t max_source = (uint32_t)(overlap * sample_speed);
    source_count = sample.frames < max_source ? sample.frames : max_source;
    sample_riser = preset.sample_id >= 9 && preset.sample_id <= 14;
    /* Short overlaps use a riser's ending segment so it resolves on the cut. */
    if (sample_riser && sample.frames > source_count)
      source_offset = sample.frames - source_count;
    if (seek_frame(&sample, source_offset) ||
        fread(sample_pcm, 4, source_count, sample.f) != source_count) {
      fail("Bundled WAV read failed.");
      goto finish;
    }
    sample_count = (uint32_t)ceilf(source_count / sample_speed);
    if (sample_count > overlap)
      sample_count = overlap;
    sample_start =
        sample_riser ? overlap - sample_count : (overlap - sample_count) / 2;
  }
  uint32_t tail = b.frames - cue - overlap, start = 0;
  if (preview) {
    start = pre > RATE * 10 ? pre - RATE * 10 : 0;
    if (tail > RATE * 10)
      tail = RATE * 10;
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
  uint32_t edge=overlap<441?overlap:441;
  if (avp && (seek_frame(&a,pre) || fread(a_edge,4,edge,a.f)!=edge)) {
    fail("Cannot read original A for vocal edge.");goto finish;
  }
  if (bvp && (seek_frame(&b,cue+overlap-edge) ||
              fread(b_edge,4,edge,b.f)!=edge)) {
    fail("Cannot read original B for vocal edge.");goto finish;
  }
  if (copy(&a, out, start, pre - start) ||
      seek_frame(avp?&av:&a,pre) || seek_frame(bvp?&bv:&b,cue))
    goto finish;
  memset(echo, 0, sizeof(echo));
  float low_a[2] = {0}, low_b[2] = {0}, filter[2] = {0};
  unsigned echo_at = 0;
  float strength = (cfg->bpm >= 60 && cfg->bpm <= 200) ? cfg->intensity : 1;
  unsigned delay_frames =
      advanced ? (unsigned)(44100 * 60 / cfg->bpm * preset.delay_beats) : 11025;
  if (delay_frames < 1)
    delay_frames = 1;
  if (delay_frames > 88200)
    delay_frames = 88200;
  unsigned roll_frames =
      advanced ? (unsigned)(44100 * 60 / cfg->bpm * preset.roll_beats) : 1;
  if (roll_frames < 1)
    roll_frames = 1;
  if (roll_frames > 44100)
    roll_frames = 44100;
  unsigned mods = advanced ? preset.mods : 0;
  float noise_low = 0, noise_high = 0, phase = 0;
  uint32_t rng = 0x73519u;
  float feedback = advanced ? preset.feedback : .35f;
  float filter_end = advanced ? preset.filter_end : 220.f;
  float curve = advanced ? preset.curve : 1.f;
  float bass_pivot = advanced ? preset.bass_pivot : .5f;
  float cut_at = advanced ? preset.cut_at : .5f;
  float wet_max = advanced ? preset.wet : .3f;
  float alpha = 1;

  for (uint32_t at = 0; at < overlap;) {
    uint32_t n = overlap - at;
    if (n > BLOCK)
      n = BLOCK;
    if (fread(ab, 4, n, avp?av.f:a.f) != n ||
        fread(bb, 4, n, bvp?bv.f:b.f) != n) {
      fail("Input read failed during transition.");
      goto finish;
    }
    for (uint32_t i = 0; i < n; i++) {
      float t = (float)(at + i) / (overlap - 1);
      float shaped = advanced ? powf(t, curve) : t, ga = 1 - shaped,
            gb = shaped;
      if (!(effects & FX_CROSSFADE)) {
        float width = fminf(.02f, seconds * .2f) / seconds;
        gb = fmaxf(0, fminf(1, (t - cut_at) / width + .5f));
        ga = 1 - gb;
      }
      /* Quick cut overrides the amplitude envelope, other processing still
       * stacks. */
      if (effects & FX_QUICK_CUT) {
        float width = fminf(.02f, seconds * .2f) / seconds;
        gb = fmaxf(0, fminf(1, (t - cut_at) / width + .5f));
        ga = 1 - gb;
      }
      /* Slow coefficient updates avoid two transcendental calls per sample. */
      if (((at + i) & 31u) == 0) {
        float hz = (mods & MOD_HIGH_PASS) ? 30 * powf(filter_end / 30, t)
                                          : 18000 * powf(filter_end / 18000, t);
        alpha = 1 - expf(-6.2831853f * hz / RATE);
      }
      float envelope = 4 * t * (1 - t), pulse = 1, noise = 0;
      if (mods & MOD_DIP) {
        float dip = 1 - strength * (1 - fabsf(2 * t - 1));
        ga *= dip;
        gb *= dip;
      }
      if (mods & MOD_GATE) {
        float beat = (at + i) / (float)RATE * cfg->bpm / 60 / preset.gate_beats;
        float wave = .5f + .5f * cosf(6.2831853f * beat);
        pulse = 1 - strength * preset.gate_depth * envelope * (1 - wave);
      }
      if (mods & MOD_NOISE) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        float white = ((rng & 65535) / 32767.5f - 1);
        noise_low += .025f * (white - noise_low);
        noise_high += .35f * (white - noise_high);
        noise = (noise_high - noise_low) * 32767 * preset.noise_mix * strength *
                envelope;
      }
      if (mods & MOD_RING) {
        phase += 6.2831853f * (preset.ring_hz * (.3f + 1.7f * t)) / RATE;
        if (phase > 6.2831853f)
          phase -= 6.2831853f;
      }
      float echo_old[2] = {echo[echo_at * 2], echo[echo_at * 2 + 1]};
      for (int c = 0; c < 2; c++) {
        float x = ab[i * 2 + c], y = bb[i * 2 + c];
        if (avp && at+i<edge) {
          float fade=(at+i)/(float)edge;
          x=a_edge[(at+i)*2+c]*(1-fade)+x*fade;
        }
        if (bvp && at+i>=overlap-edge) {
          unsigned edge_at=at+i-(overlap-edge);
          float fade=edge_at/(float)edge;
          y=y*(1-fade)+b_edge[edge_at*2+c]*fade;
        }
        if (mods & MOD_ROLL) {
          unsigned pos = at + i;
          if (pos < roll_frames)
            roll[pos * 2 + c] = (int16_t)x;
          else {
            float ramp = fminf(1, (pos - roll_frames) / 220.5f);
            float w = strength * envelope * ramp;
            /* Window at loop seams reduces clicks without unbounded storage. */
            unsigned slot = pos % roll_frames;
            float edge = fminf(1, fminf(slot, roll_frames - 1 - slot) / 110.f);
            x = x * (1 - w) + roll[slot * 2 + c] * w * edge;
          }
        }
        x *= pulse;
        if (mods & MOD_RING) {
          float w = preset.ring_mix * strength * envelope;
          x = x * (1 - w) + x * sinf(phase) * w;
        }
        low_a[c] += .02114f * (x - low_a[c]);
        low_b[c] += .02114f * (y - low_b[c]);
        if (effects & FX_BASS_SWAP) {
          float z = t < bass_pivot
                        ? .5f * t / bass_pivot
                        : .5f + .5f * (t - bass_pivot) / (1 - bass_pivot);
          float swap = z * z * (3 - 2 * z);
          x -= low_a[c] * swap * strength;
          y -= low_b[c] * (1 - swap) * strength;
        }
        filter[c] += alpha * (x - filter[c]);
        if (effects & FX_FILTER)
          x = x * (1 - t * strength) +
              ((mods & MOD_HIGH_PASS) ? x - filter[c] : filter[c]) * t *
                  strength;
        if (effects & FX_ECHO) {
          float delayed = echo_old[c];
          /* Cross-channel reads captured before either write for stereo
           * symmetry. */
          float feedback_sample = echo_old[(mods & MOD_PINGPONG) ? 1 - c : c];
          echo[echo_at * 2 + c] = x * .5f + feedback_sample * feedback;
          float wet = wet_max * t * strength;
          x = x * (1 - wet) + delayed * wet;
        }
        float v = x * ga + y * gb + noise;
        if (sample.f && at + i >= sample_start &&
            at + i < sample_start + sample_count) {
          uint32_t pos = at + i - sample_start;
          /* 10 ms edges also protect cropped sounds from clicks. Headroom
           * scales with volume so intensity zero is an exact clean blend. */
          float edge = fminf(1, fminf(pos, sample_count - 1 - pos) / 441.f);
          float level = .32f * strength * edge;
          float source_pos = pos * sample_speed;
          if (sample_reverse)
            source_pos = (source_count - 1) - source_pos;
          if (source_pos < 0)
            source_pos = 0;
          if (source_pos > source_count - 1)
            source_pos = source_count - 1;
          uint32_t si = (uint32_t)source_pos;
          uint32_t sj = si + 1 < source_count ? si + 1 : si;
          float frac = source_pos - si;
          float fx = sample_pcm[si * 2 + c] * (1 - frac) +
                     sample_pcm[sj * 2 + c] * frac;
          v = v * (1 - level) + fx * level;
        }
        mix[i * 2 + c] = (int16_t)lrintf(fmaxf(-32768, fminf(32767, v)));
      }
      echo_at = (echo_at + 1) % delay_frames;
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
  if (sample.f)
    fclose(sample.f);
  if (a.f)
    fclose(a.f);
  if (b.f)
    fclose(b.f);
  if (av.f) fclose(av.f);
  if (bv.f) fclose(bv.f);
  if (rc && !render_error[0])
    fail("Audio seek failed.");
  return rc;
}
int render_mix(const char *a,const char *b,const char *out,
               const MixSettings *cfg,int preview,Progress cb) {
  return render_mix_layers(a,b,NULL,NULL,out,cfg,preview,cb);
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
  MixSettings c = {.start_a = atof(argv[4]),
                   .start_b = atof(argv[5]),
                   .duration = atof(argv[6]),
                   .effects = (unsigned)atoi(argv[7])};
  if (getenv("PRESET_TEST")) {
    c.preset_id = atoi(getenv("PRESET_TEST"));
    c.bpm = getenv("BPM_TEST") ? atof(getenv("BPM_TEST")) : 128;
    c.intensity = getenv("INTENSITY_TEST") ? atof(getenv("INTENSITY_TEST")) : 1;
    c.sample_speed = getenv("SPEED_TEST") ? atof(getenv("SPEED_TEST")) : 1;
    c.sample_reverse =
        getenv("REVERSE_TEST") ? atoi(getenv("REVERSE_TEST")) : 0;
    TransitionPreset p;
    if (!preset_get(c.preset_id, &p)) {
      c.effects = p.effects;
      if (p.reset_sample_settings) {
        c.sample_reverse = p.default_reverse;
        c.sample_speed = p.default_speed > 0 ? p.default_speed : 1.f;
      }
    }
  }
  int r = render_mix(argv[1], argv[2], argv[3], &c, atoi(argv[8]),
                     getenv("CANCEL_TEST") ? cancel_cb : 0);
  if (r)
    fprintf(stderr, "%s\n", render_error);
  return r ? 1 : 0;
}
#endif
