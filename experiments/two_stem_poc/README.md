# Offline two-stem Auto Mix — separate, non-destructive feasibility test

**Goal:** decide whether a real, pretrained **vocal + instrumental** separator
can ever run on the PS Vita for 10–30-second Auto Mix excerpts.

**Status: host reference experiment only. Not an AI Vita app.**

## Baseline model

Use the published **Spleeter two-stem ONNX model** via sherpa-onnx, with
pretrained weights from the sherpa-onnx release. Unlike a made-up DSP split,
this invokes actual neural inference. CPU tests on GitHub Actions provide a
reference RTF, memory measurement and both output WAVs, but GitHub's host
hardware is far more powerful than a PS Vita. The test exports *metrics only*
to Actions artifacts, not copyrighted audio.

To run with your own music, convert a copy to stereo 44.1kHz 16-bit PCM WAV
and invoke `run_host.py --input path/to/owned.wav --model-root models
--out-dir /tmp/stem-results --seconds 20`.

For the actual Vita, the hard research task is a **VitaSDK-compatible native
inference runtime** (e.g. a verified cross-compilation of required
sherpa-onnx/ONNX runtime kernels) within Vita application RAM and acceptable
offline execution time. A model being only ~40MB on disk does **not** imply
40MB runtime RAM. FP16 weights may offer little CPU benefit on Cortex-A9.
Int8 should also be benchmarked rather than assumed to work.

### Strict gates before connecting to Auto Mix

1. Complete genuine separation of a 20-second **real** music excerpt on Vita,
   with source-licensed pretrained weights (no center-channel subtraction).
2. Record peak memory, run time, output WAV durations and failure behavior on
   an unmodified real Vita. Do not claim to have tested on physical hardware
   if only GitHub Actions ran.
3. Listen to and compare vocals/instrumentals and their recombination to
   the original (including bleed/artifacts); prove audible quality.
4. Only then consider using prepared stems in Auto Mix. Existing working
   preview, export and GitHub releases must remain untouched.
5. If there is no compatible low-memory inference runtime, mark the native
   proof of concept infeasible; do not simulate AI or replace Auto Mix.

Reference docs:
- https://k2-fsa.github.io/sherpa/onnx/source-separation/models.html
- https://github.com/k2-fsa/sherpa-onnx/blob/master/python-api-examples/offline-source-separation-spleeter.py
