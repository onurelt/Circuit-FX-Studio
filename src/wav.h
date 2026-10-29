#ifndef CIRCUIT_WAV_H
#define CIRCUIT_WAV_H
#include <stdint.h>
#include <stdio.h>
/* Creates a stereo 16-bit PCM WAV and writes samples in streaming chunks. */
int wav_begin(FILE *f, uint32_t sample_rate);
int wav_append(FILE *f, const int16_t *samples, uint32_t frames);
int wav_finish(FILE *f);
#endif
