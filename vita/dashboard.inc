/* Minimal two-track Auto Mix workspace. Controls here map to real planner,
 * transport, cue and export actions; no preset or bundled-FX browser is shown. */
static void flat_button(int x, int y, int w, int h, const char *label,
                        uint32_t color, int selected) {
  roundbox(x, y, w, h, 7, selected ? blend(BG, color, 87) : RGB(27,39,56),
           selected ? blend(BG, color, 54) : RGB(18,27,41));
  txt(x + 10, y + (h - 19) / 2, &body, selected ? WHITE : color,
      label, w - 18);
}

void ui_draw(uint32_t *pixels, const View *v) {
  fb = pixels;
  rect(0, 0, 960, 544, BG);
  char s[180], a[32];
  if (v->home_screen) {
    roundbox(18, 18, 924, 508, 24, RGB(27,39,58), BG);
    txt(52, 48, &large, WHITE, "CIRCUIT FX STUDIO", 650);
    roundbox(52, 138, 856, 148, 20, RGB(30,76,88), RGB(22,37,54));
    txt(78, 169, &title, CYAN, "VOCAL REMOVER", 600);
    txt(78, 222, &body, WHITE,
        "One song. Real AI stem separation is not available in this build.", 790);
    roundbox(52, 310, 856, 148, 20, RGB(81,46,91), RGB(35,31,55));
    txt(78, 341, &title, PINK, "AUTO MIX", 600);
    txt(78, 394, &body, WHITE,
        "Two songs. Beatmatched transition candidates, preview and WAV export.", 790);
    txt(290, 488, &body, MUTED, "Touch a mode to begin", 400);
    return;
  }

  if (v->workflow == 1) {
    roundbox(10, 5, 730, 39, 6, RGB(24,35,52), RGB(15,25,39));
    txt(24, 8, &small, CYAN, "VOCAL REMOVER", 160);
    txt(24, 23, &body, WHITE,
        v->songs[0] ? v->songs[0] : "Select one song", 560);
    tc(a, sizeof(a), v->lengths[0], 0);
    txt(620, 12, &small, MUTED, v->songs[0] ? a : "OPEN", 100);
    flat_button(760, 5, 190, 39, "HOME", PINK, 0);

    txt(22, 55, &small, MUTED,
        "Import separated 44.1 kHz stereo WAV sidecars. Vita does not run AI separation.", 900);
    const char *names[4] = {"VOCALS", "DRUMS", "BASS", "OTHER"};
    const uint32_t colors[4] = {PINK, RGB(255,177,72), CYAN, RGB(164,139,255)};
    for (int i = 0; i < 4; ++i) {
      int y = 88 + i * 66;
      unsigned bit = 1u << i;
      int loaded = (v->stem_present & bit) != 0;
      int muted = (v->stem_mute_mask & bit) != 0;
      int solo = v->stem_solo == i;
      roundbox(14, y, 932, 58, 8,
               loaded ? RGB(23,36,53) : RGB(20,29,42),
               loaded ? RGB(17,27,42) : RGB(14,21,33));
      txt(28, y + 6, &body, loaded ? colors[i] : MUTED, names[i], 150);
      txt(28, y + 29, &small, loaded ? MUTED : RGB(111,125,142),
          loaded ? "READY" : "MISSING", 150);

      int gain = (int)fmax(0, fmin(100, v->stem_gain[i] * 50));
      roundbox(190, y + 22, 244, 12, 6, RGB(12,21,34), RGB(12,21,34));
      if (gain > 0)
        roundbox(190, y + 22, 244 * gain / 100, 12, 6, colors[i], colors[i]);
      roundbox(190 + 244 * gain / 100 - 5, y + 17, 10, 22, 5,
               WHITE, RGB(213,225,238));
      snprintf(s, sizeof(s), "%d%%", (int)lround(v->stem_gain[i] * 100));
      txt(444, y + 18, &small, WHITE, s, 58);

      flat_button(510, y + 8, 74, 42, muted ? "MUTED" : "MUTE",
                  muted ? MUTED : WHITE, muted);
      flat_button(590, y + 8, 74, 42, solo ? "SOLO ON" : "SOLO",
                  solo ? colors[i] : WHITE, solo);
      flat_button(670, y + 8, 122, 42, "LISTEN", colors[i], 0);
      flat_button(798, y + 8, 138, 42, "EXPORT WAV", colors[i], 0);
    }

    flat_button(14, 368, 220, 48, "PREVIEW MIX", CYAN, 0);
    flat_button(242, 368, 112, 48, "STOP", WHITE, 0);
    flat_button(362, 368, 220, 48, "EXPORT MIX", PINK, 0);
    flat_button(590, 368, 190, 48, "RESCAN STEMS", WHITE, 0);
    flat_button(788, 368, 158, 48, "HOME", PINK, 0);
    txt(22, 430, &small, MUTED,
        "Place Track.vocals.wav, Track.drums.wav, Track.bass.wav and Track.other.wav beside the song in music/.", 920);
    txt(22, 454, &small, MUTED,
        "Mute/Solo affect Preview Mix. Listen and Export WAV operate on that stem only.", 920);
    if (!v->stem_present)
      txt(22, 478, &small, RGB(255,177,72),
          "No aligned separated stems found. This screen does not create or fake stems.", 920);
    else if (v->status[0] && v->progress < 0)
      txt(22, 478, &small, CYAN, v->status, 920);
    return;
  }

  for (int d = 0; d < 2; ++d) {
    int x = 10 + d * 480;
    uint32_t c = d ? PINK : CYAN;
    roundbox(x, 5, 460, 39, 6, RGB(24,35,52), RGB(15,25,39));
    roundbox(x + 4, 9, 34, 31, 5, c, blend(c, BG, 45));
    txt(x + 13, 12, &title, BG, d ? "B" : "A", 22);
    txt(x + 49, 8, &small, c, d ? "TRACK B" : "TRACK A", 86);
    txt(x + 49, 23, &body, WHITE, v->songs[d] ? v->songs[d] : "Select song", 285);
    tc(a, sizeof(a), v->lengths[d], 0);
    txt(x + 354, 8, &small, MUTED, v->songs[d] ? a : "OPEN", 85);
    if (v->songs[d]) {
      if (v->track_bpm[d] > 0)
        snprintf(s, sizeof(s), "%.1f BPM%s", v->track_bpm[d],
                 v->bpm_manual[d] ? "*" : "");
      else
        snprintf(s, sizeof(s), "BPM unavailable");
      txt(x + 354, 25, &small, c, s, 101);
    }
  }

  rect(10, 50, 940, 265, RGB(14,25,39));
  double span = v->wave_window > 0 ? v->wave_window : 45;
  double pan = v->wave_window > 0 ? v->wave_pan : -22.5;
  double tempo_ratio = v->auto_beatmatch && v->track_bpm[0] > 0 &&
                       v->track_bpm[1] > 0
                           ? v->track_bpm[0] / v->track_bpm[1] : 1.0;
  int cue_x[2], end_x[2];
  for (int d = 0; d < 2; ++d) {
    double cue = d ? v->settings.start_b : v->settings.start_a;
    cue_x[d] = 54 + (int)((cue - v->wave_origin[d] - pan) * 880 / span);
    end_x[d] = cue_x[d] + (int)(v->settings.duration *
                                  (d ? tempo_ratio : 1.0) * 880 / span);
    if (cue_x[d] < 934 && end_x[d] > 54) {
      int left = cue_x[d] < 54 ? 54 : cue_x[d];
      int right = end_x[d] > 934 ? 934 : end_x[d];
      if (right > left) rect(left, d ? 195 : 79, right-left, 115, RGB(42,30,64));
    }
  }
  for (int k = 0; k <= 8; ++k) {
    int xx = 54 + k * 110;
    rect(xx, 80, 1, 230, RGB(30,48,62));
    double at = v->wave_origin[0] + pan + span * k / 8.0;
    tc(a, sizeof(a), at, 0);
    txt(xx + 2, 56, &small, MUTED, a, 54);
  }
  rect(54, 193, 880, 1, EDGE);
  for (int d = 0; d < 2; ++d) {
    int y = d ? 253 : 135;
    uint32_t c = d ? PINK : CYAN;
    txt(16, y - 14, &large, c, d ? "B" : "A", 34);
    if (v->lengths[d] > 0) draw_wave(v, d, pan, span, y, c);
    else txt(350, y - 8, &body, MUTED,
             d ? "Select Track B above" : "Select Track A above", 270);
    if (cue_x[d] >= 54 && cue_x[d] < 934 && v->songs[d]) {
      rect(cue_x[d], y - 51, 2, 103, WHITE);
      roundbox(cue_x[d] - 4, y + 48, 10, 7, 3, c, c);
    }
  }
  for (int d = 0; d < 2; ++d)
    if (end_x[d] >= 54 && end_x[d] < 934)
      rect(end_x[d], d ? 195 : 79, 2, 115, RGB(203,177,235));
  if (cue_x[0] >= 54 && cue_x[0] < 934) {
    tc(a, sizeof(a), v->settings.duration, 1);
    snprintf(s, sizeof(s), "OVERLAP %s", a);
    txt(cue_x[0] + 8, 79, &small, WHITE, s, 185);
  }
  if (v->playing) {
    double at = v->settings.start_a - ui_timeline_pre(v) + v->play_position;
    int xx = 54 + (int)((at - v->wave_origin[0] - pan) * 880 / span);
    if (xx >= 54 && xx < 934) rect(xx, 80, 2, 229, RGB(230,247,255));
  }

  flat_button(10, 318, 102, 46, v->playing ? "PLAYING" : "PREVIEW", CYAN,
              v->row == 10);
  flat_button(118, 318, 60, 46, "STOP", WHITE, 0);
  flat_button(184, 318, 74, 46, "REPLAY", WHITE, 0);
  const char *labels[] = {"A CUE", "B CUE", "OVERLAP"};
  double values[] = {v->settings.start_a, v->settings.start_b, v->settings.duration};
  for (int i = 0; i < 3; ++i) {
    int x = 264 + 126 * i;
    roundbox(x, 318, 120, 46, 6, RGB(28,44,61), RGB(18,30,45));
    txt(x + 8, 320, &small, i == 1 ? PINK : CYAN, labels[i], 100);
    tc(a, sizeof(a), values[i], 1);
    txt(x + 8, 339, &small, WHITE, a, 104);
  }
  flat_button(642, 318, 58, 46, "-", CYAN, 0);
  flat_button(706, 318, 58, 46, "+", CYAN, 0);
  flat_button(770, 318, 54, 46, "FIT", CYAN, 0);
  flat_button(830, 318, 120, 46, "EXPORT WAV", PINK, v->row == 11);

  const char *names[3] = {"SMOOTH", "ENERGY", "VOCAL BLEND"};
  const uint32_t colors[3] = {CYAN, RGB(246,184,66), PINK};
  for (int i = 0; i < 3; ++i) {
    int x = 14 + i * 312;
    int selected = v->mix_variation == i;
    roundbox(x, 374, 302, 92, 9,
             selected ? blend(BG, colors[i], 76) : RGB(25,38,56),
             selected ? blend(BG, colors[i], 48) : RGB(14,24,39));
    txt(x + 14, 384, &title, selected ? colors[i] : WHITE, names[i], 270);
    if (v->candidates_ready) {
      char ca[32], cb[32], cd[32];
      tc(ca, sizeof(ca), v->candidate_start_a[i], 1);
      tc(cb, sizeof(cb), v->candidate_start_b[i], 1);
      tc(cd, sizeof(cd), v->candidate_duration[i], 1);
      snprintf(s, sizeof(s), "A %s   B %s   %s", ca, cb, cd);
      txt(x + 14, 415, &small, WHITE, s, 270);
      txt(x + 14, 438, &small, MUTED,
          i == 0 ? "Seamless, natural blend" :
          i == 1 ? "Shorter handoff, energy focused" :
                   "A vocals + B instrumental sidecars", 270);
    } else {
      txt(x + 14, 418, &small, MUTED, "Tap to generate for both songs", 270);
      txt(x + 14, 440, &small, MUTED,
          i == 2 ? "Prepared stems are optional" : "Cached beat and waveform analysis", 270);
    }
  }

  flat_button(14, 486, 224, 44,
              v->auto_beatmatch ? "BEATMATCH  ON" : "BEATMATCH  OFF",
              v->auto_beatmatch ? CYAN : MUTED, v->auto_beatmatch);
  flat_button(246, 486, 212, 44, "NEXT VARIATION", WHITE, 0);
  flat_button(466, 486, 222, 44, "REGENERATE", PINK, 0);
  txt(704, 491, &small, MUTED,
      v->mix_variation == AUTOMIX_VOCAL_BLEND
          ? "Aligned sidecars or clean fallback" : "Select a mix card to compare",
      238);

  if (v->status[0] && v->progress < 0 && !v->confirm && !v->browser &&
      !v->preset_browser && !v->editing) {
    rect(54, 293, 880, 18, RGB(15,25,39));
    txt(60, 295, &small, MUTED, v->status, 868);
  }
