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
static int progress(uint32_t done, uint32_t total);
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
static void clean_mp3_cache(void) {
  int d = sceIoDopen(ROOT "/cache");
  if (d < 0)
    return;
  SceIoDirent entry;
  memset(&entry, 0, sizeof(entry));
  while (sceIoDread(d, &entry) > 0) {
    if (strcmp(entry.d_name, ".") && strcmp(entry.d_name, "..") &&
        !SCE_S_ISDIR(entry.d_stat.st_mode)) {
      char path[512];
      snprintf(path, sizeof(path), ROOT "/cache/%s", entry.d_name);
      remove(path);
    }
    memset(&entry, 0, sizeof(entry));
  }
  sceIoDclose(d);
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
  memset(view.timeline, 0, sizeof(view.timeline));
  double pre = ui_timeline_pre(&view), span = ui_timeline_span(&view);
  for (int d = 0; d < 2; d++) {
    if (!view.songs[d])
      continue;
    uint32_t frames, offset;
    if (inspect_wav(paths[d], &frames, &offset))
      continue;
    FILE *f = fopen(paths[d], "rb");
    if (!f)
      continue;
    int16_t pcm[128];
    for (int x = 0; x < TIMELINE_BINS; x++) {
      double rel = span * x / TIMELINE_BINS - pre;
      if ((d == 0 && rel >= view.settings.duration) || (d == 1 && rel < 0))
        continue;
      double t = (d ? view.settings.start_b : view.settings.start_a) + rel;
      if (t < 0 || t >= view.lengths[d])
        continue;
      uint32_t at = (uint32_t)(t * 44100), n = frames - at;
      if (n > 64)
        n = 64;
      if (fseek(f, offset + (long)at * 4, SEEK_SET))
        break;
      size_t got = fread(pcm, 2, n * 2, f);
      int peak = 0;
      for (size_t i = 0; i < got; i++) {
        int a = abs((int)pcm[i]);
        if (a > peak)
          peak = a;
      }
      view.timeline[d][x] = peak;
    }
    fclose(f);
  }
  timeline_dirty = 0;
}

static void logline(const char *s) {
  FILE *f = fopen(ROOT "/session.log", "a");
  if (f) {
    fprintf(f, "%s\n", s);
    fclose(f);
  }
}
static void show(void) {
  ui_draw(buffers[back], &view);
  SceDisplayFrameBuf frame = {sizeof(frame), buffers[back],
                              960,           SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
                              960,           544};
  sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
  sceDisplayWaitVblankStart();
  back ^= 1;
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
}
static void invalidate(void) {
  ready = view.ready = 0;
  timeline_dirty = 1;
  timeline_due = sceKernelGetProcessTimeWide() + 200000;
  remove(PREVIEW);
}
static int compare(const void *a, const void *b) { return strcasecmp(a, b); }
static void scan(void) {
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
}
static void load_peaks(int deck, uint32_t frames, uint32_t offset) {
  FILE *f = fopen(paths[deck], "rb");
  memset(view.peaks[deck], 0, sizeof(view.peaks[deck]));
  if (!f)
    return;
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
}
static void browse(int deck) {
  view.browser = 1;
  view.browser_pos = 0;
  view.browser_count = count;
  while (view.browser) {
    view.first = view.browser_pos / 10 * 10;
    for (int i = 0; i < 10; i++)
      view.entries[i] = view.first + i < count ? songs[view.first + i] : NULL;
    show();
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
      snprintf(path, sizeof(path), ROOT "/music/%s", songs[view.browser_pos]);
      int is_mp3 = has_suffix(path, ".mp3");
      int use_cached_mp3 = 0;
      if (is_mp3) {
        unsigned deck_id = (unsigned)deck + 1;
        uint32_t hash = filename_hash(path);
        snprintf(cache_path, sizeof(cache_path),
                 ROOT "/cache/track%u-%08x.wav", deck_id, hash);
        snprintf(part_path, sizeof(part_path),
                 ROOT "/cache/track%u-%08x.part", deck_id, hash);
        SceIoStat cached;
        use_cached_mp3 = !strcmp(cache_paths[deck], cache_path) &&
                         sceIoGetstat(cache_path, &cached) >= 0;
        if (!use_cached_mp3) {
          remove(part_path);
          message("Decoding MP3 to a temporary WAV...");
          view.progress = 0;
          show();
          int decode_result = mp3_import_to_wav(path, part_path, progress);
          view.progress = -1;
          if (decode_result) {
            remove(part_path);
            message(mp3_import_error);
            break;
          }
          remove(cache_path);
          if (rename(part_path, cache_path)) {
            remove(part_path);
            message("Could not keep the decoded song. Check storage space.");
            break;
          }
        }
        snprintf(source_path, sizeof(source_path), "%s", cache_path);
      } else {
        snprintf(source_path, sizeof(source_path), "%s", path);
      }
      uint32_t frames, offset;
      if (inspect_wav(source_path, &frames, &offset)) {
        if (is_mp3 && !use_cached_mp3)
          remove(cache_path);
        message(render_error);
        break;
      }
      if (cache_paths[deck][0] &&
          (!is_mp3 || strcmp(cache_paths[deck], cache_path)))
        remove(cache_paths[deck]);
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
      message("Song loaded. Set cue times and preview.");
      break;
    }
  }
  view.browser = 0;
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
  int rc = render_mix(paths[0], paths[1], PREVIEW, &view.settings, 1, progress);
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
  while (1) {
    show();
    unsigned h = buttons();
    if (touch_press() && touch_y >= 312 && touch_y < 354) {
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
  }
  view.confirm = 0;
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
  int rc = render_mix(paths[0], paths[1], out, &view.settings, 0, progress);
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
  while (1) {
    show();
    unsigned h = buttons();
    int id = view.selected_preset, bank = (id - 1) / 16, local = (id - 1) % 16;
    int sample_setting_changed = 0;
    if (touch_press()) {
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
        message("Cannot play bundled WAV. Reinstall Beta 1.6.");
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
  }
  view.preset_browser = 0;
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
  while (view.editing) {
    show();
    unsigned h = buttons();
    int key = -1;
    if (touch_press())
      key = ui_key_test(touch_x, touch_y);
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
  }
  view.editing = 0;
}
int main(void) {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(ROOT, 0777);
  sceIoMkdir(ROOT "/music", 0777);
  sceIoMkdir(ROOT "/exports", 0777);
  sceIoMkdir(ROOT "/cache", 0777);
  clean_mp3_cache();
  remove(PREVIEW);
  FILE *log = fopen(ROOT "/session.log", "w");
  if (log) {
    fprintf(log, "Circuit FX Vita Beta 1.6 / 01.06\n");
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
    if (timeline_dirty && sceKernelGetProcessTimeWide() >= timeline_due)
      refresh_timeline();
    show();
    unsigned hit = buttons();
    int tap = touch_press() ? ui_hit_test(touch_x, touch_y) : -1;
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
  }
  remove(PREVIEW);
  for (int i = 0; i < 2; i++)
    if (cache_paths[i][0])
      remove(cache_paths[i]);
  logline("Clean exit");
  sceKernelExitProcess(0);
  return 0;
}
