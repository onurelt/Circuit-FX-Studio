#ifndef CIRCUIT_MP3_IMPORT_H
#define CIRCUIT_MP3_IMPORT_H
#include <stdint.h>

typedef int (*Mp3ImportProgress)(uint32_t done_frames,
                                 uint32_t total_frames);
extern char mp3_import_error[160];

/* Decode to a temporary 44.1 kHz, stereo, signed 16-bit PCM WAV. Refuses to
 * overwrite an existing destination and removes partial output on failure. */
int mp3_import_to_wav(const char *input_path, const char *output_path,
                      Mp3ImportProgress progress);
/* Inspect duration without materializing PCM. Full-song peaks are decoded
 * incrementally into a small envelope and can be cancelled by the caller. */
int mp3_probe(const char *input_path, uint32_t *target_frames);
int mp3_waveform_peaks(const char *input_path, uint16_t *peaks, unsigned bins,
                       Mp3ImportProgress progress);

#endif
