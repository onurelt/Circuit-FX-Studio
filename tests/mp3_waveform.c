#include "mp3_import.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int cancel(uint32_t done, uint32_t total) {
  (void)done; (void)total;
  return 1;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  uint32_t frames = 0;
  uint16_t peaks[8192];
  assert(mp3_probe(argv[1], &frames) == 0);
  assert(frames > 1000);
  assert(mp3_waveform_peaks(argv[1], peaks, 8192, NULL) == 0);
  int heard = 0;
  for (int i = 0; i < 8192; i++) heard |= peaks[i] > 100;
  assert(heard);
  assert(mp3_waveform_peaks(argv[1], peaks, 8192, cancel) != 0);
  puts("PASS: MP3 duration probe, streaming envelope and cancellation.");
  return 0;
}
