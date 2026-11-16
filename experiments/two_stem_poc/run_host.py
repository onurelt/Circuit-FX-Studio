#!/usr/bin/env python3
"""Measure REAL two-stem model inference on a host CPU, not on PS Vita.

The ONNX model weights are downloaded separately and not bundled in the repo.
Do not call this a Vita implementation: porting its runtime remains unproven.
"""
import argparse
import json
import math
import resource
import time
from pathlib import Path

import numpy as np
import soundfile as sf
import sherpa_onnx


def original_demo(path: Path, seconds: int) -> None:
    """Original synthetic test audio; no commercial recordings included."""
    rate = 44100
    t = np.arange(rate * seconds, dtype=np.float64) / rate
    bpm = 128.0
    beat = np.mod(t * bpm / 60.0, 1.0)
    kick = 0.25 * np.sin(2 * np.pi * (50 * t + 0.25 * t * t)) * np.exp(-32 * beat)
    bass = 0.12 * np.sin(2 * np.pi * 110 * t) * (beat < 0.55)
    # Vowel-like modulated harmonics stand in for a lead signal; real-vocal
    # separation quality must be evaluated using licensed/owned real music.
    lead = 0.12 * (np.sin(2*np.pi*(220*t+2*np.sin(2*np.pi*0.25*t))) +
                   0.25 * np.sin(2*np.pi*440*t)) * (0.6 + 0.4*np.sin(2*np.pi*0.8*t)**2)
    signal = np.clip(kick + bass + lead, -0.97, 0.97).astype(np.float32)
    stereo = np.stack((signal, signal * 0.94), axis=1)
    sf.write(str(path), stereo, rate, subtype="PCM_16")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model-root", type=Path, required=True)
    parser.add_argument("--input", type=Path, default=None,
                        help="Optional user-owned 44.1 kHz stereo WAV; never publish it to CI artifacts.")
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=20)
    args = parser.parse_args()
    if not 5 <= args.seconds <= 30:
        parser.error("--seconds must be 5-30")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    vocal_models = list(args.model_root.rglob("vocals.*.onnx"))
    accomp_models = list(args.model_root.rglob("accompaniment.*.onnx"))
    if len(vocal_models) != 1 or len(accomp_models) != 1:
        raise SystemExit("Need exactly one pretrained Spleeter ONNX vocal and accompaniment model.")
    path = args.input or args.out_dir / "original_synthetic.wav"
    if args.input is None:
        original_demo(path, args.seconds)
    samples, sample_rate = sf.read(str(path), dtype="float32", always_2d=True)
    if sample_rate != 44100 or samples.shape[1] != 2:
        raise SystemExit("Input must be 44.1 kHz stereo WAV.")
    samples = samples[:sample_rate * args.seconds, :]
    if len(samples) < 5 * sample_rate:
        raise SystemExit("Input must contain at least five seconds.")
    audio = np.ascontiguousarray(samples.T)
    config = sherpa_onnx.OfflineSourceSeparationConfig(
        model=sherpa_onnx.OfflineSourceSeparationModelConfig(
            spleeter=sherpa_onnx.OfflineSourceSeparationSpleeterModelConfig(
                vocals=str(vocal_models[0]),
                accompaniment=str(accomp_models[0]),
            ),
            num_threads=1,
            debug=False,
            provider="cpu",
        )
    )
    if not config.validate():
        raise SystemExit("Spleeter configuration was invalid.")
    started = time.monotonic()
    separator = sherpa_onnx.OfflineSourceSeparation(config)
    result = separator.process(sample_rate=sample_rate, samples=audio)
    elapsed = time.monotonic() - started
    if len(result.stems) != 2:
        raise RuntimeError("Did not produce exactly two real model output stems.")
    stems = [np.asarray(s.data, dtype=np.float32).T for s in result.stems]
    if any(s.ndim != 2 or s.shape[1] != 2 or
           abs(len(s) - len(samples)) > 8192 or
           not np.isfinite(s).all() for s in stems):
        raise RuntimeError("Incorrect/non-finite model audio output.")
    for name, stem in zip(("vocals", "instrumental"), stems):
        sf.write(str(args.out_dir / (name + ".wav")), stem, result.sample_rate,
                 subtype="PCM_16")
    duration = len(samples) / sample_rate
    common = min(len(samples), len(stems[0]), len(stems[1]))
    rms = lambda x: math.sqrt(float(np.mean(np.asarray(x, dtype=np.float64)**2)))
    metrics = {
        "model": "sherpa-onnx Spleeter 2stem pretrained CPU (NOT PS Vita)",
        "audio_seconds": round(duration, 3),
        "elapsed_seconds": round(elapsed, 3),
        "real_time_factor": round(elapsed / duration, 3),
        "peak_rss_mib_linux": round(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024, 1),
        "vocal_rms": round(rms(stems[0]), 6),
        "instrumental_rms": round(rms(stems[1]), 6),
        "reconstruction_error_ratio": round(
            rms(stems[0][:common] + stems[1][:common] - samples[:common]) /
            max(1e-9, rms(samples[:common])), 5),
        "tested_on_vita": False,
        "quality_verified_with_real_vocals": False,
    }
    (args.out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n")
    print(json.dumps(metrics, indent=2))
    print("Model inference executed. Vita feasibility NOT demonstrated.")


if __name__ == "__main__":
    main()
