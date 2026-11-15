#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifndef HOST_UI
#include <vita2d.h>
#define UI_VERTEX_Z 0.5f
static unsigned ui_gpu_fallbacks;
static vita2d_color_vertex *ui_vertex_batch(size_t count) {
  /* Vita2D's own custom-vertex example uses aligned GPU pool memory. */
  vita2d_color_vertex *vertices =
      vita2d_pool_memalign(count * sizeof(*vertices), sizeof(*vertices));
  if (!vertices) ui_gpu_fallbacks++;
  return vertices;
}
unsigned ui_gpu_fallback_count(void) { return ui_gpu_fallbacks; }
#endif
typedef struct {
  int w, h;
  const unsigned char *widths, *glyphs;
} Font;
#include "font.h"
#define RGB(r, g, b)                                                           \
  ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | 0xff000000u)
#define BG RGB(8, 12, 21)
#define WHITE RGB(235, 242, 252)
#define MUTED RGB(133, 151, 177)
#define CYAN RGB(26, 222, 235)
#define PINK RGB(235, 84, 226)
#define EDGE RGB(45, 58, 79)
static uint32_t *fb;
static uint32_t blend(uint32_t a, uint32_t b, int t) {
  uint32_t c = 0xff000000u;
  for (int k = 0; k < 24; k += 8)
    c |= ((((a >> k) & 255) * (255 - t) + ((b >> k) & 255) * t) / 255) << k;
  return c;
}
static void rect(int x, int y, int w, int h, uint32_t c) {
#ifdef HOST_UI
  for (int j = y; j < y + h && j < 544; j++)
    for (int i = x; i < x + w && i < 960; i++)
      if (i >= 0 && j >= 0)
        fb[j * 960 + i] = c;
#else
  if (w > 0 && h > 0)
    vita2d_draw_rectangle(x, y, w, h, c);
#endif
}
static void roundbox(int x, int y, int w, int h, int r, uint32_t top,
                     uint32_t bottom) {
#ifdef HOST_UI
  for (int j = 0; j < h; j++) {
    int cut = 0;
    if (j < r)
      cut = r - (int)sqrtf(r * r - (r - j - 1) * (r - j - 1));
    else if (j >= h - r)
      cut = r - (int)sqrtf(r * r - (j - (h - r)) * (j - (h - r)));
    rect(x + cut, y + j, w - 2 * cut, 1,
         blend(top, bottom, j * 255 / (h > 1 ? h - 1 : 1)));
  }
#else
  if (w <= 0 || h <= 0)
    return;
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  /* One GPU batch for the gradient body, four small rounded corners. */
  vita2d_color_vertex *v=ui_vertex_batch(18);
  if (!v) {
    vita2d_draw_rectangle(x+r,y,w-2*r,h,top);
    vita2d_draw_rectangle(x,y+r,w,h-2*r,bottom);
  }
  int edges[3][4] = {{x+r,y,w-2*r,h}, {x,y+r,r,h-2*r},
                     {x+w-r,y+r,r,h-2*r}};
  int count=0;
  for (int k=0;v && k<3;k++) {
    int *e=edges[k];
    if (e[2]<=0 || e[3]<=0) continue;
    uint32_t a=k ? blend(top,bottom,r*255/h) : top;
    uint32_t b=k ? blend(top,bottom,(h-r)*255/h) : bottom;
    vita2d_color_vertex quad[6] = {
      {e[0],e[1],UI_VERTEX_Z,a},{e[0]+e[2],e[1],UI_VERTEX_Z,a},
      {e[0],e[1]+e[3],UI_VERTEX_Z,b},{e[0]+e[2],e[1],UI_VERTEX_Z,a},
      {e[0]+e[2],e[1]+e[3],UI_VERTEX_Z,b},{e[0],e[1]+e[3],UI_VERTEX_Z,b}};
    memcpy(v+count,quad,sizeof(quad));
    count+=6;
  }
  if (v && count) vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES,v,count);
  if (r) {
    vita2d_draw_fill_circle(x+r,y+r,r,top);
    vita2d_draw_fill_circle(x+w-r,y+r,r,top);
    vita2d_draw_fill_circle(x+r,y+h-r,r,bottom);
    vita2d_draw_fill_circle(x+w-r,y+h-r,r,bottom);
  }
#endif
}
static void panel(int x, int y, int w, int h, int active, uint32_t accent) {
  roundbox(x - 2, y - 2, w + 4, h + 4, 13, blend(BG, accent, active ? 60 : 15),
           BG);
  roundbox(x, y, w, h, 11, active ? accent : EDGE, blend(EDGE, BG, 100));
  roundbox(x + 1, y + 1, w - 2, h - 2, 10, RGB(26, 35, 51), RGB(14, 21, 34));
}
static void txt(int x, int y, const Font *font, uint32_t color, const char *s,
                int limit) {
  int right = x + limit;
#ifndef HOST_UI
  int slot = font == &small ? 0 : font == &body ? 1 : font == &title ? 2 : 3;
  extern vita2d_texture *ui_atlases[4];
#endif
  for (; *s; s++) {
    unsigned c = (unsigned char)*s;
    if (c < 32 || c > 126)
      c = '?';
    c -= 32;
    if (x + font->widths[c] > right)
      break;
#ifdef HOST_UI
    for (int j = 0; j < font->h; j++)
      for (int i = 0; i < font->w; i++) {
        int px = x + i, py = y + j;
        if (px < 0 || px >= 960 || py < 0 || py >= 544)
          continue;
        unsigned a = font->glyphs[c * font->w * font->h + j * font->w + i];
        if (a)
          fb[py * 960 + px] = blend(fb[py * 960 + px], color, a);
      }
#else
    if (ui_atlases[slot])
      vita2d_draw_texture_tint_part(ui_atlases[slot], x, y,
          (c % 16) * font->w, (c / 16) * font->h, font->w, font->h, color);
#endif
    x += font->widths[c];
  }
}
#ifndef HOST_UI
vita2d_texture *ui_atlases[4];
int ui_gpu_init(void) {
  const Font *fonts[4]={&small,&body,&title,&large};
  for (int k=0;k<4;k++) {
    const Font *f=fonts[k];
    vita2d_texture *t=vita2d_create_empty_texture_format(
        16*f->w,6*f->h,SCE_GXM_TEXTURE_FORMAT_P8_ABGR);
    if (!t) { ui_gpu_fini(); return -1; }
    ui_atlases[k]=t;
    unsigned char *data=vita2d_texture_get_datap(t);
    uint32_t *palette=vita2d_texture_get_palette(t);
    unsigned stride=vita2d_texture_get_stride(t);
    for (int a=0;a<256;a++) palette[a]=0x00ffffffu | ((uint32_t)a<<24);
    for (int c=0;c<95;c++)
      for (int y=0;y<f->h;y++)
        memcpy(data+(c/16*f->h+y)*stride+(c%16)*f->w,
            f->glyphs+(c*f->h+y)*f->w,f->w);
    vita2d_texture_set_filters(t,SCE_GXM_TEXTURE_FILTER_POINT,
                                 SCE_GXM_TEXTURE_FILTER_POINT);
  }
  return 0;
}
void ui_gpu_fini(void) {
  for (int k=0;k<4;k++) {
    if (ui_atlases[k]) vita2d_free_texture(ui_atlases[k]);
    ui_atlases[k]=NULL;
  }
}
#endif
static void tc(char *s, size_t n, double t, int precise) {
  int ms = (int)(fmax(0, t) * 1000 + .5);
  if (precise)
    snprintf(s, n, "%02d:%02d.%03d", ms / 60000, ms / 1000 % 60, ms % 1000);
  else
    snprintf(s, n, "%02d:%02d", ms / 60000, ms / 1000 % 60);
}
double ui_timeline_pre(const View *v) {
  return fmin(4, fmax(0, v->settings.start_a));
}
double ui_timeline_span(const View *v) {
  return ui_timeline_pre(v) + v->settings.duration + 4;
}
int ui_hit_test_for_tab(const View *v, int x, int y) {
  if (x<10 || x>=950 || y<0 || y>=544) return -1;
  if (y >= 6 && y < 47) {
    if (v->songs[0] && y>=22 && x>=364 && x<470) return 160;
    if (v->songs[1] && y>=22 && x>=844 && x<950) return 161;
    return x < 470 ? 0 : x >= 490 ? 1 : -1;
  }
  if (y >= 318 && y < 365) {
    if (x < 112) return 10;
    if (x < 178) return 101; /* Stop */
    if (x < 258) return 102; /* Replay */
    if (x < 384) return 2;
    if (x < 510) return 3;
    if (x < 636) return 4;
    if (x < 700) return 103; /* Zoom out */
    if (x < 764) return 104; /* Zoom in */
    if (x < 824) return 105; /* Fit */
    return x < 950 ? 11 : -1;
  }
  if (y >= 371 && y < 410) return x < 322 ? 110 : x < 638 ? 111 : 112;
  if (v->tab == 2) {
    if (y >= 416 && y < 443 && x >= 14 && x < 942)
      return 130 + (x - 14) / 116;
    if (y >= 448 && y < 506 && x >= 14 && x < 946)
      return 120 + (x - 14) / 188;
    if (y >= 507 && y < 544)
      return x < 68 ? 140 : x >= 884 ? 141 : x >= 722 ? 142 :
             !v->auto_beatmatch && x >= 563 && x < 605 ? 153 :
             !v->auto_beatmatch && x >= 665 && x < 707 ? 154 : -1;
  } else if (v->tab == 1) {
    if (y >= 420 && y < 490 && x >= 14 && x < 946)
      return 120 + (x - 14) / 155;
    if (y >= 497 && y < 541)
      return x < 150 ? 150 : x >= 340 && x < 392 ? 151 :
             x >= 466 && x < 518 ? 152 :
             v->effect_index==5 && x>=550 && x<940 ? 163 :
             v->settings.preset_id && !v->auto_beatmatch && x >= 710 && x < 762 ? 153 :
             v->settings.preset_id && !v->auto_beatmatch && x >= 872 && x < 924 ? 154 : -1;
  } else if (y>=507 && y<543 && x>=620 && x<940) return 162;
  else if (y >= 420 && y < 504 && x >= 14 && x < 946)
    return 120 + (x - 14) / 188;
  return -1;
}
int ui_key_test(int x, int y) {
  if (y >= 391 && y < 435) {
    if (x >= 264 && x < 474)
      return 12;
    if (x >= 486 && x < 696)
      return 13;
  }
  if (x < 264 || x >= 696 || y < 191 || y >= 383)
    return -1;
  int c = (x - 264) / 144, r = (y - 191) / 48;
  if ((x - 264) % 144 >= 136 || (y - 191) % 48 >= 42)
    return -1;
  return r * 3 + c;
}
static void dim(void) {
#ifdef HOST_UI
  for (int i = 0; i < 960 * 544; i++)
    fb[i] = blend(fb[i], BG, 190);
#else
  vita2d_draw_rectangle(0,0,960,544,(BG & 0x00ffffffu) | (190u<<24));
#endif
}
static void draw_wave(const View *v, int deck, double pan, double span,
                      int y, uint32_t color) {
  double length = v->lengths[deck];
  if (length <= 0) return;
#ifndef HOST_UI
  vita2d_color_vertex *bars=ui_vertex_batch(880*6);
#endif
  for (int i=0;i<880;i++) {
    double t=v->wave_origin[deck]+pan+span*(i+0.5)/880.0;
    int valid=t>=0 && t<length;
    if (!valid) t=0;
    int bin=(int)(t*(WAVE_BINS-1)/length);
    int h=valid ? v->peaks[deck][bin]*47/32768 : 0;
    if (valid && h<1) h=1;
#ifdef HOST_UI
    if (valid) rect(54+i,y-h,1,h*2+1,color);
#else
    if (bars) {
      vita2d_color_vertex quad[6]={
        {54+i,y-h,UI_VERTEX_Z,valid?color:0},{55+i,y-h,UI_VERTEX_Z,valid?color:0},
        {54+i,y+h+1,UI_VERTEX_Z,valid?color:0},{55+i,y-h,UI_VERTEX_Z,valid?color:0},
        {55+i,y+h+1,UI_VERTEX_Z,valid?color:0},{54+i,y+h+1,UI_VERTEX_Z,valid?color:0}};
      memcpy(bars+i*6,quad,sizeof(quad));
    } else if (valid) vita2d_draw_rectangle(54+i,y-h,1,h*2+1,color);
#endif
  }
#ifndef HOST_UI
  if (bars) vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES,bars,880*6);
#endif
}
#include "dashboard.inc"
  if (v->progress >= 0) {
    dim();
    panel(192, 185, 576, 164, 1, CYAN);
    txt(219, 204, &title, WHITE, "Processing audio", 520);
    snprintf(s, sizeof(s), "%d%%   /   Circle to cancel", v->progress);
    txt(219, 245, &body, MUTED, s, 520);
    roundbox(219, 295, 522, 8, 4, BG, BG);
    roundbox(219, 295, 522 * v->progress / 100, 8, 4, CYAN, PINK);
  }
  if (v->confirm) {
    dim();
    panel(175, 166, 610, 209, 1, PINK);
    txt(201, 185, &title, WHITE, "Export this mix?", 560);
    txt(201, 226, &body, MUTED,
        "A to its cue, transition, then the remainder of B.", 560);
    txt(201, 257, &body, MUTED, "Your original songs stay untouched.", 560);
    roundbox(201, 312, 257, 42, 7, RGB(37, 48, 67), RGB(21, 30, 47));
    roundbox(486, 312, 273, 42, 7, RGB(68, 32, 92), RGB(38, 26, 58));
    txt(227, 321, &body, WHITE, "Circle / Cancel", 225);
    txt(520, 321, &body, PINK, "X / Export WAV", 225);
  }
  if (v->browser) {
    dim();
    panel(66, 44, 828, 456, 1, CYAN);
    txt(88, 58, &title, WHITE, "Your music", 500);
    txt(88, 92, &small, MUTED,
        "ux0:data/CircuitFX/music    /    WAV or MP3 (PCM on preview)", 760);
    for (int i = 0; i < 10; i++)
      if (v->entries[i]) {
        int y = 132 + i * 31;
        roundbox(82, y, 796, 29, 5,
                 v->first + i == v->browser_pos ? RGB(28, 78, 94)
                                                : RGB(23, 31, 46),
                 RGB(17, 26, 41));
        txt(96, y + 3, &body, WHITE, v->entries[i], 763);
      }
    if (!v->browser_count)
      txt(88, 180, &body, WHITE,
          "Copy WAV or MP3 songs into the music folder, then press SELECT.",
          770);
    snprintf(s, sizeof(s),
             "%d songs   /   Touch a song to load   /   X Select   Circle Back",
             v->browser_count);
    txt(88, 464, &small, MUTED, s, 620);
    txt(736, 464, &small, CYAN, "< PREV    NEXT >", 140);
  }
  if (v->preset_browser) {
    dim();
    panel(16, 47, 928, 455, 1, PINK);
    txt(34, 60, &title, WHITE, "Transition effects and DJ styles", 690);
    txt(34, 91, &small, MUTED,
        "WAV FX are real audio. DJ tabs are genre-inspired original recipes.",
        870);
    int selected = v->selected_preset >= 1 && v->selected_preset <= PRESET_COUNT
                       ? v->selected_preset
                       : 1;
    int bank = (selected - 1) / 16, page = ((selected - 1) % 16) / 8;
    const char *shortbanks[] = {"GSP",   "Micky F", "Anne L",   "Erik V",
                                "WAV 1", "WAV 2",   "DJ Paulo", "Honey Dijon"};
    for (int i = 0; i < BANK_COUNT; i++) {
      int tx = 34 + (i % 4) * 225, ty = 115 + (i / 4) * 29;
      roundbox(tx, ty, 216, 26, 5,
               i == bank ? RGB(77, 35, 102) : RGB(27, 37, 54), RGB(20, 28, 44));
      txt(tx + 12, ty + 3, &body, i == bank ? PINK : WHITE, shortbanks[i], 190);
    }
    for (int i = 0; i < 8; i++) {
      int id = bank * 16 + page * 8 + i + 1, y = 180 + i * 31;
      if (id > PRESET_COUNT)
        continue;
      TransitionPreset p;
      preset_get(id, &p);
      roundbox(34, y, 492, 28, 5,
               id == selected ? RGB(23, 77, 93) : RGB(25, 34, 49),
               RGB(18, 27, 41));
      snprintf(s, sizeof(s), "%02d   %s", id, p.name);
      txt(47, y + 3, &body, id == selected ? CYAN : WHITE, s, 460);
    }
    TransitionPreset p;
    preset_get(selected, &p);
    panel(544, 180, 382, 250, 0, EDGE);
    txt(559, 165, &small, PINK, preset_banks[bank], 350);
    txt(559, 191, &title, WHITE, p.name, 350);
    /* Wrap description into bounded lines without splitting UTF-8 (ASCII
     * recipes). */
    char line[46] = {0};
    int pos = 0, y = 230;
    const char *word = p.description;
    while (*word) {
      const char *end = strchr(word, ' ');
      int n = end ? (int)(end - word) : (int)strlen(word);
      if (pos + n + (pos ? 1 : 0) > 43) {
        line[pos] = 0;
        txt(559, y, &small, MUTED, line, 350);
        y += 19;
        pos = 0;
      }
      if (pos)
        line[pos++] = ' ';
      memcpy(line + pos, word, n);
      pos += n;
      word = end ? end + 1 : word + n;
    }
    line[pos] = 0;
    txt(559, y, &small, MUTED, line, 350);
    double shown_bpm=v->auto_beatmatch && v->track_bpm[0]>0 ?
                     v->track_bpm[0] : v->settings.bpm;
    snprintf(s, sizeof(s), "%d beats  /  %.2f sec at %.0f BPM", p.beats,
             p.beats * 60.0 / shown_bpm, shown_bpm);
    txt(559, 310, &body, CYAN, s, 350);
    snprintf(s, sizeof(s), "Echo %.2f beat   Feedback %.0f%%", p.delay_beats,
             p.feedback * 100);
    if (p.sample_id) {
      roundbox(555, 337, 355, 32, 5, RGB(20, 85, 101), RGB(13, 52, 65));
      txt(567, 344, &body, CYAN,
          v->playing ? "Stop WAV / Circle" : "[] Listen WAV (no songs needed)",
          330);
    } else
      txt(559, 345, &small, MUTED, s, 350);
    if (p.sample_id) {
      snprintf(s, sizeof(s), "Reverse: %s  /  touch to toggle",
               v->settings.sample_reverse ? "ON" : "OFF");
      roundbox(555, 374, 355, 29, 5, RGB(31, 39, 58), RGB(20, 28, 44));
      txt(567, 379, &body, v->settings.sample_reverse ? PINK : WHITE, s, 330);
      snprintf(s, sizeof(s), "Speed  -       %.1fx       +",
               v->settings.sample_speed);
      roundbox(555, 406, 355, 22, 5, RGB(31, 39, 58), RGB(20, 28, 44));
      txt(567, 407, &small, CYAN, s, 330);
    } else {
      txt(559, 371, &small, MUTED, "Cues stay where you set them.", 350);
      txt(559, 392, &small, MUTED, "Duration fits the available audio.", 350);
    }
    roundbox(34, 446, 240, 39, 6, RGB(35, 46, 65), RGB(20, 29, 44));
    snprintf(s, sizeof(s), "Page %d / 2   < L / R >", page + 1);
    txt(52, 455, &body, WHITE, s, 219);
    roundbox(544, 446, 170, 39, 6, RGB(35, 46, 65), RGB(20, 29, 44));
    txt(586, 455, &body, WHITE, "Cancel", 140);
    roundbox(730, 446, 196, 39, 6, RGB(16, 116, 137), RGB(11, 64, 84));
    txt(762, 455, &body, WHITE, "Apply preset", 158);
  }
  if (v->editing) {
    dim();
    panel(238, 73, 484, 397, 1, CYAN);
    snprintf(s, sizeof(s), "Set %s",
             v->editing == 1   ? "Track A cue"
             : v->editing == 2 ? "Track B cue"
             : v->editing == 3 ? "transition duration"
             : v->editing == 4 ? "effect BPM"
             : v->editing == 6 ? "Track A BPM"
             : v->editing == 7 ? "Track B BPM"
                               : "FX intensity");
    txt(264, 88, &title, WHITE, s, 430);
    roundbox(264, 128, 432, 51, 6, RGB(9, 17, 29), RGB(12, 23, 37));
    txt(278, 137, &large, CYAN, v->entry, 330);
    txt(617, 152, &small, MUTED,
        v->editing == 4 || v->editing==6 || v->editing==7 ? "BPM"
        : v->editing == 5 ? "percent"
                          : "seconds",
        70);
    const char *keys[] = {"1", "2", "3", "4", "5", "6",
                          "7", "8", "9", ".", "0", "DEL"};
    for (int k = 0; k < 12; k++) {
      int x = 264 + (k % 3) * 144, y = 191 + (k / 3) * 48;
      roundbox(x, y, 136, 42, 6, RGB(38, 49, 69), RGB(23, 33, 50));
      txt(x + 52, y + 6, &title, WHITE, keys[k], 80);
    }
    roundbox(264, 391, 210, 44, 6, RGB(44, 49, 68), RGB(27, 33, 49));
    roundbox(486, 391, 210, 44, 6, RGB(15, 114, 132), RGB(10, 69, 90));
    txt(319, 401, &body, WHITE, "Cancel", 120);
    txt(553, 401, &body, WHITE, "Apply", 120);
    txt(264, 443, &small, RGB(255, 160, 140),
        v->edit_error[0] ? v->edit_error
        : v->editing == 4
            ? "60-200 BPM; effects timing"
        : v->editing==6 || v->editing==7
            ? "70-180 BPM. Set your song's tempo manually."
        : v->editing == 5 ? "0-100%; controls effect strength, not track volume"
                          : "Enter seconds, e.g. 155.250 = 02:35.250",
        430);
  }
}
#if defined(HOST_UI) && !defined(UI_TEST)
int main(int argc, char **argv) {
  uint32_t *p = calloc(960 * 544, 4);
  View v = {0};
  v.progress = -1;
  v.settings = (MixSettings){.start_a = 155,
                             .start_b = 48,
                             .duration = 15,
                             .effects = 3,
                             .bpm = 128,
                             .intensity = 1};
  v.songs[0] = "Neon Horizons.wav";
  v.songs[1] = "Midnight Circuit.wav";
  v.lengths[0] = 360;
  v.lengths[1] = 420;
  v.wave_origin[0]=v.settings.start_a;
  v.wave_origin[1]=v.settings.start_b;
  v.wave_window=45;
  v.wave_pan=-22.5;
  v.settings.bpm = 128;
  v.settings.intensity = 1;
  v.settings.sample_speed = 1.f;
  v.row = 10;
  v.ready = 1;
  snprintf(v.status, sizeof(v.status),
           "Preview ready. Layer effects or export your mix.");
  /* Synthetic fixture only; production waveforms come from audio files. */
  for (int d = 0; d < 2; d++) {
    for (int i = 0; i < WAVE_BINS; i++)
      v.peaks[d][i] = 3000 + 22000 * fabs(sin(i * .17) * sin(i * .013 + d));
  }
  if (argc > 1 && strcmp(argv[1], "editor") == 0) {
    v.editing = 1;
    snprintf(v.entry, sizeof(v.entry), "155.250");
  }
  if (argc > 1 && strcmp(argv[1], "browser") == 0) {
    v.browser = 1;
    v.browser_count = 3;
    v.entries[0] = "Neon Horizons.wav";
    v.entries[1] = "Midnight Circuit.wav";
    v.entries[2] = "Long song name - live set recording.wav";
  }
  if (argc > 1 && strcmp(argv[1], "presets") == 0) {
    v.preset_browser = 1;
    v.selected_preset = 65;
  }
  if (argc > 1 && strcmp(argv[1], "effects-tab") == 0) {
    v.tab = 1;
    v.effect_index = 1;
  }
  if (argc > 1 && strcmp(argv[1], "presets-tab") == 0) {
    v.tab = 2;
    v.preset_bank = 4;
    v.selected_preset = 65;
  }
  ui_draw(p, &v);
  FILE *f = fopen("ui-preview.ppm", "wb");
  fprintf(f, "P6\n960 544\n255\n");
  for (int i = 0; i < 960 * 544; i++) {
    fputc(p[i] & 255, f);
    fputc((p[i] >> 8) & 255, f);
    fputc((p[i] >> 16) & 255, f);
  }
  fclose(f);
  free(p);
  return 0;
}
#endif
