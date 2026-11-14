#include "ui.h"
#include "font.h"
#include <stdio.h>
#include <string.h>
#define RGB(r, g, b)                                                           \
  ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | 0xff000000u)
#define BG RGB(11, 15, 25)
#define PANEL RGB(22, 29, 43)
#define WHITE RGB(235, 242, 253)
#define MUTED RGB(144, 161, 185)
#define CYAN RGB(26, 217, 236)
#define PINK RGB(235, 73, 224)
static uint32_t *fb;
static void rect(int x, int y, int w, int h, uint32_t c) {
  for (int j = y; j < y + h && j < 544; j++)
    for (int i = x; i < x + w && i < 960; i++)
      if (i >= 0 && j >= 0)
        fb[j * 960 + i] = c;
}
static void text(int x, int y, uint32_t color, const char *s, int limit) {
  int right = x + limit;
  for (; *s; s++) {
    unsigned c = (unsigned char)*s;
    if (c < 32 || c > 126)
      c = '?';
    c -= 32;
    if (x + widths[c] > right)
      break;
    for (int j = 0; j < 26; j++)
      for (int i = 0; i < 24; i++) {
        int px = x + i, py = y + j;
        if (px < 0 || px >= 960 || py < 0 || py >= 544)
          continue;
        unsigned a = glyphs[c * 624 + j * 24 + i];
        if (!a)
          continue;
        uint32_t old = fb[py * 960 + px], v = 0xff000000u;
        for (int k = 0; k < 24; k += 8)
          v |= ((((color >> k) & 255) * a + ((old >> k) & 255) * (255 - a)) /
                255)
               << k;
        fb[py * 960 + px] = v;
      }
    x += widths[c];
  }
}
static void timecode(char *s, size_t n, double t) {
  int ms = (int)(t * 1000 + .5);
  snprintf(s, n, "%02d:%02d.%03d", ms / 60000, ms / 1000 % 60, ms % 1000);
}
static void card(int x, int y, int w, int h, int active, uint32_t accent) {
  rect(x, y, w, h, PANEL);
  rect(x, y, w, active ? 3 : 1, active ? WHITE : accent);
}
void ui_draw(uint32_t *pixels, const View *v) {
  fb = pixels;
  rect(0, 0, 960, 544, BG);
  rect(0, 0, 960, 3, CYAN);
  text(22, 13, CYAN, "|||", 60);
  text(62, 13, WHITE, "CIRCUIT FX STUDIO", 400);
  text(751, 13, PINK, "VITA / BETA 1", 190);
  char s[180], a[24], b[24];
  for (int d = 0; d < 2; d++) {
    int x = 16 + d * 472;
    uint32_t color = d ? PINK : CYAN;
    card(x, 53, 456, 105, v->row == d, color);
    text(x + 14, 60, color, d ? "B / INCOMING" : "A / OUTGOING", 400);
    text(x + 14, 86, WHITE, v->songs[d] ? v->songs[d] : "Choose a WAV song...",
         425);
    timecode(a, sizeof(a), v->lengths[d]);
    snprintf(s, sizeof(s), "%s   /   X to browse",
             v->songs[d] ? a : "--:--.---");
    text(x + 14, 120, MUTED, s, 425);
    card(x, 167, 456, 60, 0, color);
    rect(x + 10, 196, 436, 1, RGB(49, 59, 78));
    for (int i = 0; i < 436; i++) {
      int h = v->peaks[d][i] * 24 / 32768;
      rect(x + 10 + i, 196 - h, 1, 2 * h + 1, color);
    }
  }
  timecode(a, sizeof(a), v->settings.start_a);
  timecode(b, sizeof(b), v->settings.start_b);
  card(16, 236, 306, 51, v->row == 2, CYAN);
  snprintf(s, sizeof(s), "A cue   %s", a);
  text(29, 249, WHITE, s, 285);
  card(330, 236, 306, 51, v->row == 3, PINK);
  snprintf(s, sizeof(s), "B cue   %s", b);
  text(343, 249, WHITE, s, 285);
  card(644, 236, 300, 51, v->row == 4, CYAN);
  snprintf(s, sizeof(s), "Duration   %.3f s", v->settings.duration);
  text(657, 249, WHITE, s, 280);
  text(20, 296, MUTED, "EFFECT STACK", 400);
  text(496, 296, MUTED, "PREVIEW / EXPORT", 430);
  const char *fx[] = {"Crossfade", "Echo Out", "Filter Sweep", "Bass Swap",
                      "Quick Cut"};
  for (int i = 0; i < 5; i++) {
    int y = 326 + i * 30;
    card(16, y, 456, 27, v->row == 5 + i, CYAN);
    text(29, y + 1, v->row == 5 + i ? WHITE : MUTED, fx[i], 300);
    text(406, y + 1, (v->settings.effects & (1u << i)) ? CYAN : MUTED,
         (v->settings.effects & (1u << i)) ? "ON" : "OFF", 55);
  }
  card(488, 326, 456, 54, v->row == 10, CYAN);
  text(510, 341, CYAN, v->playing ? "STOP PREVIEW" : "PLAY TRANSITION PREVIEW",
       420);
  card(488, 389, 456, 54, v->row == 11, PINK);
  text(510, 404, WHITE, "EXPORT FULL MIX / WAV", 420);
  text(496, 451, MUTED, "A to cue + overlap + remaining B", 440);
  rect(16, 484, 928, 2, RGB(46, 57, 76));
  text(19, 490, WHITE, v->status, 925);
  text(19, 519, MUTED,
       "D-pad: move / adjust    X: select    L: 1 ms    R: 10 s    START: "
       "preview    SELECT: scan",
       925);
  if (v->progress >= 0) {
    rect(160, 190, 640, 150, RGB(27, 37, 54));
    text(188, 210, WHITE, "RENDERING AUDIO", 580);
    snprintf(s, sizeof(s), "%d%%   /   Circle cancels safely", v->progress);
    text(188, 250, MUTED, s, 580);
    rect(188, 302, 584, 5, BG);
    rect(188, 302, 584 * v->progress / 100, 5, CYAN);
  }
  if (v->confirm) {
    rect(160, 180, 640, 180, RGB(27, 37, 54));
    text(188, 204, WHITE, "EXPORT THIS MIX?", 580);
    text(188, 244, MUTED, "Keeps A before its cue; B begins at its cue.", 580);
    text(188, 276, MUTED, "Original songs are preserved.", 580);
    text(188, 320, CYAN, "X: Export     Circle: Cancel", 580);
  }
  if (v->browser) {
    rect(65, 45, 830, 454, RGB(19, 27, 41));
    text(88, 60, CYAN, "SELECT WAV / ux0:data/CircuitFX/music", 780);
    text(88, 92, MUTED, "44.1 kHz / stereo / 16-bit PCM     Circle: back", 780);
    for (int i = 0; i < 10; i++)
      if (v->entries[i]) {
        int y = 132 + i * 31;
        if (v->first + i == v->browser_pos)
          rect(82, y, 795, 30, RGB(35, 62, 79));
        text(94, y, WHITE, v->entries[i], 770);
      }
    if (!v->browser_count)
      text(88, 180, WHITE, "No WAV files. Copy songs into the music folder.",
           770);
    snprintf(s, sizeof(s), "%d files   /   D-pad: browse   X: load",
             v->browser_count);
    text(88, 464, MUTED, s, 780);
  }
}
#ifdef HOST_UI
#include <stdlib.h>
int main(void) {
  uint32_t *p = calloc(960 * 544, 4);
  View v = {0};
  v.progress = -1;
  v.settings = (MixSettings){155, 48, 15, 15};
  v.songs[0] = "Neon Horizons.wav";
  v.songs[1] = "Midnight Circuit.wav";
  v.lengths[0] = 360;
  v.lengths[1] = 420;
  v.row = 10;
  snprintf(v.status, sizeof(v.status),
           "Choose effects, then preview your transition.");
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
