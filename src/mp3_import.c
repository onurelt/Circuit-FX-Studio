#include "mp3_import.h"
#include "wav.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define OUTPUT_RATE 44100u
#define INPUT_FRAMES 4096u
#define OUTPUT_FRAMES 1024u
#define MAX_MP3_BYTES (64u * 1024u * 1024u)
char mp3_import_error[160];

static size_t mp3_read(void *buf, size_t bytes, void *user_data) {
  return fread(buf, 1, bytes, (FILE *)user_data);
}
static int mp3_seek(uint64_t position, void *user_data) {
  if (position > LONG_MAX)
    return -1;
  return fseek((FILE *)user_data, (long)position, SEEK_SET);
}
static int fail(const char *message) {
  snprintf(mp3_import_error, sizeof(mp3_import_error), "%s", message);
  return -1;
}
static int16_t clamp_sample(float x) {
  x = fmaxf(-32768.f, fminf(32767.f, x));
  return (int16_t)lrintf(x);
}

int mp3_import_to_wav(const char *input_path, const char *output_path,
                      Mp3ImportProgress progress) {
  FILE *input = NULL, *output = NULL;
  mp3dec_ex_t decoder = {0};
  mp3dec_io_t io = {0};
  int result = -1, decoder_open = 0, output_created = 0;
  mp3_import_error[0] = 0;
  if (!input_path || !output_path || !strcmp(input_path, output_path))
    return fail("Invalid MP3 input or cache path.");
  input = fopen(input_path, "rb");
  if (!input)
    return fail("Cannot open MP3 file.");
  if (fseek(input, 0, SEEK_END)) {
    fail("Cannot seek MP3 file.");
    goto done;
  }
  long file_bytes = ftell(input);
  if (file_bytes < 32 || (unsigned long)file_bytes > MAX_MP3_BYTES) {
    fail("MP3 must be between 32 bytes and 64 MB.");
    goto done;
  }
  if (fseek(input, 0, SEEK_SET)) {
    fail("Cannot rewind MP3 file.");
    goto done;
  }
  FILE *existing = fopen(output_path, "rb");
  if (existing) {
    fclose(existing);
    fail("MP3 cache already exists; choose a fresh path.");
    goto done;
  }
  io.read = mp3_read;
  io.read_data = input;
  io.seek = mp3_seek;
  io.seek_data = input;
  int open_result = mp3dec_ex_open_cb(&decoder, &io, MP3D_SEEK_TO_SAMPLE);
  if (open_result) {
    mp3dec_ex_close(&decoder);
    fail("Cannot decode this MP3. Check that it is complete and supported.");
    goto done;
  }
  decoder_open = 1;
  unsigned channels = decoder.info.channels > 0 ? decoder.info.channels : 0;
  unsigned source_rate = decoder.info.hz > 0 ? decoder.info.hz : 0;
  uint64_t source_frames = channels ? decoder.samples / channels : 0;
  if (!channels || channels > 2 || source_rate < 8000 || source_rate > 48000 ||
      decoder.info.layer != 3 || !source_frames) {
    fail("Use a valid mono or stereo MP3 (8–48 kHz).");
    goto done;
  }
  uint64_t target64 =
      (uint64_t)llround((double)source_frames * OUTPUT_RATE / source_rate);
  if (!target64 || target64 > (INT32_MAX - 44u) / 4u) {
    fail("Decoded song is too long for a WAV export.");
    goto done;
  }
  uint32_t target_frames = (uint32_t)target64;
  output = fopen(output_path, "wb");
  if (!output) {
    fail("Cannot create MP3 conversion cache. Check free storage.");
    goto done;
  }
  output_created = 1;
  if (wav_begin(output, OUTPUT_RATE)) {
    fail("Cannot create MP3 conversion cache. Check free storage.");
    goto done;
  }

  mp3d_sample_t input_pcm[INPUT_FRAMES * 2];
  int16_t output_pcm[OUTPUT_FRAMES * 2];
  uint64_t window_start = 0, output_at = 0;
  size_t buffered_frames = 0, output_used = 0;
  int eof = 0;
  const double step = (double)source_rate / OUTPUT_RATE;
  while (output_at < target_frames) {
    if (!eof && buffered_frames < INPUT_FRAMES) {
      size_t room = (INPUT_FRAMES - buffered_frames) * channels;
      size_t got = mp3dec_ex_read(&decoder,
                                  input_pcm + buffered_frames * channels, room);
      if (got % channels) {
        fail("MP3 decoder returned an incomplete audio frame.");
        goto done;
      }
      buffered_frames += got / channels;
      if (!got) {
        eof = 1;
        if (decoder.last_error) {
          fail("MP3 decoding stopped on a damaged frame.");
          goto done;
        }
      }
    }
    if (!buffered_frames) {
      fail("MP3 ended before its reported duration.");
      goto done;
    }
    uint64_t window_end = window_start + buffered_frames;
    int emitted = 0;
    while (output_at < target_frames) {
      double source_position = output_at * step;
      if (source_position > source_frames - 1)
        source_position = (double)(source_frames - 1);
      uint64_t source_index = (uint64_t)source_position;
      if (source_index < window_start) {
        fail("MP3 resampler lost its input position.");
        goto done;
      }
      if (source_index + 1 >= window_end && !eof)
        break;
      if (source_index >= window_end) {
        fail("MP3 ended before its reported duration.");
        goto done;
      }
      size_t i0 = (size_t)(source_index - window_start);
      size_t i1 = i0 + 1 < buffered_frames ? i0 + 1 : i0;
      float fraction = (float)(source_position - source_index);
      for (unsigned channel = 0; channel < 2; channel++) {
        unsigned source_channel = channels == 1 ? 0 : channel;
        float a = input_pcm[i0 * channels + source_channel];
        float b = input_pcm[i1 * channels + source_channel];
        output_pcm[output_used * 2 + channel] =
            clamp_sample(a + (b - a) * fraction);
      }
      output_used++;
      output_at++;
      emitted = 1;
      if (output_used == OUTPUT_FRAMES) {
        if (wav_append(output, output_pcm, (uint32_t)output_used)) {
          fail("Writing decoded WAV failed. Check free storage.");
          goto done;
        }
        output_used = 0;
        if (progress && progress((uint32_t)output_at, target_frames)) {
          fail("MP3 conversion cancelled.");
          goto done;
        }
      }
    }
    if (output_at >= target_frames)
      break;
    if (!eof && buffered_frames) {
      size_t keep = buffered_frames - 1;
      if (!keep && !emitted) {
        fail("MP3 decoder made no progress.");
        goto done;
      }
      input_pcm[0] = input_pcm[keep * channels];
      if (channels == 2)
        input_pcm[1] = input_pcm[keep * channels + 1];
      window_start += keep;
      buffered_frames = 1;
    }
  }
  if (output_used && wav_append(output, output_pcm, (uint32_t)output_used)) {
    fail("Writing decoded WAV failed. Check free storage.");
    goto done;
  }
  if (progress && progress(target_frames, target_frames)) {
    fail("MP3 conversion cancelled.");
    goto done;
  }
  if (wav_finish(output)) {
    fail("Could not finish decoded WAV. Check free storage.");
    goto done;
  }
  result = 0;
done:
  if (output && fclose(output) && result == 0) {
    result = -1;
    fail("Could not close decoded WAV.");
  }
  if (decoder_open)
    mp3dec_ex_close(&decoder);
  if (input)
    fclose(input);
  if (result && output_created)
    remove(output_path);
  return result;
}
