# Offline stem-separation feasibility

## Current result

No neural separator is integrated into the production Vita app. No vocals, drums, bass, or instrument stems are synthesized or labeled as real. Auto Mix can optionally consume externally prepared, sample-aligned `TrackA.vocals.wav` and `TrackB.instrumental.wav` sidecars for its Vocal Blend overlap. Both must be 44.1 kHz, stereo, 16-bit PCM and exactly match the corresponding decoded track length. This does not provide AI separation. The one-song Vocal Remover still lacks stem import, per-stem controls, and stem export.

The existing Spleeter two-stem experiment was a Linux-host reference only. Recorded peak RSS was approximately 626 MiB for FP16/20 seconds, 615 MiB for INT8/20 seconds, and 362 MiB for INT8/5 seconds. Those measurements are not Vita measurements and cannot be compared directly with Vita partitions, but they are too high to justify bundling the Python/sherpa-onnx path. Do not repeat those Spleeter runs.

## Smaller candidates reviewed

| Candidate | Published artifact | What it provides | Current assessment |
|---|---:|---|---|
| Open-Unmix UMX-HQ ONNX vocal model | 35,623,512 bytes | One vocals model; MIT provenance is stated by the model publisher | Smaller on disk than Spleeter, but a single model is not four stems. Activation RAM and Cortex-A9 runtime cost are unmeasured. |
| UVR MDX-Net KARA 2 ONNX | 52,786,726 bytes | Vocal/instrumental model | Real pretrained weights exist, but model file size does not establish working RAM, audio quality on the Vita, or runtime compatibility. |
| ONNX Runtime C API | Runtime, not a model | General graph inference | The repository's VitaSDK build does not include ONNX Runtime, and no VitaSDK cross-build or required operator set has been verified. |

References:

- [Open-Unmix vocal ONNX model card](https://huggingface.co/MixDirective/open-unmix-umxhq-vocals-onnx)
- [UVR MDX-Net KARA 2 ONNX model card](https://huggingface.co/musetric/uvr-mdxnet-kara2-onnx)
- [ONNX Runtime execution providers](https://onnxruntime.ai/docs/execution-providers/)
- [sherpa-onnx source-separation models](https://k2-fsa.github.io/sherpa/onnx/source-separation/models.html)

The model cards are useful for discovering weights and reported file sizes. They do not provide Vita working-set or runtime measurements. Four-stem separation is more demanding than a single vocal/instrumental model; using separate source models multiplies compute and can increase peak memory. Chunking can limit audio buffers, but it does not remove model weights or internal activations.

## Why no native inference proof of concept is included

Neither candidate has a verified VitaSDK-compatible runtime, operator set, peak working-set measurement, or Cortex-A9 benchmark in this repository. The current VitaSDK build does not include ONNX Runtime. A host-only neural inference run would not demonstrate offline Vita feasibility, and uncompiled model-specific code would not be a valid proof of concept. This milestone therefore does not add untested inference code or model weights.

A real proof of concept still needs all of these gates:

1. Pin a pretrained weight set with clear redistribution license and checksums.
2. Provide a native ARMv7/VitaSDK inference implementation for the model's operator set.
3. Measure peak Vita user memory and elapsed time over a 10–30 second excerpt at stock clocks.
4. Compare separated stems and reconstruction against legally usable real music.
5. Test cancellation and repeated use on physical Vita hardware.

Until those measurements pass, Auto Mix uses cached BPM/energy analysis and ordinary beatmatched crossfades. Vocal Blend is displayed as a fallback and does not claim to recognize or isolate vocals.
