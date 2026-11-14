#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  for (int j = y; j < y + h && j < 544; j++)
    for (int i = x; i < x + w && i < 960; i++)
      if (i >= 0 && j >= 0)
        fb[j * 960 + i] = c;
}
static void roundbox(int x, int y, int w, int h, int r, uint32_t top,
                     uint32_t bottom) {
  for (int j = 0; j < h; j++) {
    int cut = 0;
    if (j < r)
      cut = r - (int)sqrtf(r * r - (r - j - 1) * (r - j - 1));
    else if (j >= h - r)
      cut = r - (int)sqrtf(r * r - (j - (h - r)) * (j - (h - r)));
    rect(x + cut, y + j, w - 2 * cut, 1,
         blend(top, bottom, j * 255 / (h > 1 ? h - 1 : 1)));
  }
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
  for (; *s; s++) {
    unsigned c = (unsigned char)*s;
    if (c < 32 || c > 126)
      c = '?';
    c -= 32;
    if (x + font->widths[c] > right)
      break;
    for (int j = 0; j < font->h; j++)
      for (int i = 0; i < font->w; i++) {
        int px = x + i, py = y + j;
        if (px < 0 || px >= 960 || py < 0 || py >= 544)
          continue;
        unsigned a = font->glyphs[c * font->w * font->h + j * font->w + i];
        if (a)
          fb[py * 960 + px] = blend(fb[py * 960 + px], color, a);
      }
    x += font->widths[c];
  }
}
static void tc(char *s, size_t n, double t, int precise) {
  int ms = (int)(fmax(0, t) * 1000 + .5);
  if (precise)
    snprintf(s, n, "%02d:%02d.%03d", ms / 60000, ms / 1000 % 60, ms % 1000);
  else
    snprintf(s, n, "%02d:%02d", ms / 60000, ms / 1000 % 60);
}
static void play_icon(int x, int y, uint32_t c) {
  for (int i = 0; i < 15; i++)
    rect(x + i, y + i / 2, 1, 18 - i, c);
}
static void toggle(int x, int y, int on) {
  roundbox(x, y, 44, 22, 11, on ? RGB(12, 152, 174) : RGB(52, 63, 82),
           on ? RGB(10, 97, 120) : RGB(34, 44, 62));
  roundbox(x + (on ? 24 : 3), y + 3, 16, 16, 8, on ? WHITE : MUTED,
           on ? WHITE : MUTED);
}
static void wave_icon(int x, int y, uint32_t c) {
  int heights[] = {10, 20, 32, 22, 14, 26, 12};
  for (int i = 0; i < 7; i++)
    roundbox(x + i * 6, y + (32 - heights[i]) / 2, 3, heights[i], 1, c, c);
}
static void fx_icon(int x, int y, int index, uint32_t c) {
  for (int i = 0; i < 30; i++) {
    int h;
    if (index == 0) {
      rect(x + i, y + i / 2, 2, 2, c);
      rect(x + i, y + 15 - i / 2, 2, 2, c);
    } else if (index == 1) {
      h = (int)(8 * expf(-i * .07f) * sinf(i * .9f));
      rect(x + i, y + 8 + h, 1, 2, c);
    } else if (index == 2) {
      h = (int)(15 * (1 - expf(-i * .13f)));
      rect(x + i, y + 16 - h, 2, 2, c);
    } else if (index == 3) {
      h = i < 15 ? 12 : 5;
      rect(x + i, y + 8 - h / 2, 1, h, c);
    } else {
      rect(x + i, y + (i < 15 ? 2 : 14), 1, 2, c);
      if (i == 15)
        rect(x + i, y + 2, 2, 14, c);
    }
  }
}
double ui_timeline_pre(const View *v) {
  return fmin(4, fmax(0, v->settings.start_a));
}
double ui_timeline_span(const View *v) {
  return ui_timeline_pre(v) + v->settings.duration + 4;
}
int ui_hit_test(int x, int y) {
  if (x < 14 || x >= 946)
    return -1;
  if (y >= 47 && y < 150)
    return x < 480 ? 0 : 1;
  if (y >= 272 && y < 320)
    return x < 330 ? 2 : x < 642 ? 3 : 4;
  if (x < 548 && y >= 355 && y < 505)
    return 5 + (y - 355) / 30;
  if (x >= 562 && y >= 352 && y < 436)
    return 12 + (y - 352) / 28;
  if (y >= 448 && y < 496)
    return x >= 756 ? 11 : x >= 562 ? 10 : -1;
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
  for (int i = 0; i < 960 * 544; i++)
    fb[i] = blend(fb[i], BG, 190);
}
void ui_draw(uint32_t *pixels, const View *v) {
  fb = pixels;
  for (int y = 0; y < 544; y++)
    rect(0, y, 960, 1, blend(RGB(19, 24, 40), BG, y * 255 / 544));
  wave_icon(19, 7, CYAN);
  txt(72, 5, &title, WHITE, "Circuit FX Studio", 420);
  roundbox(301, 11, 74, 22, 6, RGB(72, 35, 99), RGB(38, 25, 64));
  txt(310, 12, &small, RGB(225, 160, 254), "BETA 1.5", 70);
  txt(733, 13, &small, MUTED, "OFFLINE  /  WAV STUDIO", 211);
  rect(14, 40, 932, 1, EDGE);
  char s[180], a[32];
  for (int d = 0; d < 2; d++) {
    int x = 14 + d * 474;
    uint32_t c = d ? PINK : CYAN;
    panel(x, 48, 458, 101, v->row == d, c);
    roundbox(x + 12, 61, 65, 74, 8, blend(BG, c, 34), RGB(12, 20, 34));
    txt(x + 32, 65, &large, c, d ? "B" : "A", 40);
    for (int i = 0; i < 25; i++) {
      int h = v->peaks[d][i * 17] * 18 / 32768;
      rect(x + 19 + i * 2, 120 - h / 2, 1, h ? h : 1, c);
    }
    txt(x + 92, 57, &small, c,
        d ? "TRACK B  /  INCOMING" : "TRACK A  /  OUTGOING", 335);
    char name[256];
    snprintf(name, sizeof(name), "%s",
             v->songs[d] ? v->songs[d] : "Select a song");
    char *ext = strrchr(name, '.');
    if (ext)
      *ext = 0;
    txt(x + 92, 78, &title, WHITE, name, 350);
    tc(a, sizeof(a), v->lengths[d], 0);
    snprintf(s, sizeof(s), "%s    %s", v->songs[d] ? a : "No track loaded",
             v->songs[d] ? "STEREO WAV" : "Tap to open your music");
    txt(x + 93, 116, &small, MUTED, s, 350);
  }
  panel(14, 160, 932, 102, 0, EDGE);
  double pre = ui_timeline_pre(v), span = ui_timeline_span(v);
  int left = 54, start = left + (int)(880 * pre / span),
      end = left + (int)(880 * (pre + v->settings.duration) / span);
  for (int i = 0; i <= 8; i++) {
    int x = left + i * 110;
    rect(x, 184, 1, 70, RGB(31, 43, 60));
    tc(a, sizeof(a), v->settings.start_a - pre + span * i / 8, 0);
    txt(x - 14, 163, &small, MUTED, a, 60);
  }
  rect(start, 185, end - start, 68, RGB(42, 33, 72));
  txt(24, 190, &body, CYAN, "A", 24);
  txt(24, 224, &body, PINK, "B", 24);
  for (int d = 0; d < 2; d++) {
    uint32_t c = d ? PINK : CYAN;
    int y = 207 + d * 33;
    rect(left, y, 880, 1, RGB(43, 49, 69));
    for (int i = 0; i < 880; i++) {
      double rel = span * i / 880 - pre;
      float gain = 1;
      if (rel >= 0 && rel < v->settings.duration) {
        float t = rel / v->settings.duration;
        gain = d ? t : 1 - t;
        if ((v->settings.effects & FX_QUICK_CUT) ||
            !(v->settings.effects & FX_CROSSFADE))
          gain = d ? (t >= .5f) : (t < .5f);
      }
      int h = (int)(v->timeline[d][i] * 13 / 32768.0f);
      uint32_t col = blend(RGB(50, 43, 74), c, (int)(70 + 185 * gain));
      if (h)
        rect(left + i, y - h, 1, h * 2 + 1, col);
    }
  }
  rect(start, 184, 2, 69, PINK);
  rect(end, 184, 2, 69, PINK);
  roundbox((start + end) / 2 - 65, 183, 130, 18, 4, RGB(85, 40, 117),
           RGB(56, 30, 83));
  snprintf(s, sizeof(s), "TRANSITION  %.1fs", v->settings.duration);
  txt((start + end) / 2 - 57, 184, &small, WHITE, s, 118);
  if (v->playing) {
    int px = left + (int)(880 * fmin(v->play_position, span) / span);
    rect(px, 184, 2, 70, WHITE);
    roundbox(px - 3, 179, 8, 6, 2, WHITE, WHITE);
  }
  const char *labels[] = {"A CUE", "B CUE", "DURATION"};
  double vals[] = {v->settings.start_a, v->settings.start_b,
                   v->settings.duration};
  for (int i = 0; i < 3; i++) {
    int x = 14 + i * 316;
    panel(x, 273, 300, 45, v->row == i + 2, i == 1 ? PINK : CYAN);
    txt(x + 11, 280, &small, MUTED, labels[i], 78);
    if (i < 2)
      tc(s, sizeof(s), vals[i], 1);
    else
      snprintf(s, sizeof(s), "%.3f s", vals[i]);
    txt(x + 99, 281, &body, WHITE, s, 162);
    txt(x + 276, 281, &body, CYAN, "+", 20);
  }
  panel(14, 331, 534, 176, 0, EDGE);
  txt(29, 336, &small, WHITE, "EFFECT STACK", 240);
  txt(359, 336, &small, MUTED, "Tap to layer effects", 170);
  const char *fx[] = {"Crossfade", "Echo Out", "Filter Sweep", "Bass Swap",
                      "Quick Cut"};
  const char *desc[] = {"Smooth blend", "250 ms delay", "Low-pass sweep",
                        "Low-end exchange", "20 ms handoff"};
  for (int i = 0; i < 5; i++) {
    int y = 355 + i * 30, on = (v->settings.effects & (1u << i)) != 0;
    if (v->row == 5 + i)
      roundbox(22, y, 518, 28, 5, RGB(38, 58, 77), RGB(25, 38, 56));
    else if (i % 2 == 0)
      roundbox(22, y, 518, 28, 5, RGB(25, 33, 49), RGB(22, 29, 44));
    fx_icon(31, y + 6, i, on ? CYAN : MUTED);
    txt(79, y + 3, &body, on ? WHITE : MUTED, fx[i], 184);
    txt(280, y + 6, &small, MUTED, desc[i], 190);
    toggle(484, y + 3, on);
  }
  panel(562, 331, 384, 109, 0, EDGE);
  txt(575, 336, &small, WHITE, "16 WAV SOUNDS + 64 RECIPES", 350);
  const char *labels2[] = {"Browse WAV sounds + presets", "Effect BPM",
                           "FX intensity"};
  for (int i = 0; i < 3; i++) {
    int y = 352 + i * 28;
    roundbox(570, y, 368, 26, 5,
             v->row == 12 + i ? RGB(46, 58, 80) : RGB(24, 31, 47),
             RGB(17, 24, 38));
    txt(584, y + 3, &body, WHITE, labels2[i], 265);
    if (i == 0)
      snprintf(s, sizeof(s), "OPEN");
    else if (i == 1)
      snprintf(s, sizeof(s), "%.0f", v->settings.bpm);
    else
      snprintf(s, sizeof(s), "%.0f%%", v->settings.intensity * 100);
    txt(879, y + 6, &small, CYAN, s, 52);
  }
  panel(562, 448, 184, 48, v->row == 10, CYAN);
  play_icon(577, 463, CYAN);
  txt(605, 459, &title, CYAN, v->playing ? "Stop" : "Preview", 137);
  panel(756, 448, 190, 48, v->row == 11, PINK);
  rect(773, 460, 3, 15, v->ready ? PINK : MUTED);
  rect(768, 474, 13, 2, v->ready ? PINK : MUTED);
  txt(793, 459, &title, v->ready ? WHITE : MUTED, "Export", 144);
  txt(18, 512, &small, WHITE, v->status, 700);
  txt(741, 512, &small, MUTED, "START Preview   [] Replay", 205);
  txt(18, 530, &small, MUTED,
      "D-pad Navigate / Adjust     X Open / Toggle     L 1ms     R 10s     "
      "SELECT Rescan     TRIANGLE Exit",
      925);
  if (v->progress >= 0) {
    dim();
    panel(192, 185, 576, 164, 1, CYAN);
    txt(219, 204, &title, WHITE, "Rendering your transition", 520);
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
        "ux0:data/CircuitFX/music    /    44.1 kHz stereo 16-bit WAV", 760);
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
          "Copy WAV songs into the music folder, then press SELECT.", 770);
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
    snprintf(s, sizeof(s), "%d beats  /  %.2f sec at %.0f BPM", p.beats,
             p.beats * 60.0 / v->settings.bpm, v->settings.bpm);
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
                               : "FX intensity");
    txt(264, 88, &title, WHITE, s, 430);
    roundbox(264, 128, 432, 51, 6, RGB(9, 17, 29), RGB(12, 23, 37));
    txt(278, 137, &large, CYAN, v->entry, 330);
    txt(617, 152, &small, MUTED,
        v->editing == 4   ? "BPM"
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
            ? "60-200 BPM; effects timing, not automatic beatmatching"
        : v->editing == 5 ? "0-100%; controls effect strength, not track volume"
                          : "Enter seconds, e.g. 155.250 = 02:35.250",
        430);
  }
}
#ifdef HOST_UI
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
  v.settings.bpm = 128;
  v.settings.intensity = 1;
  v.settings.sample_speed = 1.f;
  v.row = 10;
  v.ready = 1;
  snprintf(v.status, sizeof(v.status),
           "Preview ready. Layer effects or export your mix.");
  /* Synthetic fixture only; production waveforms always come from selected
   * WAVs. */
  for (int d = 0; d < 2; d++) {
    for (int i = 0; i < 440; i++)
      v.peaks[d][i] = 12000 + 9000 * sinf(i * .27f);
    for (int i = 0; i < 880; i++) {
      double rel = ui_timeline_span(&v) * i / 880 - ui_timeline_pre(&v);
      v.timeline[d][i] =
          (d ? rel >= 0 : rel < 15)
              ? (int)(5000 + 16000 * fabs(sin(i * .17) * sin(i * .031 + d)))
              : 0;
    }
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
