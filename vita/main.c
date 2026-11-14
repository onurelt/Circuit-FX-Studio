#include "ui.h"
#include "mp3_import.h"
#include <math.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <psp2/touch.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define ROOT "ux0:data/CircuitFX"
#define PREVIEW ROOT "/preview.wav"
#define MAX_SONGS 512
#define CACHE_LIMIT_BYTES (512LL * 1024LL * 1024LL)
static View view;
static char songs[MAX_SONGS][256], paths[2][512];
static char cache_paths[2][512];
static int count, ready, back;
static uint32_t previous, held;
static uint32_t *buffers[2];
static SceUID blocks[2];
static int timeline_dirty = 1;
static uint64_t timeline_due;
static int touch_x, touch_y, touch_down;
static int screen_dirty = 1;
static uint64_t draw_total_us, draw_max_us, draw_count;
static int progress(uint32_t done, uint32_t total);
static void logline(const char *s);
static int has_suffix(const char *name, const char *suffix) {
  size_t n = strlen(name), s = strlen(suffix);
  return n >= s && !strcasecmp(name + n - s, suffix);
}
static uint32_t filename_hash(const char *name) {
  uint32_t hash = 2166136261u;
  while (*name)
    hash = (hash ^ (unsigned char)*name++) * 16777619u;
  return hash;
}
static uint32_t track_signature(const char *path, SceIoStat *st) {
  if (!path || sceIoGetstat(path, st) < 0 || SCE_S_ISDIR(st->st_mode))
    return 0;
  uint32_t h = filename_hash(path);
  const unsigned char *p = (const unsigned char *)&st->st_size;
  for (size_t i = 0; i < sizeof(st->st_size); ++i)
    h = (h ^ p[i]) * 16777619u;
  const unsigned short stamp[] = {st->st_mtime.year, st->st_mtime.month,
                                  st->st_mtime.day,  st->st_mtime.hour,
                                  st->st_mtime.minute, st->st_mtime.second};
  for (size_t i = 0; i < sizeof(stamp) / sizeof(stamp[0]); ++i) {
    h = (h ^ (unsigned char)stamp[i]) * 16777619u;
    h = (h ^ (unsigned char)(stamp[i] >> 8)) * 16777619u;
  }
  return h ? h : 1;
}
static int older_date(const SceDateTime *a, const SceDateTime *b) {
  const unsigned short av[] = {a->year, a->month, a->day, a->hour,
                               a->minute, a->second};
  const unsigned short bv[] = {b->year, b->month, b->day, b->hour,
                               b->minute, b->second};
  for (size_t i = 0; i < sizeof(av) / sizeof(av[0]); ++i) {
    if (av[i] != bv[i])
      return av[i] < bv[i];
  }
  return 0;
}
static const SceDateTime *cache_age(const SceIoStat *st) {
  return st->st_atime.year ? &st->st_atime : &st->st_mtime;
}
static int cache_protected(const char *path) {
  for (int i = 0; i < 2; ++i)
    if (cache_paths[i][0] && !strcmp(cache_paths[i], path))
      return 1;
  return 0;
}
/* Keep useful conversions across launches. Partial files are never reusable;
 * when the cache exceeds 512 MiB, evict the least recently opened item first. */
static void prune_cache(void) {
  int d = sceIoDopen(ROOT "/cache");
  if (d < 0)
    return;
  int64_t total = 0;
  SceIoDirent e;
  memset(&e, 0, sizeof(e));
  while (sceIoDread(d, &e) > 0) {
    if (!SCE_S_ISDIR(e.d_stat.st_mode) && strcmp(e.d_name, ".") &&
        strcmp(e.d_name, "..")) {
      char p[512];
      snprintf(p, sizeof(p), ROOT "/cache/%s", e.d_name);
      size_t n = strlen(p);
      if (n >= 5 && !strcmp(p + n - 5, ".part"))
        remove(p);
      else if (e.d_stat.st_size > 0)
        total += e.d_stat.st_size;
    }
    memset(&e, 0, sizeof(e));
  }
  sceIoDclose(d);
  while (total > CACHE_LIMIT_BYTES) {
    d = sceIoDopen(ROOT "/cache");
    if (d < 0)
      break;
    char oldest[512] = {0};
    SceDateTime oldest_at = {0};
    int have_oldest = 0;
    int64_t oldest_size = 0;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(d, &e) > 0) {
      if (!SCE_S_ISDIR(e.d_stat.st_mode) && strcmp(e.d_name, ".") &&
          strcmp(e.d_name, "..")) {
        char p[512];
        snprintf(p, sizeof(p), ROOT "/cache/%s", e.d_name);
        const SceDateTime *age = cache_age(&e.d_stat);
        if (!cache_protected(p) && e.d_stat.st_size > 0 &&
            (!have_oldest || older_date(age, &oldest_at))) {
          snprintf(oldest, sizeof(oldest), "%s", p);
          oldest_at = *age;
          oldest_size = e.d_stat.st_size;
          have_oldest = 1;
        }
      }
      memset(&e, 0, sizeof(e));
    }
    sceIoDclose(d);
    if (!have_oldest || remove(oldest))
      break;
    total -= oldest_size;
  }
}
static void cache_touch(const char *path) {
  SceIoStat st;
  if (sceIoGetstat(path, &st) >= 0 &&
      sceRtcGetCurrentClockLocalTime(&st.st_atime) >= 0) {
    st.st_mtime = st.st_atime;
    sceIoChstat(path, &st, SCE_CST_AT | SCE_CST_MT);
  }
}
static int touch_press(void) {
  SceTouchData t = {0};
  sceTouchPeek(SCE_TOUCH_PORT_FRONT, &t, 1);
  int pressed = t.reportNum && !touch_down;
  touch_down = t.reportNum > 0;
  if (t.reportNum) {
    touch_x = t.report[0].x / 2;
    touch_y = t.report[0].y / 2;
  }
  return pressed;
}
static void refresh_timeline(void) {
  uint64_t started = sceKernelGetProcessTimeWide();
  memset(view.timeline, 0, sizeof(view.timeline));
  double pre = ui_timeline_pre(&view), span = ui_timeline_span(&view);
  for (int d = 0; d < 2; d++) {
    if (!view.songs[d] || view.lengths[d] <= 0)
      continue;
    for (int x = 0; x < TIMELINE_BINS; x++) {
      double rel = span * x / TIMELINE_BINS - pre;
      if ((d == 0 && rel >= view.settings.duration) || (d == 1 && rel < 0))
        continue;
      double t = (d ? view.settings.start_b : view.settings.start_a) + rel;
      if (t < 0 || t >= view.lengths[d])
        continue;
      int bin = (int)(t / view.lengths[d] * 440.0);
      if (bin < 0)
        bin = 0;
      if (bin > 439)
        bin = 439;
      view.timeline[d][x] = view.peaks[d][bin];
    }
  }
  timeline_dirty = 0;
  /* Timeline generation now uses the in-memory waveform envelope; it never
   * seeks across the tracks for every cue, tempo, or effect adjustment. */
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING timeline_ms=%llu",
           (unsigned long long)(sceKernelGetProcessTimeWide() - started) / 1000);
  logline(timing);
}

static void logline(const char *s) {
  FILE *f = fopen(ROOT "/session.log", "a");
  if (f) {
    fprintf(f, "%s\n", s);
    fclose(f);
  }
}
static void show(void) {
  uint64_t started = sceKernelGetProcessTimeWide();
  ui_draw(buffers[back], &view);
  uint64_t render_us = sceKernelGetProcessTimeWide() - started;
  SceDisplayFrameBuf frame = {sizeof(frame), buffers[back],
                              960,           SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
                              960,           544};
  sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
  sceDisplayWaitVblankStart();
  back ^= 1;
  screen_dirty = 0;
  draw_total_us += render_us;
  draw_count++;
  if (render_us > draw_max_us)
    draw_max_us = render_us;
}
static uint32_t buttons(void) {
  SceCtrlData p = {0};
  sceCtrlPeekBufferPositive(0, &p, 1);
  held = p.buttons;
  uint32_t hit = held & ~previous;
  static uint64_t repeat_at;
  uint64_t now = sceKernelGetProcessTimeWide();
  uint32_t dirs =
      held & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT);
  if (hit || !dirs)
    repeat_at = now + 350000;
  else if (now >= repeat_at) {
    hit |= dirs;
    repeat_at = now + 70000;
  }
  previous = held;
  return hit;
}
static void message(const char *s) {
  snprintf(view.status, sizeof(view.status), "%s", s);
  logline(s);
  screen_dirty = 1;
}
static void invalidate(void) {
  ready = view.ready = 0;
  timeline_dirty = 1;
  timeline_due = sceKernelGetProcessTimeWide() + 200000;
  remove(PREVIEW);
  screen_dirty = 1;
}
static int compare(const void *a, const void *b) { return strcasecmp(a, b); }
static void scan(void) {
  uint64_t started = sceKernelGetProcessTimeWide();
  count = 0;
  int d = sceIoDopen(ROOT "/music");
  if (d < 0) {
    message("Cannot open music folder.");
    return;
  }
  SceIoDirent e;
  memset(&e, 0, sizeof(e));
  while (sceIoDread(d, &e) > 0) {
    if (!SCE_S_ISDIR(e.d_stat.st_mode) && count < MAX_SONGS &&
        (has_suffix(e.d_name, ".wav") || has_suffix(e.d_name, ".mp3")))
      snprintf(songs[count++], 256, "%s", e.d_name);
    memset(&e, 0, sizeof(e));
  }
  sceIoDclose(d);
  qsort(songs, count, 256, compare);
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING scan_ms=%llu files=%d",
           (unsigned long long)(sceKernelGetProcessTimeWide() - started) / 1000,
           count);
  logline(timing);
}
static void load_peaks(int deck, uint32_t frames, uint32_t offset) {
  uint64_t started = sceKernelGetProcessTimeWide();
  SceIoStat st;
  uint32_t signature = track_signature(paths[deck], &st);
  char peak_path[512], peak_part[512];
  snprintf(peak_path, sizeof(peak_path), ROOT "/cache/wave-%08x.bin",
           signature);
  snprintf(peak_part, sizeof(peak_part), ROOT "/cache/wave-%08x.part",
           signature);
  /* The same track can occupy both decks. Share its already-computed envelope. */
  int other = deck ^ 1;
  if (paths[other][0] && !strcmp(paths[other], paths[deck])) {
    memcpy(view.peaks[deck], view.peaks[other], sizeof(view.peaks[deck]));
    return;
  }
  FILE *cached = fopen(peak_path, "rb");
  if (cached) {
    size_t got = fread(view.peaks[deck], 1, sizeof(view.peaks[deck]), cached);
    int extra = fgetc(cached);
    fclose(cached);
    if (got == sizeof(view.peaks[deck]) && extra == EOF) {
      cache_touch(peak_path);
      char timing[128];
      snprintf(timing, sizeof(timing), "TIMING waveform_cache_hit_ms=%llu",
               (unsigned long long)(sceKernelGetProcessTimeWide() - started) /
                   1000);
      logline(timing);
      return;
    }
    remove(peak_path);
  }
  FILE *f = fopen(paths[deck], "rb");
  memset(view.peaks[deck], 0, sizeof(view.peaks[deck]));
  if (!f) {
    char timing[128];
    snprintf(timing, sizeof(timing), "TIMING waveform_ms=%llu result=open_failed",
             (unsigned long long)(sceKernelGetProcessTimeWide() - started) /
                 1000);
    logline(timing);
    return;
  }
  int16_t samples[128];
  for (int x = 0; x < 440; x++) {
    uint32_t at = (uint64_t)x * frames / 440;
    unsigned n = frames - at;
    if (n > 64)
      n = 64;
    if (fseek(f, offset + (long)at * 4, SEEK_SET))
      break;
    size_t got = fread(samples, 2, n * 2, f);
    int peak = 0;
    for (size_t k = 0; k < got; k++) {
      int value = abs((int)samples[k]);
      if (value > peak)
        peak = value;
    }
    view.peaks[deck][x] = peak;
  }
  fclose(f);
  FILE *out = fopen(peak_part, "wb");
  if (out) {
    size_t written = fwrite(view.peaks[deck], 1, sizeof(view.peaks[deck]), out);
    int close_error = fclose(out);
    if (written == sizeof(view.peaks[deck]) && !close_error)
      rename(peak_part, peak_path);
    else
      remove(peak_part);
  }
  cache_touch(peak_path);
  prune_cache();
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING waveform_ms=%llu bins=440",
           (unsigned long long)(sceKernelGetProcessTimeWide() - started) / 1000);
  logline(timing);
}
static struct {
  atomic_int cancel, done, result, too_large;
  atomic_uint frames, total;
  char input[512], output[512];
} mp3_job;
static int mp3_worker_progress(uint32_t done, uint32_t total) {
  atomic_store(&mp3_job.frames, done);
  atomic_store(&mp3_job.total, total);
  if ((uint64_t)total * 4 + 44 > CACHE_LIMIT_BYTES) {
    atomic_store(&mp3_job.too_large, 1);
    return 1;
  }
  return atomic_load(&mp3_job.cancel);
}
static int mp3_worker(SceSize args, void *argp) {
  (void)args;
  (void)argp;
  int rc = mp3_import_to_wav(mp3_job.input, mp3_job.output,
                             mp3_worker_progress);
  atomic_store(&mp3_job.result, rc);
  atomic_store(&mp3_job.done, 1);
  return 0;
}
static int decode_mp3_async(const char *input, const char *output) {
  uint64_t started = sceKernelGetProcessTimeWide();
  snprintf(mp3_job.input, sizeof(mp3_job.input), "%s", input);
  snprintf(mp3_job.output, sizeof(mp3_job.output), "%s", output);
  atomic_store(&mp3_job.cancel, 0);
  atomic_store(&mp3_job.done, 0);
  atomic_store(&mp3_job.result, -1);
  atomic_store(&mp3_job.too_large, 0);
  atomic_store(&mp3_job.frames, 0);
  atomic_store(&mp3_job.total, 0);
  SceUID thread = sceKernelCreateThread("CircuitFX MP3", mp3_worker,
                                        0x10000100, 128 * 1024, 0, 0, NULL);
  if (thread < 0)
    return -1;
  if (sceKernelStartThread(thread, 0, NULL) < 0) {
    sceKernelDeleteThread(thread);
    return -1;
  }
  view.progress = 0;
  screen_dirty = 1;
  show();
  int last_percent = -1;
  uint64_t power_tick = sceKernelGetProcessTimeWide() + 500000;
  while (!atomic_load(&mp3_job.done)) {
    uint32_t hit = buttons();
    int tapped = touch_press();
    if ((hit & SCE_CTRL_CIRCLE) ||
        (tapped && touch_x >= 192 && touch_x < 768 && touch_y >= 185 &&
         touch_y < 349))
      atomic_store(&mp3_job.cancel, 1);
    unsigned total = atomic_load(&mp3_job.total);
    unsigned done = atomic_load(&mp3_job.frames);
    int percent = total ? (int)((uint64_t)done * 100 / total) : 0;
    if (percent > 100)
      percent = 100;
    if (percent != last_percent) {
      view.progress = percent;
      show();
      last_percent = percent;
    }
    uint64_t now = sceKernelGetProcessTimeWide();
    if (now >= power_tick) {
      sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
      power_tick = now + 500000;
    }
    sceKernelDelayThread(10000);
  }
  sceKernelWaitThreadEnd(thread, NULL, NULL);
  sceKernelDeleteThread(thread);
  view.progress = -1;
  int rc = atomic_load(&mp3_job.result);
  screen_dirty = 1;
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING mp3_import_ms=%llu result=%d",
           (unsigned long long)(sceKernelGetProcessTimeWide() - started) / 1000,
           rc);
  logline(timing);
  return rc;
}
static void browse(int deck) {
  view.browser = 1;
  view.browser_pos = 0;
  view.browser_count = count;
  int drawn_pos = -1;
  while (view.browser) {
    view.first = view.browser_pos / 10 * 10;
    for (int i = 0; i < 10; i++)
      view.entries[i] = view.first + i < count ? songs[view.first + i] : NULL;
    if (drawn_pos != view.browser_pos) {
      show();
      drawn_pos = view.browser_pos;
    }
    uint32_t hit = buttons();
    if (touch_press()) {
      if (touch_x >= 82 && touch_x < 878 && touch_y >= 132 && touch_y < 442) {
        int selected = view.first + (touch_y - 132) / 31;
        if (selected < count) {
          view.browser_pos = selected;
          hit |= SCE_CTRL_CROSS;
        }
      } else if (touch_y >= 455 && touch_y < 495) {
        if (touch_x >= 814)
          view.browser_pos =
              (view.first + 10 < count) ? view.first + 10 : view.browser_pos;
        else if (touch_x >= 730)
          view.browser_pos = view.first >= 10 ? view.first - 10 : 0;
      }
    }
    if (hit & SCE_CTRL_CIRCLE)
      break;
    if (!count)
      continue;
    if (hit & SCE_CTRL_UP)
      view.browser_pos = (view.browser_pos + count - 1) % count;
    if (hit & SCE_CTRL_DOWN)
      view.browser_pos = (view.browser_pos + 1) % count;
    if (hit & SCE_CTRL_CROSS) {
      char path[512], source_path[512], cache_path[512], part_path[512];
      uint64_t selection_started = sceKernelGetProcessTimeWide();
      snprintf(path, sizeof(path), ROOT "/music/%s", songs[view.browser_pos]);
      int is_mp3 = has_suffix(path, ".mp3");
      int use_cached_mp3 = 0;
      if (is_mp3) {
        SceIoStat input_stat;
        uint32_t hash = track_signature(path, &input_stat);
        if (!hash) {
          message("Cannot read MP3 file information.");
          break;
        }
        snprintf(cache_path, sizeof(cache_path),
                 ROOT "/cache/mp3-%08x.wav", hash);
        snprintf(part_path, sizeof(part_path),
                 ROOT "/cache/mp3-%08x.part", hash);
        SceIoStat cached;
        uint32_t cached_frames, cached_offset;
        use_cached_mp3 = sceIoGetstat(cache_path, &cached) >= 0 &&
                         !inspect_wav(cache_path, &cached_frames,
                                      &cached_offset);
        if (sceIoGetstat(cache_path, &cached) >= 0 && !use_cached_mp3)
          remove(cache_path);
        if (!use_cached_mp3) {
          remove(part_path);
          message("Decoding MP3. Circle or tap the dialog to cancel.");
          int decode_result = decode_mp3_async(path, part_path);
          if (decode_result) {
            remove(part_path);
            message(atomic_load(&mp3_job.too_large)
                        ? "Decoded song exceeds the 512 MB cache limit."
                        : mp3_import_error[0]
                              ? mp3_import_error
                              : "Could not start MP3 decoder.");
            break;
          }
          if (rename(part_path, cache_path)) {
            remove(part_path);
            message("Could not keep the decoded song. Check storage space.");
            break;
          }
          if (sceIoGetstat(cache_path, &cached) >= 0 &&
              cached.st_size > CACHE_LIMIT_BYTES) {
            remove(cache_path);
            message("Decoded song exceeds the 512 MB cache limit.");
            break;
          }
        } else
          logline("TIMING mp3_cache_hit=1");
        cache_touch(cache_path);
        snprintf(source_path, sizeof(source_path), "%s", cache_path);
      } else {
        snprintf(source_path, sizeof(source_path), "%s", path);
      }
      uint32_t frames, offset;
      if (inspect_wav(source_path, &frames, &offset)) {
        if (is_mp3)
          remove(cache_path);
        message(render_error);
        break;
      }
      if (is_mp3)
        snprintf(cache_paths[deck], sizeof(cache_paths[deck]), "%s", cache_path);
      else
        cache_paths[deck][0] = 0;
      snprintf(paths[deck], sizeof(paths[deck]), "%s", source_path);
      view.songs[deck] = strrchr(paths[deck], '/') + 1;
      view.lengths[deck] = (double)frames / 44100;
      invalidate();
      if (deck == 0)
        view.settings.start_a =
            fmax(0, view.lengths[0] - view.settings.duration);
      else
        view.settings.start_b = 0;
      load_peaks(deck, frames, offset);
      prune_cache();
      char timing[128];
      snprintf(timing, sizeof(timing), "TIMING song_select_ms=%llu format=%s",
               (unsigned long long)(sceKernelGetProcessTimeWide() -
                                    selection_started) /
                   1000,
               is_mp3 ? "MP3" : "WAV");
      logline(timing);
      message("Song loaded. Set cue times and preview.");
      break;
    }
    sceKernelDelayThread(2000);
  }
  view.browser = 0;
  screen_dirty = 1;
}
static int progress(uint32_t done, uint32_t total) {
  static int last = -1;
  int pc = total ? (int)((uint64_t)done * 100 / total) : 0;
  if (pc != last) {
    view.progress = pc;
    show();
    last = pc;
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
  }
  buttons();
  return (held & SCE_CTRL_CIRCLE) != 0;
}
/* Audio owns its port and file on a worker; GUI never blocks the audio feed. */
static atomic_int audio_stop, audio_done, audio_result;
static atomic_uint audio_frames;
static char audio_path[256];
static uint32_t audio_offset, audio_length, audio_output_length;
static int audio_sfx_mode;
static int16_t audition_sfx[44100 * 12 * 2] __attribute__((aligned(64)));
static int audio_worker(SceSize args, void *argp) {
  (void)args;
  (void)argp;
  int result = -1, port = -1;
  FILE *f = fopen(audio_path, "rb");
  static int16_t pcm[2][2048] __attribute__((aligned(64)));
  if (!f || fseek(f, audio_offset, SEEK_SET))
    goto finish;
  port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 44100,
                             SCE_AUDIO_OUT_MODE_STEREO);
  if (port < 0)
    goto finish;
  int vols[2] = {SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB};
  if (sceAudioOutSetVolume(
          port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vols) <
      0)
    goto finish;
  result = 0;
  int index = 0;
  uint32_t remaining = audio_output_length;
  while (!atomic_load(&audio_stop)) {
    memset(pcm[index], 0, sizeof(pcm[index]));
    size_t count = remaining < 1024 ? remaining : 1024;
    if (!count)
      break;
    size_t n = count;
    if (audio_sfx_mode) {
      uint32_t output_at = atomic_load(&audio_frames);
      for (size_t i = 0; i < n; ++i) {
        float src = (output_at + i) * view.settings.sample_speed;
        if (view.settings.sample_reverse)
          src = (audio_length - 1) - src;
        if (src < 0)
          src = 0;
        if (src > audio_length - 1)
          src = audio_length - 1;
        uint32_t lo = (uint32_t)src;
        uint32_t hi = lo + 1 < audio_length ? lo + 1 : lo;
        float frac = src - lo;
        for (int ch = 0; ch < 2; ++ch)
          pcm[index][i * 2 + ch] =
              (int16_t)(audition_sfx[lo * 2 + ch] * (1 - frac) +
                        audition_sfx[hi * 2 + ch] * frac);
      }
    } else {
      n = fread(pcm[index], 4, count, f);
      if (n != count) {
        result = -1;
        break;
      }
    }
    remaining -= n;
    if (sceAudioOutOutput(port, pcm[index]) < 0) {
      result = -1;
      break;
    }
    atomic_fetch_add(&audio_frames, (unsigned)n);
    index ^= 1;
  }
finish:
  if (port >= 0) {
    sceAudioOutOutput(port, NULL);
    sceAudioOutReleasePort(port);
  }
  if (f)
    fclose(f);
  atomic_store(&audio_result, result);
  atomic_store(&audio_done, 1);
  return 0;
}
static int play_file(const char *path, int transform_sfx) {
  uint32_t frames, offset;
  if (inspect_wav(path, &frames, &offset))
    return -1;
  snprintf(audio_path, sizeof(audio_path), "%s", path);
  audio_length = frames;
  audio_offset = transform_sfx ? frames : offset;
  audio_sfx_mode = transform_sfx;
  audio_output_length =
      transform_sfx ? (uint32_t)ceilf(frames / view.settings.sample_speed)
                    : frames;
  if (transform_sfx) {
    if (frames > 44100u * 12u)
      return -1;
    FILE *sfx = fopen(path, "rb");
    if (!sfx || fseek(sfx, offset, SEEK_SET) ||
        fread(audition_sfx, 4, frames, sfx) != frames) {
      if (sfx)
        fclose(sfx);
      return -1;
    }
    fclose(sfx);
  }
  view.preview_length = (double)audio_output_length / 44100;
  view.play_position = 0;
  view.playing = 1;
  atomic_store(&audio_stop, 0);
  atomic_store(&audio_done, 0);
  atomic_store(&audio_result, -1);
  atomic_store(&audio_frames, 0);
  SceUID thread = sceKernelCreateThread("CircuitFX Audio", audio_worker,
                                        0x10000100, 65536, 0, 0, NULL);
  if (thread < 0) {
    view.playing = 0;
    return -1;
  }
  if (sceKernelStartThread(thread, 0, NULL) < 0) {
    sceKernelDeleteThread(thread);
    view.playing = 0;
    return -1;
  }
  message("Playing preview. Tap Stop, Circle or START to stop.");
  while (!atomic_load(&audio_done)) {
    view.play_position = (double)atomic_load(&audio_frames) / 44100;
    show();
    unsigned hit = buttons();
    if ((hit & (SCE_CTRL_CIRCLE | SCE_CTRL_START | SCE_CTRL_SQUARE)) ||
        (touch_press() &&
         (view.preset_browser || ui_hit_test(touch_x, touch_y) == 10)))
      atomic_store(&audio_stop, 1);
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
  }
  sceKernelWaitThreadEnd(thread, NULL, NULL);
  sceKernelDeleteThread(thread);
  view.playing = 0;
  view.play_position = 0;
  return atomic_load(&audio_result);
}
static int play(void) { return play_file(PREVIEW, 0); }
static void preview(void) {
  if (!view.songs[0] || !view.songs[1]) {
    message("Choose both songs first.");
    return;
  }
  invalidate();
  refresh_timeline();
  message("Rendering preview...");
  view.progress = 0;
  show();
  uint64_t render_started = sceKernelGetProcessTimeWide();
  int rc = render_mix(paths[0], paths[1], PREVIEW, &view.settings, 1, progress);
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING preview_render_ms=%llu result=%d",
           (unsigned long long)(sceKernelGetProcessTimeWide() - render_started) /
               1000,
           rc);
  logline(timing);
  view.progress = -1;
  if (rc) {
    message(render_error);
    return;
  }
  ready = view.ready = 1;
  if (play())
    message("Audio playback failed. Square retries.");
  else
    message("Preview ready. Export when satisfied.");
}
static void export_mix(void) {
  if (!ready) {
    message("Preview these settings before exporting.");
    return;
  }
  view.confirm = 1;
  int yes = 0;
  int redraw = 1;
  while (1) {
    if (redraw) {
      show();
      redraw = 0;
    }
    unsigned h = buttons();
    int tapped = touch_press();
    if (h || tapped)
      redraw = 1;
    if (tapped && touch_y >= 312 && touch_y < 354) {
      if (touch_x >= 201 && touch_x < 458)
        h |= SCE_CTRL_CIRCLE;
      if (touch_x >= 486 && touch_x < 759)
        h |= SCE_CTRL_CROSS;
    }
    if (h & SCE_CTRL_CIRCLE)
      break;
    if (h & SCE_CTRL_CROSS) {
      yes = 1;
      break;
    }
    sceKernelDelayThread(4000);
  }
  view.confirm = 0;
  screen_dirty = 1;
  if (!yes)
    return;
  char out[512];
  int found = 0;
  for (int i = 1; i <= 9999; i++) {
    snprintf(out, sizeof(out), ROOT "/exports/MIX%04d.WAV", i);
    SceIoStat st;
    if (sceIoGetstat(out, &st) < 0) {
      found = 1;
      break;
    }
  }
  if (!found) {
    message("Export folder has 9999 mixes.");
    return;
  }
  message("Exporting full mix...");
  view.progress = 0;
  show();
  uint64_t render_started = sceKernelGetProcessTimeWide();
  int rc = render_mix(paths[0], paths[1], out, &view.settings, 0, progress);
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING export_render_ms=%llu result=%d",
           (unsigned long long)(sceKernelGetProcessTimeWide() - render_started) /
               1000,
           rc);
  logline(timing);
  view.progress = -1;
  if (rc)
    message(render_error);
  else {
    char s[180];
    snprintf(s, sizeof(s), "Saved exports/%s", strrchr(out, '/') + 1);
    message(s);
  }
}
static void browse_presets(void) {
  view.preset_browser = 1;
  view.selected_preset = view.settings.preset_id ? view.settings.preset_id : 65;
  int redraw = 1;
  while (1) {
    if (redraw) {
      show();
      redraw = 0;
    }
    unsigned h = buttons();
    int tapped = touch_press();
    if (h || tapped)
      redraw = 1;
    int id = view.selected_preset, bank = (id - 1) / 16, local = (id - 1) % 16;
    int sample_setting_changed = 0;
    if (tapped) {
      if (touch_y >= 115 && touch_y < 172 && touch_x >= 34 && touch_x < 934) {
        int row = (touch_y - 115) / 29;
        int b = row * 4 + (touch_x - 34) / 225;
        if (b < BANK_COUNT) {
          if (b == 5 && local > 8)
            local = 8;
          view.selected_preset = b * 16 + local + 1;
        }
      } else if (touch_y >= 180 && touch_y < 430 && touch_x >= 34 &&
                 touch_x < 526) {
        int i = (touch_y - 180) / 31;
        view.selected_preset = bank * 16 + (local / 8) * 8 + i + 1;
      } else if (bank >= 4 && touch_y >= 337 && touch_y < 369 &&
                 touch_x >= 555 && touch_x < 910) {
        h |= SCE_CTRL_SQUARE;
      } else if (bank >= 4 && touch_y >= 374 && touch_y < 403 &&
                 touch_x >= 555 && touch_x < 910) {
        view.settings.sample_reverse = !view.settings.sample_reverse;
        sample_setting_changed = 1;
      } else if (bank >= 4 && touch_y >= 403 && touch_y < 431 &&
                 touch_x >= 555 && touch_x < 910) {
        view.settings.sample_speed =
            roundf((view.settings.sample_speed + (touch_x < 730 ? -.1f : .1f)) *
                   10.f) /
            10.f;
        view.settings.sample_speed =
            fmaxf(.5f, fminf(2.f, view.settings.sample_speed));
        sample_setting_changed = 1;
      } else if (touch_y >= 446 && touch_y < 485) {
        if (touch_x >= 34 && touch_x < 274)
          h |= SCE_CTRL_RTRIGGER;
        else if (touch_x >= 544 && touch_x < 714)
          h |= SCE_CTRL_CIRCLE;
        else if (touch_x >= 730 && touch_x < 926)
          h |= SCE_CTRL_CROSS;
      }
    }
    if (h & SCE_CTRL_CIRCLE)
      break;
    int slots = 16;
    if (h & SCE_CTRL_UP)
      view.selected_preset = bank * 16 + (local + slots - 1) % slots + 1;
    if (h & SCE_CTRL_DOWN)
      view.selected_preset = bank * 16 + (local + 1) % slots + 1;
    if (h & SCE_CTRL_LEFT) {
      int target = (bank + BANK_COUNT - 1) % BANK_COUNT;
      view.selected_preset =
          target * 16 + (target == 5 && local > 8 ? 8 : local) + 1;
    }
    if (h & SCE_CTRL_RIGHT) {
      int target = (bank + 1) % BANK_COUNT;
      view.selected_preset =
          target * 16 + (target == 5 && local > 8 ? 8 : local) + 1;
    }
    if (h & (SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER))
      view.selected_preset = bank * 16 + (local + 8) % 16 + 1;
    if (sample_setting_changed)
      message("Reverse and speed apply to WAV audition, preview and export.");
    if ((h & SCE_CTRL_SQUARE) && bank >= 4) {
      char path[64];
      TransitionPreset selected;
      if (preset_get(view.selected_preset, &selected)) {
        message("This WAV preset is invalid.");
        continue;
      }
      snprintf(path, sizeof(path), "app0:sfx/%02d.wav", selected.sample_id);
      if (!selected.sample_id || play_file(path, 1))
        message("Cannot play bundled WAV. Reinstall Beta 1.7.");
      else
        message("WAV audition finished. Settings apply to audition and mix.");
    }
    if (h & SCE_CTRL_CROSS) {
      TransitionPreset p;
      preset_get(view.selected_preset, &p);
      double duration = p.beats * 60.0 / view.settings.bpm;
      double available = 120;
      if (view.songs[0])
        available = fmin(available, view.lengths[0] - view.settings.start_a);
      if (view.songs[1])
        available = fmin(available, view.lengths[1] - view.settings.start_b);
      if (available < .1) {
        message("Move cues earlier before applying a preset.");
        break;
      }
      view.settings.preset_id = view.selected_preset;
      view.settings.effects = p.effects;
      if (p.reset_sample_settings) {
        view.settings.sample_reverse = p.default_reverse;
        view.settings.sample_speed =
            p.default_speed > 0 ? p.default_speed : 1.f;
      }
      view.settings.duration = floor(fmin(duration, available) * 1000) / 1000;
      invalidate();
      char status[180];
      snprintf(status, sizeof(status), "%s / %s%s", preset_banks[p.bank],
               p.name, available < duration ? " (duration shortened)" : "");
      message(status);
      break;
    }
    sceKernelDelayThread(4000);
  }
  view.preset_browser = 0;
  screen_dirty = 1;
}
static void edit_time(int row) {
  double extra = row == 13 ? view.settings.bpm : view.settings.intensity * 100;
  double *value = row >= 13  ? &extra
                  : row == 2 ? &view.settings.start_a
                  : row == 3 ? &view.settings.start_b
                             : &view.settings.duration;
  view.editing = row >= 13 ? row - 9 : row - 1;
  snprintf(view.entry, sizeof(view.entry), "%.3f", *value);
  view.edit_error[0] = 0;
  int fresh = 1;
  int redraw = 1;
  while (view.editing) {
    if (redraw) {
      show();
      redraw = 0;
    }
    unsigned h = buttons();
    int key = -1;
    int tapped = touch_press();
    if (tapped)
      key = ui_key_test(touch_x, touch_y);
    if (h || tapped)
      redraw = 1;
    if ((h & SCE_CTRL_CIRCLE) || key == 12)
      break;
    if (key >= 0 && key < 12) {
      size_t n = strlen(view.entry);
      view.edit_error[0] = 0;
      if (key == 11) {
        if (n)
          view.entry[n - 1] = 0;
        fresh = 0;
      } else {
        char c = key == 9 ? '.' : key == 10 ? '0' : (char)('1' + key);
        if (fresh) {
          view.entry[0] = 0;
          n = 0;
          fresh = 0;
        }
        char *dot = strchr(view.entry, '.');
        if (n < 12 && !(c == '.' && dot) && !(dot && strlen(dot) > 3)) {
          view.entry[n] = c;
          view.entry[n + 1] = 0;
        }
      }
    }
    if ((h & SCE_CTRL_CROSS) || key == 13) {
      char *end;
      double number = strtod(view.entry, &end);
      double max = row == 13   ? 200
                   : row == 14 ? 100
                   : row == 4  ? 120
                               : view.lengths[row - 2],
             min = row == 13  ? 60
                   : row == 4 ? .1
                              : 0;
      if (!view.entry[0] || *end || !isfinite(number) || number < min ||
          number > max) {
        snprintf(view.edit_error, sizeof(view.edit_error),
                 "Enter %.3f to %.3f %s", min, max,
                 row == 13   ? "BPM"
                 : row == 14 ? "percent"
                             : "seconds");
        continue;
      }
      *value = round(number * 1000) / 1000;
      if (row == 13)
        view.settings.bpm = extra;
      else if (row == 14)
        view.settings.intensity = extra / 100;
      invalidate();
      message(row >= 13 ? "Effect settings updated. Preview to hear the change."
                        : "Cue updated. Preview to hear your transition.");
      break;
    }
    sceKernelDelayThread(4000);
  }
  view.editing = 0;
  screen_dirty = 1;
}
int main(void) {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(ROOT, 0777);
  sceIoMkdir(ROOT "/music", 0777);
  sceIoMkdir(ROOT "/exports", 0777);
  sceIoMkdir(ROOT "/cache", 0777);
  prune_cache();
  remove(PREVIEW);
  FILE *log = fopen(ROOT "/session.log", "w");
  if (log) {
    fprintf(log, "Circuit FX Vita Beta 1.7 / 01.07\n");
    fclose(log);
  }
  for (int i = 0; i < 2; i++) {
    blocks[i] = sceKernelAllocMemBlock("CircuitFX_FB",
                                       SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                       2 * 1024 * 1024, NULL);
    if (blocks[i] < 0 ||
        sceKernelGetMemBlockBase(blocks[i], (void **)&buffers[i]) < 0) {
      logline("Framebuffer allocation failed");
      sceKernelExitProcess(1);
      return 1;
    }
  }
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
  sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,
                           SCE_TOUCH_SAMPLING_STATE_START);
  view.progress = -1;
  view.settings = (MixSettings){.start_a = 0,
                                .start_b = 0,
                                .duration = 15,
                                .effects = FX_CROSSFADE,
                                .sample_speed = 1.f,
                                .sample_reverse = 0};
  view.settings.bpm = 128;
  view.settings.intensity = 1;
  scan();
  message("Choose two songs. WAV or MP3. Triangle: exit safely.");
  while (1) {
    if (timeline_dirty && sceKernelGetProcessTimeWide() >= timeline_due) {
      refresh_timeline();
      screen_dirty = 1;
    }
    unsigned hit = buttons();
    int tap = touch_press() ? ui_hit_test(touch_x, touch_y) : -1;
    if (hit || tap >= 0)
      screen_dirty = 1;
    if (tap >= 0) {
      view.row = tap;
      hit |= SCE_CTRL_CROSS;
    }
    if (hit & SCE_CTRL_TRIANGLE)
      break;
    if (hit & SCE_CTRL_UP)
      view.row = (view.row + CONTROL_COUNT - 1) % CONTROL_COUNT;
    if (hit & SCE_CTRL_DOWN)
      view.row = (view.row + 1) % CONTROL_COUNT;
    if (hit & SCE_CTRL_SELECT) {
      scan();
      message("Song list refreshed.");
    }
    if (view.row >= 2 && view.row <= 4 &&
        (hit & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT))) {
      double step = (held & SCE_CTRL_LTRIGGER)   ? .001
                    : (held & SCE_CTRL_RTRIGGER) ? 10
                                                 : .1;
      double *value = view.row == 2   ? &view.settings.start_a
                      : view.row == 3 ? &view.settings.start_b
                                      : &view.settings.duration;
      *value += (hit & SCE_CTRL_LEFT) ? -step : step;
      *value = round(*value * 1000) / 1000;
      double max = view.row == 4 ? 120 : view.lengths[view.row - 2];
      *value = fmax(view.row == 4 ? .1 : 0, fmin(max, *value));
      invalidate();
      message("Settings changed. Preview to hear the result.");
    }
    if (hit & SCE_CTRL_CROSS) {
      if (view.row < 2)
        browse(view.row);
      else if (view.row >= 2 && view.row <= 4)
        edit_time(view.row);
      else if (view.row >= 5 && view.row <= 9) {
        view.settings.effects ^= 1u << (view.row - 5);
        invalidate();
      } else if (view.row == 10)
        preview();
      else if (view.row == 11)
        export_mix();
      else if (view.row == 12)
        browse_presets();
      else if (view.row == 13 || view.row == 14)
        edit_time(view.row);
    }
    if (hit & SCE_CTRL_START) { /* Clear the trigger edge before synchronous
                                   playback. */
      preview();
    }
    if (hit & SCE_CTRL_SQUARE) {
      if (ready) {
        if (play())
          message("Preview playback failed.");
      } else
        message("Generate a preview first.");
    }
    if (screen_dirty)
      show();
    sceKernelDelayThread(2000);
  }
  remove(PREVIEW);
  char summary[160];
  snprintf(summary, sizeof(summary),
           "TIMING ui_draw_calls=%llu ui_draw_avg_us=%llu ui_draw_max_us=%llu",
           (unsigned long long)draw_count,
           (unsigned long long)(draw_count ? draw_total_us / draw_count
                                           : 0),
           (unsigned long long)draw_max_us);
  logline(summary);
  prune_cache();
  logline("Clean exit");
  sceKernelExitProcess(0);
  return 0;
}
