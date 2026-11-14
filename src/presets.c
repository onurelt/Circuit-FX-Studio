#include "presets.h"
#include "render.h"
#include <stdio.h>
#include <string.h>
const char *preset_banks[BANK_COUNT] = {
    "GSP-inspired", "Micky Friedmann-inspired", "Anne Louise-inspired",
    "Erik Vilar-inspired"};
/* Original recipes informed by genre descriptions, not transcribed DJ
 * techniques. 16 distinct constructions, tuned separately for each genre bank.
 */
static const char *names[4][16] = {
    {"Vocal Glide", "Tribal Bass Relay", "Quarter Echo", "Dotted Vocal Trail",
     "Tech Filter Lift", "Clean Drop Cut", "Progressive Dip",
     "Stereo Echo Bridge", "Pulse Handoff", "One Beat Roll", "Tight Roll Exit",
     "Metallic Lift", "Air Sweep", "Ring Whoosh", "Echo Bass Tunnel",
     "Peak Release"},
    {"Melodic Arc", "Deep Bass Exchange", "Melodic Echo", "Dotted Atmosphere",
     "Dark Filter Rise", "Late Scene Cut", "Breakdown Passage",
     "Wide Echo Journey", "Techno Pulse", "Rolling Tension",
     "Tight Tension Exit", "Metallic Bloom", "Atmospheric Lift",
     "Long Ring Whoosh", "Low-End Suspense", "Dramatic Release"},
    {"Uplifting Glide", "Tribal Groove Swap", "Bright Echo", "Dotted Glow",
     "Sunrise Filter", "Celebration Cut", "Open-Air Break", "Stereo Glow",
     "Tribal Pulse", "Groove Roll", "Quick Groove Exit", "Shimmer Lift",
     "Bright Air Sweep", "Shimmer Whoosh", "Bass Celebration",
     "Festival Release"},
    {"House Drive", "Progressive Bass Push", "Driving Echo", "Dotted Drive",
     "Tech Pressure", "Sharp Drop Cut", "Deep Reset", "Wide Driving Echo",
     "Tech Gate", "Driving Roll", "Fast Roll Cut", "Metallic Pressure",
     "Pressure Sweep", "Electronic Whoosh", "Deep Bass Tunnel",
     "Peak Pressure"}};
static const char *desc[16] = {
    "Curved blend; keeps the handoff smooth.",
    "Bass exchange with a shaped crossfade.",
    "Tempo-timed echo on the outgoing track.",
    "Dotted delay with a gradual bass exchange.",
    "High-pass lift thins A before B arrives.",
    "Short, click-reduced switch late in overlap.",
    "Dip through a quiet midpoint, then rebuild.",
    "Alternating stereo delay across the handoff.",
    "Rhythmic volume pulses on A, without added drums.",
    "Repeats a captured one-beat slice of A.",
    "Short captured slice repeats into a quick cut.",
    "Rising ring modulation on A for metallic tension.",
    "Filtered electronic noise rises then clears.",
    "Ring modulation plus filtered noise, no drum layer.",
    "Low-pass sweep, echo and bass handoff.",
    "Pulse, high-pass, noise lift and delayed handoff."};
int preset_get(int id, TransitionPreset *p) {
  if (id < 1 || id > PRESET_COUNT || !p)
    return -1;
  int b = (id - 1) / 16, k = (id - 1) % 16;
  memset(p, 0, sizeof(*p));
  p->bank = b;
  snprintf(p->name, sizeof(p->name), "%s", names[b][k]);
  p->description = desc[k];
  p->beats = (b == 1 ? 32 : 16);
  p->effects = FX_CROSSFADE;
  p->curve = (float[]){1.15f, 1.7f, .8f, 1.35f}[b];
  p->delay_beats = (float[]){.5f, 1.f, .75f, .25f}[b];
  p->feedback = .24f + .055f * b;
  p->wet = .28f + .035f * b;
  p->filter_end = 240 + 110 * b;
  p->bass_pivot = .40f + .06f * b;
  p->cut_at = .6f + .08f * b;
  p->gate_beats = (float[]){.5f, 1.f, .5f, .25f}[b];
  p->gate_depth = .55f + .08f * b;
  p->roll_beats = 1.f;
  p->ring_hz = 180 + 80 * b;
  p->ring_mix = .22f + .04f * b;
  p->noise_mix = .07f + .012f * b;
  switch (k) {
  case 0:
    p->beats = b == 1 ? 64 : 32;
    break;
  case 1:
    p->effects |= FX_BASS_SWAP;
    break;
  case 2:
    p->effects |= FX_ECHO;
    p->beats = 8;
    p->delay_beats = .25f;
    break;
  case 3:
    p->effects |= FX_ECHO | FX_BASS_SWAP;
    p->delay_beats = .75f;
    p->feedback += .08f;
    break;
  case 4:
    p->effects |= FX_FILTER;
    p->mods = MOD_HIGH_PASS;
    p->filter_end = 1800 + 400 * b;
    break;
  case 5:
    p->effects = FX_QUICK_CUT;
    p->beats = 4;
    break;
  case 6:
    p->mods = MOD_DIP;
    p->effects |= FX_FILTER;
    p->beats = b == 1 ? 64 : 32;
    break;
  case 7:
    p->effects |= FX_ECHO;
    p->mods = MOD_PINGPONG;
    p->delay_beats = .5f;
    p->wet += .12f;
    break;
  case 8:
    p->mods = MOD_GATE;
    p->effects |= FX_BASS_SWAP;
    p->beats = 8;
    break;
  case 9:
    p->mods = MOD_ROLL;
    p->effects |= FX_ECHO;
    p->roll_beats = 1;
    break;
  case 10:
    p->mods = MOD_ROLL;
    p->effects = FX_QUICK_CUT | FX_FILTER;
    p->roll_beats = .25f;
    p->beats = 4;
    break;
  case 11:
    p->mods = MOD_RING;
    p->effects |= FX_FILTER;
    p->mods |= MOD_HIGH_PASS;
    p->filter_end = 1200 + 150 * b;
    break;
  case 12:
    p->mods = MOD_NOISE;
    p->effects |= FX_BASS_SWAP;
    break;
  case 13:
    p->mods = MOD_RING | MOD_NOISE;
    p->effects |= FX_FILTER | FX_ECHO;
    p->beats = b == 1 ? 64 : 32;
    break;
  case 14:
    p->effects |= FX_FILTER | FX_BASS_SWAP | FX_ECHO;
    p->filter_end = 120 + 55 * b;
    p->wet += .08f;
    break;
  case 15:
    p->mods = MOD_GATE | MOD_NOISE | MOD_HIGH_PASS;
    p->effects = FX_FILTER | FX_ECHO | FX_BASS_SWAP | FX_QUICK_CUT;
    p->filter_end = 2400 + 300 * b;
    break;
  }
  return 0;
}
