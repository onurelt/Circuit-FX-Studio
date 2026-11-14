#include "ui.h"
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
static int count, ready, back;
static uint32_t previous, held;
static uint32_t *buffers[2];
static SceUID blocks[2];
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
  ready = 0;
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
    size_t n = strlen(e.d_name);
    if (!SCE_S_ISDIR(e.d_stat.st_mode) && n > 4 &&
        !strcasecmp(e.d_name + n - 4, ".wav") && count < MAX_SONGS)
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
    if (hit & SCE_CTRL_CIRCLE)
      break;
    if (!count)
      continue;
    if (hit & SCE_CTRL_UP)
      view.browser_pos = (view.browser_pos + count - 1) % count;
    if (hit & SCE_CTRL_DOWN)
      view.browser_pos = (view.browser_pos + 1) % count;
    if (hit & SCE_CTRL_CROSS) {
      char path[512];
      snprintf(path, sizeof(path), ROOT "/music/%s", songs[view.browser_pos]);
      uint32_t frames, offset;
      if (inspect_wav(path, &frames, &offset)) {
        message(render_error);
        break;
      }
      snprintf(paths[deck], sizeof(paths[deck]), "%s", path);
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
static int play(void) {
  FILE *f = fopen(PREVIEW, "rb");
  if (!f)
    return -1;
  if (fseek(f, 44, SEEK_SET)) {
    fclose(f);
    return -1;
  }
  int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 44100,
                                 SCE_AUDIO_OUT_MODE_STEREO);
  if (port < 0) {
    fclose(f);
    return -1;
  }
  int volumes[2] = {SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB};
  int rc = sceAudioOutSetVolume(
      port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, volumes);
  static int16_t audio_buffers[2][2048] __attribute__((aligned(64)));
  int audio_index = 0;
  view.playing = 1;
  message("Playing preview. Circle / START stops.");
  show();
  while (rc >= 0) {
    int16_t *pcm = audio_buffers[audio_index];
    memset(pcm, 0, 4096);
    size_t n = fread(pcm, 4, 1024, f);
    if (!n) {
      if (ferror(f))
        rc = -1;
      break;
    }
    if (sceAudioOutOutput(port, pcm) < 0) {
      rc = -1;
      break;
    }
    unsigned h = buttons();
    if (h & (SCE_CTRL_CIRCLE | SCE_CTRL_START))
      break;
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    audio_index ^= 1;
  }
  /* Drain the last buffer before releasing this ordinary (non-background) port.
   */
  sceAudioOutOutput(port, NULL);
  sceAudioOutReleasePort(port);
  fclose(f);
  view.playing = 0;
  return rc < 0 ? -1 : 0;
}
static void preview(void) {
  if (!view.songs[0] || !view.songs[1]) {
    message("Choose both songs first.");
    return;
  }
  invalidate();
  message("Rendering preview...");
  view.progress = 0;
  show();
  int rc = render_mix(paths[0], paths[1], PREVIEW, &view.settings, 1, progress);
  view.progress = -1;
  if (rc) {
    message(render_error);
    return;
  }
  ready = 1;
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
static int touch_row(void) {
  static int down;
  SceTouchData t = {0};
  sceTouchPeek(SCE_TOUCH_PORT_FRONT, &t, 1);
  int row = -1;
  if (t.reportNum && !down) {
    int x = t.report[0].x / 2, y = t.report[0].y / 2;
    if (y >= 53 && y < 158)
      row = x < 480 ? 0 : 1;
    else if (y >= 236 && y < 287)
      row = x < 326 ? 2 : x < 640 ? 3 : 4;
    else if (x < 480 && y >= 326 && y < 476)
      row = 5 + (y - 326) / 30;
    else if (x >= 488 && y >= 326 && y < 380)
      row = 10;
    else if (x >= 488 && y >= 389 && y < 443)
      row = 11;
  }
  down = t.reportNum > 0;
  return row;
}
int main(void) {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(ROOT, 0777);
  sceIoMkdir(ROOT "/music", 0777);
  sceIoMkdir(ROOT "/exports", 0777);
  remove(PREVIEW);
  FILE *log = fopen(ROOT "/session.log", "w");
  if (log) {
    fprintf(log, "Circuit FX Vita Beta 1 / 01.01\n");
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
  view.settings = (MixSettings){0, 0, 15, FX_CROSSFADE};
  scan();
  message("Choose two WAV songs. Triangle: exit safely.");
  while (1) {
    show();
    unsigned hit = buttons();
    int tap = touch_row();
    if (tap >= 0) {
      view.row = tap;
      if (tap < 2 || tap >= 5)
        hit |= SCE_CTRL_CROSS;
    }
    if (hit & SCE_CTRL_TRIANGLE)
      break;
    if (hit & SCE_CTRL_UP)
      view.row = (view.row + 11) % 12;
    if (hit & SCE_CTRL_DOWN)
      view.row = (view.row + 1) % 12;
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
      else if (view.row >= 5 && view.row <= 9) {
        view.settings.effects ^= 1u << (view.row - 5);
        invalidate();
      } else if (view.row == 10)
        preview();
      else if (view.row == 11)
        export_mix();
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
  logline("Clean exit");
  sceKernelExitProcess(0);
  return 0;
}
