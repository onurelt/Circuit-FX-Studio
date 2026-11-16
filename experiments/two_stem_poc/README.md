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

## Measured host results — 2026-11-16 (NOT Vita measurements)

All three runs successfully executed pretrained neural inference and generated
two WAV stems from the same original **synthetic** music test. Results are
specific to GitHub-hosted Linux runners (Python + sherpa-onnx + ONNX Runtime).

| Model | Clip | Peak RSS | Elapsed | RTF |
|---|---:|---:|---:|---:|
| FP16 two-stem | 20 s | 625.7 MiB | 1.278 s | 0.064 |
| INT8 two-stem | 20 s | 615.2 MiB | 1.015 s | 0.051 |
| INT8 two-stem | 5 s | 361.7 MiB | 0.727 s | 0.145 |

GitHub Actions logs:
- https://github.com/onurelt/Circuit-FX-Studio/actions/runs/43957965142
- https://github.com/onurelt/Circuit-FX-Studio/actions/runs/43958163499

**Interpretation:** these memory figures exceed the Vita's ordinary 256 MiB
user-memory budget, even for a 5-second INT8 clip. The 5-second run nearly
fills the maximum extended 365 MiB main partition *without any UI/audio
engine*. Linux peak RSS includes Python and desktop runtime overhead and
cannot be directly equated to native Vita RAM; however, there is no evidence
this Spleeter runtime fits. Large model activation buffers dominate; smaller
weights alone are insufficient.

**Audio-quality caveat:** the INT8 outputs had unusually large individual
RMS relative to the input while reconstruction was relatively close, so it
requires careful listening/level assessment using real licensed music before
any production use. This test did **not** evaluate true vocal fidelity.

**Decision:** do NOT merge this AI implementation into Vita Auto Mix.
Next, search for *smaller-activation pretrained* architectures with usable
weights and a demonstrable VitaSDK-compatible inference runtime. Do not
treat a paper's parameter count alone as runtime feasibility. The main
Auto Mix branch is unchanged.
