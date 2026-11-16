#include "ui.h"
#include "mp3_import.h"
#include "beatmatch.h"
#include "automix.h"
#include "vocal.h"
#include <math.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <psp2/touch.h>
#include <vita2d.h>
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
static char originals[2][512], selected_names[2][256];
static char cache_paths[2][512];
static char matched_path[512], matched_overlap_path[512];
static char vocal_cache_paths[2][512], render_paths[2][512], overlap_paths[2][512];
static MixSettings render_settings;
static BeatInfo beats[2];
static char active_wave_part[512];
static int count, ready;
static uint32_t previous, held;
static int touch_x, touch_y, touch_down;
static int touches, touch_points[2][2];
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
  if (active_wave_part[0] && !strcmp(active_wave_part, path)) return 1;
  if (matched_path[0] && !strcmp(matched_path,path)) return 1;
  if (matched_overlap_path[0] && !strcmp(matched_overlap_path,path)) return 1;
  for (int i=0;i<2;i++)
    if (vocal_cache_paths[i][0] && !strcmp(vocal_cache_paths[i],path)) return 1;
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
      if (n >= 5 && !strcmp(p + n - 5, ".part") &&
          !cache_protected(p))
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
  touches = t.reportNum > 2 ? 2 : t.reportNum;
  for (int i = 0; i < touches; ++i) {
    touch_points[i][0] = t.report[i].x / 2;
    touch_points[i][1] = t.report[i].y / 2;
  }
  if (t.reportNum) {
    touch_x = t.report[0].x / 2;
    touch_y = t.report[0].y / 2;
  }
  return pressed;
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
  vita2d_start_drawing();
  vita2d_clear_screen();
  ui_draw(NULL, &view);
  vita2d_end_drawing();
  uint64_t render_us = sceKernelGetProcessTimeWide() - started;
  vita2d_swap_buffers();
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
  matched_path[0]=0;
  matched_overlap_path[0]=0;
  vocal_cache_paths[0][0]=vocal_cache_paths[1][0]=0;
  remove(PREVIEW);
  screen_dirty = 1;
}
static int gesture_deck = -1, gesture_cue, gesture_x, gesture_distance;
static void wave_bounds(double *left,double *right) {
  *left=0; *right=0;
  for (int d=0;d<2;d++) if (view.songs[d]) {
    *left=fmin(*left,-view.wave_origin[d]);
    *right=fmax(*right,view.lengths[d]-view.wave_origin[d]);
  }
}
static void clamp_wave(void) {
  double left,right;
  wave_bounds(&left,&right);
  double span=right-left;
  if (span<5) span=5;
  view.wave_window=fmax(5,fmin(view.wave_window,span));
  view.wave_pan=fmax(left,fmin(view.wave_pan,right-view.wave_window));
  screen_dirty=1;
}
static void zoom_wave(double factor) {
  double center=view.wave_pan+view.wave_window*.5;
  view.wave_window*=factor;
  view.wave_pan=center-view.wave_window*.5;
  clamp_wave();
}
static void fit_wave(void) {
  double left,right;
  wave_bounds(&left,&right);
  view.wave_pan=left;
  view.wave_window=fmax(5,right-left);
  screen_dirty=1;
}
static void wave_gesture(int pressed) {
  if (!touches) { gesture_deck = -1; return; }
  int x = touch_points[0][0], y = touch_points[0][1];
  if (pressed && x >= 54 && x < 934 && y >= 80 && y < 309) {
    gesture_deck = y < 194 ? 0 : 1;
    double length = view.lengths[gesture_deck];
    if (length <= 0) { gesture_deck = -1; return; }
    double span = view.wave_window;
    double cue = gesture_deck ? view.settings.start_b : view.settings.start_a;
    double cue_x = 54 + 880 * (cue-view.wave_origin[gesture_deck]-view.wave_pan) / span;
    gesture_cue = fabs(x - cue_x) < 24;
    gesture_x = x;
    gesture_distance = 0;
    if (gesture_cue) invalidate();
    return;
  }
  if (gesture_deck < 0) return;
  int d = gesture_deck;
  double length = view.lengths[d], span = view.wave_window;
  if (touches > 1 && !gesture_cue) {
    int distance = abs(touch_points[1][0] - x);
    if (gesture_distance > 12 && distance > 12) {
      double new_span = span * gesture_distance / distance;
      double middle = (x + touch_points[1][0]) / 2.0;
      double anchor = view.wave_pan + span * (middle - 54) / 880;
      view.wave_pan = anchor - new_span * (middle-54) / 880;
      view.wave_window = new_span;
      clamp_wave();
    }
    gesture_distance = distance;
  } else if (gesture_cue) {
    double value = view.wave_origin[d]+view.wave_pan+span*(x-54)/880;
    double *cue = d ? &view.settings.start_b : &view.settings.start_a;
    value = round(fmax(0, fmin(length, value)) * 1000) / 1000;
    if (value != *cue) { *cue = value; screen_dirty = 1; }
  } else {
    if (x != gesture_x) {
      view.wave_pan -= (x-gesture_x)*span/880;
      clamp_wave();
    }
    gesture_distance = 0;
  }
  gesture_x = x;
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
        (has_suffix(e.d_name, ".wav") || has_suffix(e.d_name, ".mp3")) &&
        !has_suffix(e.d_name,".instrumental.wav") &&
        !has_suffix(e.d_name,".vocals.wav"))
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
#define WAVE_MAGIC 0x57415632u
typedef struct { uint32_t magic, signature, frames, bins; } WaveHeader;
#define BEAT_MAGIC 0x42454131u
typedef struct { uint32_t magic, signature, frames; BeatInfo info; } BeatDisk;
static void beat_cache_name(char *out,size_t size,uint32_t key) {
  snprintf(out,size,ROOT "/cache/beat-%08x-v1.bin",key);
}
static int beat_load(int deck,uint32_t key,uint32_t frames) {
  char path[512];
  beat_cache_name(path,sizeof(path),key);
  FILE *f=fopen(path,"rb");
  if (!f) return -1;
  BeatDisk disk;
  int okay=fread(&disk,1,sizeof(disk),f)==sizeof(disk) &&
           fgetc(f)==EOF && disk.magic==BEAT_MAGIC &&
           disk.signature==key && disk.frames==frames &&
           isfinite(disk.info.bpm) && isfinite(disk.info.phase_seconds) &&
           isfinite(disk.info.confidence);
  fclose(f);
  if (!okay) {remove(path);return -1;}
  beats[deck]=disk.info;
  if (!view.bpm_manual[deck]) view.track_bpm[deck]=disk.info.bpm;
  logline("TIMING beat_cache_hit=1");
  return 0;
}
static void beat_save(uint32_t key,uint32_t frames,BeatInfo info) {
  char path[512],part[512];
  beat_cache_name(path,sizeof(path),key);
  snprintf(part,sizeof(part),ROOT "/cache/beat-%08x-v1.part",key);
  FILE *f=fopen(part,"wb");
  BeatDisk disk={BEAT_MAGIC,key,frames,info};
  int okay=f && fwrite(&disk,1,sizeof(disk),f)==sizeof(disk);
  if (f && fclose(f)) okay=0;
  if (!okay || rename(part,path)) remove(part);
}
static struct {
  atomic_int cancel, done, result;
  SceUID thread;
  char input[512], cache[512], part[512];
  uint32_t signature, frames, offset;
  uint16_t peaks[WAVE_BINS];
  BeatInfo beat;
} wave_job;
static int wave_pending[2], wave_ready[2];
static char wave_inputs[2][512];
static uint32_t wave_keys[2], wave_frames[2], wave_offsets[2];
static int wave_running;
static int wave_cancel(uint32_t done, uint32_t total) {
  (void)done; (void)total;
  return atomic_load(&wave_job.cancel);
}
static int wave_worker(SceSize args, void *argp) {
  (void)args; (void)argp;
  uint64_t started=sceKernelGetProcessTimeWide();
  FILE *f=fopen(wave_job.input,"rb");
  int result=-1;
  memset(wave_job.peaks,0,sizeof(wave_job.peaks));
  if (f && has_suffix(wave_job.input,".mp3")) {
    fclose(f);
    f=NULL;
    result=mp3_waveform_peaks(wave_job.input,wave_job.peaks,WAVE_BINS,
                              wave_cancel);
  } else if (f && !fseek(f,wave_job.offset,SEEK_SET) && wave_job.frames) {
    int16_t pcm[4096*2];
    uint32_t at=0, bin=0;
    uint64_t boundary=(uint64_t)(bin+1)*wave_job.frames/WAVE_BINS;
    if (!boundary) boundary=1;
    result=0;
    while (at < wave_job.frames && !atomic_load(&wave_job.cancel)) {
      uint32_t count=wave_job.frames-at;
      if (count>4096) count=4096;
      if (fread(pcm,4,count,f)!=count) {result=-1;break;}
      for (uint32_t i=0;i<count;i+=8) {
        uint32_t frame=at+i;
        while (frame>=boundary && bin<WAVE_BINS-1) {
          bin++;
          boundary=(uint64_t)(bin+1)*wave_job.frames/WAVE_BINS;
          if (boundary<=frame) boundary=frame+1;
        }
        int a=abs((int)pcm[i*2]), b=abs((int)pcm[i*2+1]);
        uint16_t peak=a>b?a:b;
        if (peak>wave_job.peaks[bin]) wave_job.peaks[bin]=peak;
      }
      at+=count;
    }
    if (atomic_load(&wave_job.cancel)) result=-1;
  }
  if (f) fclose(f);
  if (!result) {
    wave_job.beat=beat_analyze_peaks(wave_job.peaks,WAVE_BINS,
                                     (double)wave_job.frames/44100);
    FILE *out=fopen(wave_job.part,"wb");
    WaveHeader h={WAVE_MAGIC,wave_job.signature,wave_job.frames,WAVE_BINS};
    if (!out || fwrite(&h,1,sizeof(h),out)!=sizeof(h) ||
        fwrite(wave_job.peaks,1,sizeof(wave_job.peaks),out)!=sizeof(wave_job.peaks))
      result=-1;
    if (out && fclose(out)) result=-1;
    if (!result && rename(wave_job.part,wave_job.cache)) result=-1;
    if (result) remove(wave_job.part);
    else beat_save(wave_job.signature,wave_job.frames,wave_job.beat);
  }
  char timing[128];
  snprintf(timing,sizeof(timing),"TIMING waveform_ms=%llu result=%d",
      (unsigned long long)(sceKernelGetProcessTimeWide()-started)/1000,result);
  logline(timing);
  atomic_store(&wave_job.result,result);
  atomic_store(&wave_job.done,1);
  return 0;
}
static void wave_request(int deck, const char *source, uint32_t frames,
                         uint32_t offset) {
  SceIoStat st;
  uint32_t key=track_signature(source,&st);
  wave_pending[deck]=key!=0;
  wave_ready[deck]=0;
  wave_keys[deck]=key;
  wave_frames[deck]=frames;
  wave_offsets[deck]=offset;
  int cached_pcm = has_suffix(source,".mp3") && offset != 0;
  snprintf(wave_inputs[deck],sizeof(wave_inputs[deck]),"%s",
           cached_pcm ? paths[deck] : source);
  memset(view.peaks[deck],0,sizeof(view.peaks[deck]));
  beats[deck]=(BeatInfo){0};
  if (!view.bpm_manual[deck]) view.track_bpm[deck]=0;
  if (key) beat_load(deck,key,frames);
  if (wave_running && !strcmp(wave_job.input,wave_inputs[deck]) &&
      wave_job.signature==key) return;
  if (wave_running && !strcmp(wave_job.input,source) &&
      wave_job.signature!=key)
    atomic_store(&wave_job.cancel,1);
  screen_dirty=1;
}
static void wave_poll(void) {
  if (wave_running && atomic_load(&wave_job.done)) {
    sceKernelWaitThreadEnd(wave_job.thread,NULL,NULL);
    sceKernelDeleteThread(wave_job.thread);
    wave_running=0;
    active_wave_part[0]=0;
    if (!atomic_load(&wave_job.result)) {
      for (int d=0;d<2;d++)
        if (wave_pending[d] && wave_keys[d]==wave_job.signature &&
            !strcmp(wave_inputs[d],wave_job.input)) {
          memcpy(view.peaks[d],wave_job.peaks,sizeof(view.peaks[d]));
          beats[d]=wave_job.beat;
          if (!view.bpm_manual[d]) view.track_bpm[d]=beats[d].bpm;
          wave_pending[d]=0; wave_ready[d]=1; screen_dirty=1;
        }
      cache_touch(wave_job.cache);
      prune_cache();
    } else for (int d=0;d<2;d++)
      if (wave_pending[d] && wave_keys[d]==wave_job.signature &&
          !strcmp(wave_inputs[d],wave_job.input)) wave_pending[d]=0;
  }
  if (wave_running) return;
  for (int d=0;d<2;d++) {
    if (!wave_pending[d]) continue;
    int other=d^1;
    if (wave_ready[other] && wave_keys[d]==wave_keys[other] &&
        !strcmp(wave_inputs[d],wave_inputs[other])) {
      memcpy(view.peaks[d],view.peaks[other],sizeof(view.peaks[d]));
      beats[d]=beats[other];
      if (!view.bpm_manual[d]) view.track_bpm[d]=beats[d].bpm;
      wave_pending[d]=0; wave_ready[d]=1; screen_dirty=1;
      continue;
    }
    char cache[512];
    snprintf(cache,sizeof(cache),ROOT "/cache/wave-%08x-v2.bin",wave_keys[d]);
    FILE *f=fopen(cache,"rb");
    WaveHeader h={0};
    if (f) {
      int ok=fread(&h,1,sizeof(h),f)==sizeof(h) &&
          h.magic==WAVE_MAGIC && h.signature==wave_keys[d] &&
          h.frames==wave_frames[d] && h.bins==WAVE_BINS &&
          fread(view.peaks[d],1,sizeof(view.peaks[d]),f)==sizeof(view.peaks[d]) &&
          fgetc(f)==EOF;
      fclose(f);
      if (ok) {
        wave_pending[d]=0; wave_ready[d]=1; screen_dirty=1;
        if (beat_load(d,wave_keys[d],wave_frames[d])) {
          beats[d]=beat_analyze_peaks(view.peaks[d],WAVE_BINS,
                                      (double)wave_frames[d]/44100);
          if (!view.bpm_manual[d]) view.track_bpm[d]=beats[d].bpm;
          beat_save(wave_keys[d],wave_frames[d],beats[d]);
        }
        cache_touch(cache);
        logline("TIMING waveform_cache_hit=1");
        continue;
      }
      remove(cache);
    }
    snprintf(wave_job.input,sizeof(wave_job.input),"%s",wave_inputs[d]);
    snprintf(wave_job.cache,sizeof(wave_job.cache),"%s",cache);
    snprintf(wave_job.part,sizeof(wave_job.part),ROOT "/cache/wave-%08x-v2.part",wave_keys[d]);
    snprintf(active_wave_part,sizeof(active_wave_part),"%s",wave_job.part);
    wave_job.signature=wave_keys[d];
    wave_job.frames=wave_frames[d];
    wave_job.offset=wave_offsets[d];
    atomic_store(&wave_job.cancel,0);
    atomic_store(&wave_job.done,0);
    atomic_store(&wave_job.result,-1);
    wave_job.thread=sceKernelCreateThread("CircuitFX Wave",wave_worker,
                                         0x10000110,64*1024,0,0,NULL);
    if (wave_job.thread<0) {wave_pending[d]=0;active_wave_part[0]=0;return;}
    if (sceKernelStartThread(wave_job.thread,0,NULL)<0) {
      sceKernelDeleteThread(wave_job.thread);wave_pending[d]=0;
      active_wave_part[0]=0;return;
    }
    wave_running=1;
    return;
  }
}
static void wave_shutdown(void) {
  if (wave_running) {
    atomic_store(&wave_job.cancel,1);
    sceKernelWaitThreadEnd(wave_job.thread,NULL,NULL);
    sceKernelDeleteThread(wave_job.thread);
    wave_running=0;
    remove(wave_job.part);
    active_wave_part[0]=0;
  }
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
  view.home_screen = 1;
  int rc = atomic_load(&mp3_job.result);
  screen_dirty = 1;
  char timing[128];
  snprintf(timing, sizeof(timing), "TIMING mp3_import_ms=%llu result=%d",
           (unsigned long long)(sceKernelGetProcessTimeWide() - started) / 1000,
           rc);
  logline(timing);
  return rc;
}
static int ensure_pcm(int deck) {
  if (!has_suffix(originals[deck], ".mp3")) return 0;
  uint32_t frames, offset;
  if (!inspect_wav(cache_paths[deck], &frames, &offset)) {
    strcpy(paths[deck], cache_paths[deck]);
    cache_touch(cache_paths[deck]);
    logline("TIMING mp3_cache_hit=1");
    return 0;
  }
  remove(cache_paths[deck]);
  char part[512];
  if (strlen(cache_paths[deck]) + sizeof(".part") > sizeof(part)) return -1;
  strcpy(part, cache_paths[deck]);
  strcat(part, ".part");
  remove(part);
  message("Decoding MP3 for preview. Circle or tap to cancel.");
  if (decode_mp3_async(originals[deck], part)) {
    remove(part);
    message(atomic_load(&mp3_job.too_large) ?
            "Decoded song exceeds the 512 MB cache limit." :
            mp3_import_error[0] ? mp3_import_error : "MP3 decode failed.");
    return -1;
  }
  if (rename(part, cache_paths[deck]) ||
      inspect_wav(cache_paths[deck], &frames, &offset)) {
    remove(part);
    remove(cache_paths[deck]);
    message("Could not keep decoded song. Check storage space.");
    return -1;
  }
  strcpy(paths[deck], cache_paths[deck]);
  cache_touch(cache_paths[deck]);
  prune_cache();
  return 0;
}
static struct {
  atomic_int cancel,done,result;
  atomic_uint processed,total;
  char input[512],output[512];
  double ratio;
} match_job;
static int match_progress(uint32_t at,uint32_t total) {
  atomic_store(&match_job.processed,at);
  atomic_store(&match_job.total,total);
  return atomic_load(&match_job.cancel);
}
static int match_worker(SceSize args,void *argp) {
  (void)args;(void)argp;
  int rc=beat_stretch_wav(match_job.input,match_job.output,
                           match_job.ratio,match_progress);
  atomic_store(&match_job.result,rc);
  atomic_store(&match_job.done,1);
  return 0;
}
static int stretch_async(const char *input,const char *output,double ratio) {
  snprintf(match_job.input,sizeof(match_job.input),"%s",input);
  snprintf(match_job.output,sizeof(match_job.output),"%s",output);
  match_job.ratio=ratio;
  atomic_store(&match_job.cancel,0);
  atomic_store(&match_job.done,0);
  atomic_store(&match_job.result,-1);
  atomic_store(&match_job.processed,0);
  atomic_store(&match_job.total,0);
  SceUID thread=sceKernelCreateThread("CircuitFX Beatmatch",match_worker,
                                      0x10000110,128*1024,0,0,NULL);
  if (thread<0) return -1;
  if (sceKernelStartThread(thread,0,NULL)<0) {
    sceKernelDeleteThread(thread);return -1;
  }
  view.progress=0;
  show();
  int last=-1;
  uint64_t tick=sceKernelGetProcessTimeWide()+500000;
  while (!atomic_load(&match_job.done)) {
    unsigned hit=buttons();
    int tapped=touch_press();
    if ((hit&SCE_CTRL_CIRCLE) ||
        (tapped && touch_x>=192 && touch_x<768 && touch_y>=185 && touch_y<349))
      atomic_store(&match_job.cancel,1);
    unsigned total=atomic_load(&match_job.total);
    unsigned done=atomic_load(&match_job.processed);
    int percent=total?(int)((uint64_t)done*100/total):0;
    if (percent!=last) {view.progress=percent;show();last=percent;}
    uint64_t now=sceKernelGetProcessTimeWide();
    if (now>=tick) {sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);tick=now+500000;}
    sceKernelDelayThread(10000);
  }
  sceKernelWaitThreadEnd(thread,NULL,NULL);
  sceKernelDeleteThread(thread);
  view.progress=-1;
  screen_dirty=1;
  return atomic_load(&match_job.result);
}
static int wait_for_beats(void) {
  for (int d=0;d<2;d++) {
    if (view.track_bpm[d]>0) continue;
    message("Analyzing song beats. Circle to cancel.");
    view.progress=0;
    show();
    while (wave_pending[d] || wave_running) {
      wave_poll();
      if (buttons()&SCE_CTRL_CIRCLE) break;
      int tapped=touch_press();
      if (tapped && touch_x>=192 && touch_x<768 && touch_y>=185 && touch_y<349)
        break;
      if (view.track_bpm[d]>0) break;
      sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
      sceKernelDelayThread(10000);
    }
    view.progress=-1;
    screen_dirty=1;
    if (view.track_bpm[d]<=0) {
      message("No stable beat found. Set song BPM or turn Auto off.");
      return -1;
    }
  }
  return 0;
}
static int prepare_match(const char *input,double ratio,char *selected) {
  snprintf(selected,512,"%s",input);
  if (fabs(ratio-1)<.001) return 0;
  SceIoStat st;
  uint32_t key=track_signature(originals[1],&st);
  if (!key) {message("Track B changed. Select it again.");return -1;}
  uint32_t a=(uint32_t)lround(view.track_bpm[0]*1000);
  uint32_t b=(uint32_t)lround(view.track_bpm[1]*1000);
  key=(key^filename_hash(input))*16777619u;
  key=(key^a)*16777619u;
  key=(key^b)*16777619u;
  char cache[512],part[512];
  snprintf(cache,sizeof(cache),ROOT "/cache/match-%08x-v1.wav",key);
  snprintf(part,sizeof(part),ROOT "/cache/match-%08x-v1.part",key);
  uint32_t frames,offset;
  if (inspect_wav(cache,&frames,&offset)) {
    remove(cache);remove(part);
    uint32_t source_frames,source_offset;
    if (inspect_wav(input,&source_frames,&source_offset)) {
      message(render_error);return -1;
    }
    if ((uint64_t)llround(source_frames/ratio)*4+44>CACHE_LIMIT_BYTES) {
      message("Matched song exceeds the 512 MB cache limit.");return -1;
    }
    uint64_t started=sceKernelGetProcessTimeWide();
    message("Preparing pitch-preserving beatmatch. Circle cancels.");
    int rc=stretch_async(input,part,ratio);
    char timing[120];
    snprintf(timing,sizeof(timing),"TIMING beatmatch_ms=%llu result=%d",
             (unsigned long long)(sceKernelGetProcessTimeWide()-started)/1000,rc);
    logline(timing);
    if (rc || rename(part,cache)) {
      remove(part);
      message(beatmatch_error[0]?beatmatch_error:"Could not cache matched song.");
      return -1;
    }
  } else logline("TIMING beatmatch_cache_hit=1");
  snprintf(selected,512,"%s",cache);
  cache_touch(cache);
  prune_cache();
  return 0;
}
static struct {
  atomic_int cancel,done,result;
  atomic_uint processed,total;
  char input[512],output[512],instrumental[512],vocals[512];
  unsigned strength;
} vocal_job;
static int vocal_progress(uint32_t at,uint32_t total) {
  atomic_store(&vocal_job.processed,at);
  atomic_store(&vocal_job.total,total);
  return atomic_load(&vocal_job.cancel);
}
static int vocal_worker(SceSize args,void *argp) {
  (void)args;(void)argp;
  int rc=vocal_reduce_wav(vocal_job.input,vocal_job.output,
               vocal_job.instrumental,vocal_job.vocals,
               vocal_job.strength,vocal_progress);
  atomic_store(&vocal_job.result,rc);
  atomic_store(&vocal_job.done,1);
  return 0;
}
static int vocal_async(const char *input,const char *output,
                       const char *inst,const char *voc,unsigned strength) {
  vocal_error[0]=0;
  snprintf(vocal_job.input,sizeof(vocal_job.input),"%s",input);
  snprintf(vocal_job.output,sizeof(vocal_job.output),"%s",output);
  snprintf(vocal_job.instrumental,sizeof(vocal_job.instrumental),"%s",inst);
  snprintf(vocal_job.vocals,sizeof(vocal_job.vocals),"%s",voc);
  vocal_job.strength=strength;
  atomic_store(&vocal_job.cancel,0);
  atomic_store(&vocal_job.done,0);
  atomic_store(&vocal_job.result,-1);
  atomic_store(&vocal_job.processed,0);
  atomic_store(&vocal_job.total,0);
  SceUID thread=sceKernelCreateThread("CircuitFX Vocal",vocal_worker,
                                      0x10000110,64*1024,0,0,NULL);
  if (thread<0) return -1;
  if (sceKernelStartThread(thread,0,NULL)<0) {
    sceKernelDeleteThread(thread);return -1;
  }
  view.progress=0;show();
  int last=-1;
  uint64_t tick=sceKernelGetProcessTimeWide()+500000;
  while (!atomic_load(&vocal_job.done)) {
    unsigned hit=buttons();
    int tapped=touch_press();
    if ((hit&SCE_CTRL_CIRCLE) ||
        (tapped && touch_x>=192 && touch_x<768 && touch_y>=185 && touch_y<349))
      atomic_store(&vocal_job.cancel,1);
    unsigned total=atomic_load(&vocal_job.total);
    unsigned done=atomic_load(&vocal_job.processed);
    int percent=total?(int)((uint64_t)done*100/total):0;
    if (percent!=last) {view.progress=percent;show();last=percent;}
    uint64_t now=sceKernelGetProcessTimeWide();
    if (now>=tick) {sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);tick=now+500000;}
    sceKernelDelayThread(10000);
  }
  sceKernelWaitThreadEnd(thread,NULL,NULL);
  sceKernelDeleteThread(thread);
  view.progress=-1;screen_dirty=1;
  return atomic_load(&vocal_job.result);
}
static int prepare_vocal(int deck,const char *input,char *selected) {
  snprintf(selected,512,"%s",input);
  if (!view.vocal_enabled || view.vocal_strength<=0) return 0;
  const char *dot=strrchr(originals[deck],'.');
  if (!dot) {message("Song has no extension for stem matching.");return -1;}
  int prefix=(int)(dot-originals[deck]);
  char inst[512],voc[512];
  if (snprintf(inst,sizeof(inst),"%.*s.instrumental.wav",prefix,originals[deck])>=
      (int)sizeof(inst) ||
      snprintf(voc,sizeof(voc),"%.*s.vocals.wav",prefix,originals[deck])>=
      (int)sizeof(voc)) {message("Stem file path is too long.");return -1;}
  SceIoStat st;
  uint32_t key=track_signature(originals[deck],&st);
  if (!key) {message("Song changed. Select it again.");return -1;}
  uint32_t ikey=track_signature(inst,&st),vkey=track_signature(voc,&st);
  if (!ikey) inst[0]=0;
  if (!vkey || ikey) voc[0]=0;
  key=(key^view.vocal_strength)*16777619u;
  key=(key^ikey)*16777619u;
  key=(key^vkey)*16777619u;
  char cache[512],part[512];
  snprintf(cache,sizeof(cache),ROOT "/cache/vocal-%08x-v1.wav",key);
  snprintf(part,sizeof(part),ROOT "/cache/vocal-%08x-v1.part",key);
  uint32_t frames,offset,source_frames,source_offset;
  if (inspect_wav(input,&source_frames,&source_offset)) {
    message(render_error);return -1;
  }
  if (inspect_wav(cache,&frames,&offset) || frames!=source_frames) {
    remove(cache);remove(part);
    if ((uint64_t)source_frames*4+44>CACHE_LIMIT_BYTES) {
      message("Vocal cache exceeds the 512 MB limit.");return -1;
    }
    uint64_t started=sceKernelGetProcessTimeWide();
    message("Preparing vocal reduction. Circle cancels.");
    int rc=vocal_async(input,part,inst,voc,view.vocal_strength);
    char timing[120];
    snprintf(timing,sizeof(timing),"TIMING vocal_ms=%llu result=%d",
             (unsigned long long)(sceKernelGetProcessTimeWide()-started)/1000,rc);
    logline(timing);
    if (rc || rename(part,cache)) {
      remove(part);
      message(vocal_error[0]?vocal_error:"Could not cache vocal reduction.");
      return -1;
    }
  } else logline("TIMING vocal_cache_hit=1");
  snprintf(selected,512,"%s",cache);
  cache_touch(cache);prune_cache();
  return 0;
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
      char path[512], cache_path[512] = {0};
      uint64_t selection_started = sceKernelGetProcessTimeWide();
      snprintf(path, sizeof(path), ROOT "/music/%s", songs[view.browser_pos]);
      int is_mp3 = has_suffix(path, ".mp3");
      uint32_t frames = 0, offset = 0;
      if (is_mp3) {
        SceIoStat input_stat;
        uint32_t hash = track_signature(path, &input_stat);
        if (!hash) {
          message("Cannot read MP3 file information.");
          break;
        }
        snprintf(cache_path, sizeof(cache_path),
                 ROOT "/cache/mp3-%08x.wav", hash);
        if (inspect_wav(cache_path, &frames, &offset)) {
          uint64_t probe_started = sceKernelGetProcessTimeWide();
          if (mp3_probe(path, &frames)) {
            message(mp3_import_error);
            break;
          }
          char timing[100];
          snprintf(timing, sizeof(timing), "TIMING mp3_probe_ms=%llu",
                   (unsigned long long)(sceKernelGetProcessTimeWide()-probe_started)/1000);
          logline(timing);
          offset = 0;
        } else {
          cache_touch(cache_path);
          logline("TIMING mp3_cache_hit=1");
        }
      } else {
        if (inspect_wav(path, &frames, &offset)) {
          message(render_error);
          break;
        }
      }
      if (is_mp3)
        snprintf(cache_paths[deck], sizeof(cache_paths[deck]), "%s", cache_path);
      else
        cache_paths[deck][0] = 0;
      snprintf(originals[deck], sizeof(originals[deck]), "%s", path);
      snprintf(paths[deck], sizeof(paths[deck]), "%s",
               is_mp3 ? cache_path : path);
      snprintf(selected_names[deck], sizeof(selected_names[deck]), "%s",
               songs[view.browser_pos]);
      view.songs[deck] = selected_names[deck];
      view.lengths[deck] = (double)frames / 44100;
      invalidate();
      if (deck == 0)
        view.settings.start_a =
            fmax(0, view.lengths[0] - view.settings.duration);
      else
        view.settings.start_b = 0;
      view.wave_origin[deck] = deck ? view.settings.start_b : view.settings.start_a;
      view.bpm_manual[deck]=0;
      wave_request(deck, path, frames, offset);
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
         (view.preset_browser ||
          ui_hit_test_for_tab(&view,touch_x,touch_y)==101 ||
          ui_hit_test_for_tab(&view,touch_x,touch_y)==10)))
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
  double ratio=1;
  if (view.auto_beatmatch) {
    /* Wait for peak envelopes, but never refuse to mix an unbeatmatchable
     * pairing. The planner falls back to a safe original-tempo crossfade. */
    (void)wait_for_beats();
    BeatInfo grid[2]={beats[0],beats[1]};
    for (int d=0;d<2;d++) {
      grid[d].bpm=view.track_bpm[d];
      if (view.bpm_manual[d] && grid[d].bpm>0)
        grid[d].phase_seconds=(float)fmod(view.settings.start_a*(1-d)+
                                  view.settings.start_b*d,60./grid[d].bpm);
    }
    AutoMixPlan plan;
    if (automix_plan(view.peaks[0],view.peaks[1],WAVE_BINS,
                     view.lengths[0],view.lengths[1],
                     &grid[0],&grid[1],&plan)) {
      message("Auto Mix requires two songs longer than 8 seconds.");
      return;
    }
    ratio=plan.tempo_ratio;
    view.settings.start_a=plan.mix.start_a;
    view.settings.start_b=plan.mix.start_b;
    view.settings.duration=plan.mix.duration;
    view.settings.effects=plan.mix.effects;
    view.settings.preset_id=0;
    view.settings.bpm=plan.mix.bpm;
    view.settings.intensity=plan.mix.intensity;
    view.wave_origin[0]=plan.mix.start_a;
    view.wave_origin[1]=plan.mix.start_b;
    char log[180];
    snprintf(log,sizeof(log),
             "AUTOMIX mode=%s cueA=%.3f cueB=%.3f overlap=%.3f ratio=%.4f",
             plan.use_beatmatch?"phrase-grid":"safe-crossfade",
             plan.mix.start_a,plan.mix.start_b,plan.mix.duration,ratio);
    logline(log);
  }
  /* Keep the waveform worker off the storage bus while importing PCM. */
  wave_shutdown();
  for (int d=0; d<2; d++)
    if (ensure_pcm(d)) return;
  render_settings=view.settings;
  for (int d=0;d<2;d++) {
    strcpy(render_paths[d],paths[d]);
    overlap_paths[d][0]=0;
  }
  int vocal_active=view.vocal_enabled && view.vocal_strength>0;
  for (int d=0;d<2;d++) {
    int wanted=vocal_active && (view.vocal_target==2 ||
            view.vocal_target==3 || view.vocal_target==d);
    if (!wanted) continue;
    if (prepare_vocal(d,paths[d],vocal_cache_paths[d])) return;
    if (view.vocal_target==2)
      snprintf(overlap_paths[d],sizeof(overlap_paths[d]),"%s",vocal_cache_paths[d]);
    else snprintf(render_paths[d],sizeof(render_paths[d]),"%s",vocal_cache_paths[d]);
  }
  if (prepare_match(render_paths[1],ratio,matched_path)) return;
  snprintf(render_paths[1],sizeof(render_paths[1]),"%s",matched_path);
  if (overlap_paths[1][0]) {
    if (prepare_match(overlap_paths[1],ratio,matched_overlap_path)) return;
    snprintf(overlap_paths[1],sizeof(overlap_paths[1]),"%s",matched_overlap_path);
  }
  if (view.auto_beatmatch) render_settings.start_b/=ratio;
  message("Rendering preview...");
  view.progress = 0;
  show();
  uint64_t render_started = sceKernelGetProcessTimeWide();
  int rc = render_mix_layers(render_paths[0],render_paths[1],
                overlap_paths[0][0]?overlap_paths[0]:NULL,
                overlap_paths[1][0]?overlap_paths[1]:NULL,
                PREVIEW,&render_settings,1,progress);
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
  int rc = render_mix_layers(render_paths[0],render_paths[1],
                overlap_paths[0][0]?overlap_paths[0]:NULL,
                overlap_paths[1][0]?overlap_paths[1]:NULL,
                out,&render_settings,0,progress);
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
  if (view.selected_preset < 1 || view.selected_preset > PRESET_COUNT)
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
        message("Cannot play bundled WAV. Reinstall Beta 1.9.");
      else
        message("WAV audition finished. Settings apply to audition and mix.");
    }
    if (h & SCE_CTRL_CROSS) {
      TransitionPreset p;
      preset_get(view.selected_preset, &p);
      double tempo=view.auto_beatmatch && view.track_bpm[0]>0 ?
                   view.track_bpm[0] : view.settings.bpm;
      double duration = p.beats * 60.0 / tempo;
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
  view.preset_bank = (view.selected_preset-1)/16;
  view.preset_page = ((view.selected_preset-1)%16)/5;
  screen_dirty = 1;
}
static void edit_time(int row) {
  double extra = row == 13 ? view.settings.bpm
               : row == 14 ? view.settings.intensity * 100
               : row >= 15 && view.track_bpm[row-15]>0 ?
                 view.track_bpm[row-15] : 120;
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
      double max = row >= 15   ? 180
                   : row == 13   ? 200
                   : row == 14 ? 100
                   : row == 4  ? 120
                               : view.lengths[row - 2],
             min = row >= 15  ? 70
                   : row == 13  ? 60
                   : row == 4 ? .1
                              : 0;
      if (!view.entry[0] || *end || !isfinite(number) || number < min ||
          number > max) {
        snprintf(view.edit_error, sizeof(view.edit_error),
                 "Enter %.3f to %.3f %s", min, max,
                 row == 13 || row>=15 ? "BPM"
                 : row == 14 ? "percent"
                             : "seconds");
        continue;
      }
      *value = round(number * 1000) / 1000;
      if (row==2 || row==3) view.wave_origin[row-2]=*value;
      if (row == 13)
        view.settings.bpm = extra;
      else if (row == 14)
        view.settings.intensity = extra / 100;
      else if (row>=15) {
        int deck=row-15;
        view.track_bpm[deck]=(float)extra;
        view.bpm_manual[deck]=1;
      }
      invalidate();
      message(row >= 15 ? "Song BPM set manually. Auto uses this tempo."
              : row >= 13 ? "Effect settings updated. Preview to hear the change."
                        : "Cue updated. Preview to hear your transition.");
      break;
    }
    sceKernelDelayThread(4000);
  }
  view.editing = 0;
  screen_dirty = 1;
}
static void card_action(int index) {
  if (index<0 || index>5) return;
  if (view.tab==2) {
    int id=view.preset_bank*16+view.preset_page*5+index+1;
    if (id<=(view.preset_bank+1)*16) {
      view.selected_preset=id;
      screen_dirty=1;
    }
    return;
  }
  view.effect_index=index;
  if (view.tab==0) {
    if (index==5) return;
    view.settings.preset_id=0;
    view.settings.effects=index==4 ? FX_QUICK_CUT :
                           FX_CROSSFADE | (index ? 1u<<index : 0);
    invalidate();
    message("Transition selected. Preview to hear it.");
  } else if (index<5) {
    view.settings.effects^=1u<<index;
    invalidate();
    message("Effect stack updated. Preview to hear it.");
  } else message("Vocal Reduction Lite selected. Use controls below.");
}
static void direct_action(int action) {
  if (action==160 || action==161) edit_time(15+action-160);
  else if (action==162) {
    view.auto_beatmatch=!view.auto_beatmatch;
    invalidate();
    message(view.auto_beatmatch?"Auto beatmatch ON. Preview prepares the mix.":
                                "Auto beatmatch OFF. Original tempos used.");
  } else if (action>=110 && action<=112) {
    view.tab=action-110;
    screen_dirty=1;
  } else if (action>=120 && action<=125) card_action(action-120);
  else if (action>=130 && action<138) {
    view.preset_bank=action-130;
    view.preset_page=0;
    view.selected_preset=view.preset_bank*16+1;
    screen_dirty=1;
  } else if (action==140 || action==141) {
    view.preset_page=(view.preset_page+(action==141?1:3))%4;
    screen_dirty=1;
  } else if (action==142) browse_presets();
  else if (action==150) {
    if (view.tab==1 && view.effect_index==5)
      view.vocal_enabled=!view.vocal_enabled;
    else view.settings.effects^=1u<<view.effect_index;
    invalidate();
  } else if (action==151 || action==152) {
    if (view.tab==1 && view.effect_index==5)
      view.vocal_strength=(int)fmaxf(0,fminf(100,view.vocal_strength+
                                          (action==151?-10:10)));
    else {
      float delta=action==151?-.05f:.05f;
      view.settings.intensity=fmaxf(0,fminf(1,
                        roundf((view.settings.intensity+delta)*20)/20));
    }
    invalidate();
  } else if (action==163) {
    view.vocal_target=(view.vocal_target+1)%4;
    invalidate();
  } else if (action==153 || action==154) {
    view.settings.bpm=fmaxf(60,fminf(200,
                       view.settings.bpm+(action==153?-1:1)));
    invalidate();
  } else if (action==101) message("Playback is stopped. Preview or Replay.");
  else if (action==102) {
    if (ready) {
      if (play()) message("Preview playback failed.");
    } else message("Generate a preview first.");
  } else if (action==103) zoom_wave(1.5);
  else if (action==104) zoom_wave(1/1.5);
  else if (action==105) fit_wave();
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
    fprintf(log, "Circuit FX Vita Beta 2.2 / 02.02\n");
    fclose(log);
  }
  if (vita2d_init() < 0 || ui_gpu_init() < 0) {
    logline("GPU initialization failed");
    sceKernelExitProcess(1);
    return 1;
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
  view.auto_beatmatch=1;
  view.vocal_strength=70;
  view.vocal_target=2; /* transition only */
  view.wave_window=45;
  view.wave_pan=-20;
  view.selected_preset=65;
  scan();
  message("Choose two songs. WAV or MP3. Triangle: exit safely.");
  while (1) {
    wave_poll();
    unsigned hit = buttons();
    int touched = touch_press();
    wave_gesture(touched);
    int tap = touched && gesture_deck < 0 ?
              ui_hit_test_for_tab(&view,touch_x,touch_y) : -1;
    if (hit || tap >= 0)
      screen_dirty = 1;
    if (tap==300 || tap==301) {
      view.workflow=(tap==300)?1:0;
      view.home_screen=0;
      scan();
      browse(0);
      if (view.songs[0]) {
        if (view.workflow==1) {
          view.vocal_enabled=1;
          view.vocal_target=0;
          view.tab=1;
          view.effect_index=5;
          message("Vocal Lite selected: center-channel reduction only; no AI stems.");
        } else {
          browse(1);
          if (view.songs[1]) {
            view.auto_beatmatch=1;
            view.tab=0;
            message("Auto Mix ready. Preview plans and renders the transition.");
          } else {
            view.home_screen=1;
          }
        }
      } else {
        view.home_screen=1;
      }
      tap=-1;
      screen_dirty=1;
    }
    if (tap>=100) {
      direct_action(tap);
      tap=-1;
    } else if (tap >= 0) {
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
    if (view.row>4 && (hit&(SCE_CTRL_LTRIGGER|SCE_CTRL_RTRIGGER))) {
      view.tab=(view.tab+(hit&SCE_CTRL_RTRIGGER?1:2))%3;
      screen_dirty=1;
    }
    if (view.tab==2 && view.row>=5 && view.row<=9 &&
        (hit&(SCE_CTRL_LEFT|SCE_CTRL_RIGHT))) {
      view.preset_page=(view.preset_page+(hit&SCE_CTRL_RIGHT?1:3))%4;
      screen_dirty=1;
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
      if (view.row<4) view.wave_origin[view.row-2]=*value;
      invalidate();
      message("Settings changed. Preview to hear the result.");
    }
    if (hit & SCE_CTRL_CROSS) {
      if (view.row < 2)
        browse(view.row);
      else if (view.row >= 2 && view.row <= 4)
        edit_time(view.row);
      else if (view.row >= 5 && view.row <= 9) {
        card_action(view.row-5);
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
  wave_shutdown();
  char summary[160];
  snprintf(summary, sizeof(summary),
           "TIMING ui_draw_calls=%llu ui_draw_avg_us=%llu ui_draw_max_us=%llu",
           (unsigned long long)draw_count,
           (unsigned long long)(draw_count ? draw_total_us / draw_count
                                           : 0),
           (unsigned long long)draw_max_us);
  logline(summary);
  snprintf(summary,sizeof(summary),"TIMING gpu_fallbacks=%u",
           ui_gpu_fallback_count());
  logline(summary);
  prune_cache();
  logline("Clean exit");
  vita2d_wait_rendering_done();
  ui_gpu_fini();
  vita2d_fini();
  sceKernelExitProcess(0);
  return 0;
}
