#include "prepared_stems.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

static int sidecar_path(const char *track, const char *kind,
                        char *out, size_t capacity) {
  const char *dot = strrchr(track, '.');
  const char *slash = strrchr(track, '/');
  if (!dot || (slash && dot < slash))
    return 0;
  size_t prefix = (size_t)(dot - track);
  if (prefix > 511)
    return 0;
  int written = snprintf(out, capacity, "%.*s.%s.wav",
                         (int)prefix, track, kind);
  return written >= 0 && (size_t)written < capacity;
}

int prepared_vocal_handoff_paths(const char *track_a, const char *pcm_a,
                                 const char *track_b, const char *pcm_b,
                                 char *vocal_a, size_t vocal_a_capacity,
                                 char *instrumental_b,
                                 size_t instrumental_b_capacity) {
  if (vocal_a && vocal_a_capacity)
    vocal_a[0] = '\0';
  if (instrumental_b && instrumental_b_capacity)
    instrumental_b[0] = '\0';
  if (!track_a || !pcm_a || !track_b || !pcm_b || !vocal_a ||
      !instrumental_b || !vocal_a_capacity || !instrumental_b_capacity)
    return -1;

  char candidate_vocal[1024], candidate_instrumental[1024];
  if (!sidecar_path(track_a, "vocals", candidate_vocal,
                    sizeof(candidate_vocal)) ||
      !sidecar_path(track_b, "instrumental", candidate_instrumental,
                    sizeof(candidate_instrumental)))
    return 0;
  if (strlen(candidate_vocal) + 1 > vocal_a_capacity ||
      strlen(candidate_instrumental) + 1 > instrumental_b_capacity)
    return 0;

  uint32_t frames_a, offset_a, frames_b, offset_b;
  uint32_t frames_vocal, offset_vocal, frames_instrumental, offset_instrumental;
  if (inspect_wav(pcm_a, &frames_a, &offset_a) ||
      inspect_wav(pcm_b, &frames_b, &offset_b) ||
      inspect_wav(candidate_vocal, &frames_vocal, &offset_vocal) ||
      inspect_wav(candidate_instrumental, &frames_instrumental,
                  &offset_instrumental))
    return 0;
  if (frames_a != frames_vocal || frames_b != frames_instrumental)
    return 0;

  snprintf(vocal_a, vocal_a_capacity, "%s", candidate_vocal);
  snprintf(instrumental_b, instrumental_b_capacity, "%s",
           candidate_instrumental);
  return 1;
}
