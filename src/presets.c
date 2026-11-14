#include "presets.h"
#include "render.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const char *preset_banks[BANK_COUNT] = {
    "GSP-inspired",         "Micky Friedmann-inspired",
    "Anne Louise-inspired", "Erik Vilar-inspired",
    "WAV FX (1-16)",        "WAV FX (17-25)",
    "DJ Paulo-inspired",    "Honey Dijon-inspired"};
/* Original recipes informed by genre descriptions, not transcribed DJ
 * techniques. 16 distinct constructions, tuned separately for each genre bank.
 */
static const char *names[6][16] = {
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
     "Peak Pressure"},
    {"Paulo Vocal Lift", "Tribal Roller", "Paulo Echo Tail", "Classic Rework",
     "Percussion Filter", "Peak Hour Cut", "Vocal Bridge", "Wide Club Echo",
     "Groove Gate", "Tribal Roll", "Fast Floor Cut", "Bright Riser",
     "Percussive Sweep", "Classic Whoosh", "Sub Exchange",
     "Dancefloor Release"},
    {"Chicago House Glide", "Jackin Bass Pass", "Dijon Echo", "Soulful Trail",
     "Dusty Filter Lift", "Disco Cut", "House Breakdown", "Wide Room Echo",
     "Four Floor Pulse", "House Roll", "Quick Disco Cut", "Warm Riser",
     "Vinyl Air Sweep", "Shimmer Whoosh", "Low-End Lift", "Club Release"}};
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
  if (b == 4 || b == 5) {
    static const char *sample_names[25] = {"Fast Whoosh",
                                           "Airy Whoosh",
                                           "Deep Whoosh",
                                           "Whip Sweep",
                                           "Right-to-Left Whoosh",
                                           "Long Pass",
                                           "Tight Whoosh",
                                           "Wind Swirl",
                                           "Tension Riser",
                                           "Big Riser",
                                           "Air Riser",
                                           "Short Riser",
                                           "Reverse Cymbal (2s)",
                                           "Reverse Cymbal (4s)",
                                           "Glitch Flick",
                                           "Glitch Switch",
                                           "Glitch Burst",
                                           "Glitch Bridge",
                                           "Glitch Stutter",
                                           "Glitch Snap",
                                           "Cinematic Boom",
                                           "Punch Impact",
                                           "Sub Drop",
                                           "Hit Tail",
                                           "Dry Thud"};
    static const char *reverse_names[7] = {
        "Glitch Burst Reverse",  "Glitch Bridge Reverse",
        "Glitch Stutter Reverse", "Glitch Snap Reverse",
        "Cinematic Boom Reverse", "Dry Thud Reverse", "Sub Drop Reverse"};
    static const int reverse_samples[7] = {17, 18, 19, 20, 21, 25, 23};
    int sample = (b - 4) * 16 + k + 1;
    if (b == 5 && k >= 9) {
      p->name[0] = 0;
      snprintf(p->name, sizeof(p->name), "%s", reverse_names[k - 9]);
      sample = reverse_samples[k - 9];
      p->reset_sample_settings = 1;
      p->default_reverse = 1;
    } else {
      if (sample > 25)
        return -1;
      snprintf(p->name, sizeof(p->name), "%s", sample_names[sample - 1]);
    }
    p->description =
        "WAV sound layered over your songs. Reverse and speed controls affect "
        "audition, preview and export.";
    p->sample_id = sample;
    p->beats =
        sample >= 9 && sample <= 14 ? 16 : (sample == 6 || sample == 8 ? 8 : 4);
    p->effects = FX_CROSSFADE | ((b == 5 && k >= 9) ? FX_ECHO : 0);
    p->curve = 1;
    p->delay_beats = .25f;
    p->filter_end = 220;
    p->bass_pivot = p->cut_at = .5f;
    p->roll_beats = 1;
    return 0;
  }
  int style = b >= 6 ? b - 6 : b;
  snprintf(p->name, sizeof(p->name), "%s", names[b < 4 ? b : style + 4][k]);
  if (b >= 6)
    p->description = b == 6
                         ? "Original circuit-house recipe guided by DJ Paulo's "
                           "published vocal, tribal and classic-rework style."
                         : "Original house recipe guided by Honey Dijon’s "
                           "Chicago house and queer-club background.";
  else
    p->description = desc[k];
  p->beats = (style == 1 ? 32 : 16);
  p->effects = FX_CROSSFADE;
  p->curve = (float[]){1.15f, 1.7f, .8f, 1.35f}[style];
  p->delay_beats = (float[]){.5f, 1.f, .75f, .25f}[style];
  p->feedback = .24f + .055f * style;
  p->wet = .28f + .035f * style;
  p->filter_end = 240 + 110 * style;
  p->bass_pivot = .40f + .06f * style;
  p->cut_at = .6f + .08f * style;
  p->gate_beats = (float[]){.5f, 1.f, .5f, .25f}[style];
  p->gate_depth = .55f + .08f * style;
  p->roll_beats = 1.f;
  p->ring_hz = 180 + 80 * style;
  p->ring_mix = .22f + .04f * style;
  p->noise_mix = .07f + .012f * style;
  if (b >= 6) {
    float factor = b == 6 ? 1.19f : .87f;
    p->curve *= factor;
    p->delay_beats *= b == 6 ? 1.35f : 1.2f;
    p->feedback += b == 6 ? .045f : .07f;
    p->wet += b == 6 ? .04f : .08f;
    p->filter_end *= b == 6 ? 1.22f : .76f;
    p->bass_pivot = fminf(.88f, p->bass_pivot + (b == 6 ? .09f : -.07f));
  }
  if (b >= 6)
    p->effects |= FX_ECHO;
  switch (k) {
  case 0:
    p->beats = style == 1 ? 64 : 32;
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
    p->beats = style == 1 ? 64 : 32;
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
    p->beats = style == 1 ? 64 : 32;
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
  if (b >= 6)
    p->effects |= FX_ECHO;
  return 0;
}
