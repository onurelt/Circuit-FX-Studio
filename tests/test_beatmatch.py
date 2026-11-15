"""Exercise tempo analysis, pitch-preserving preparation, cancel and mixer."""
import math
import os
import pathlib
import struct
import subprocess
import tempfile
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "tests/beatmatch_tool"
RENDER = ROOT / "tests/render"
RATE = 44100


def song(path, bpm, invert_right=False):
    with wave.open(str(path), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        for second in range(16):
            data = bytearray()
            for i in range(RATE):
                t = second + i / RATE
                beat = (t * bpm / 60) % 1
                kick = (20000 * math.exp(-beat * 42) *
                        math.sin(2 * math.pi * 80 * beat * 60 / bpm)) if beat < .2 else 0
                sample = int(5500 * math.sin(2 * math.pi * 440 * t) + kick)
                data += struct.pack("<hh", sample, -sample if invert_right else sample)
            f.writeframes(data)


def analyze(path):
    text = subprocess.check_output([str(TOOL), "analyze", str(path)], text=True)
    return tuple(map(float, text.split()))


def tone_frequency(path):
    with wave.open(str(path), "rb") as f:
        f.setpos(int(6.2 * RATE))
        raw = f.readframes(int(.25 * RATE))
    samples = memoryview(raw).cast("h")[::2]
    best = (0, 0)
    for hz in range(420, 461):
        re = im = 0.0
        for i, value in enumerate(samples):
            phase = 2 * math.pi * hz * i / RATE
            re += value * math.cos(phase)
            im += value * math.sin(phase)
        energy = re * re + im * im
        if energy > best[0]:
            best = (energy, hz)
    return best[1]


with tempfile.TemporaryDirectory() as directory:
    temp = pathlib.Path(directory)
    a, b, matched = (temp / name for name in ("a.wav", "b.wav", "matched.wav"))
    song(a, 128)
    song(b, 120, invert_right=True)
    ba, phase_a, confidence_a = analyze(a)
    bb, phase_b, confidence_b = analyze(b)
    assert abs(ba - 128) < 1 and abs(bb - 120) < 1
    assert confidence_a > .15 and confidence_b > .15
    slower = temp / "slower.wav"
    song(slower, 95)
    assert abs(analyze(slower)[0] - 95) < 1
    silent = temp / "silent.wav"
    with wave.open(str(silent), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"\0" * 10 * RATE * 4)
    assert subprocess.run([str(TOOL), "analyze", str(silent)],
                          capture_output=True).returncode != 0
    ratio = ba / bb
    subprocess.run([str(TOOL), "stretch", str(b), str(matched), str(ratio)], check=True)
    with wave.open(str(matched), "rb") as f:
        assert f.getnchannels() == 2 and f.getframerate() == RATE
        assert abs(f.getnframes() - round(16 * RATE / ratio)) <= 1
    assert abs(tone_frequency(b) - tone_frequency(matched)) <= 2
    with wave.open(str(matched), "rb") as f:
        f.setpos(7 * RATE)
        stereo = memoryview(f.readframes(4096)).cast("h")
        assert all(stereo[i] == -stereo[i + 1] for i in range(0, len(stereo), 2))

    preview, full = temp / "preview.wav", temp / "full.wav"
    args = [str(a), str(matched), "4", str(4 / ratio), "2", "3"]
    subprocess.run([str(RENDER), args[0], args[1], str(preview), *args[2:], "1"], check=True)
    subprocess.run([str(RENDER), args[0], args[1], str(full), *args[2:], "0"], check=True)
    with wave.open(str(preview), "rb") as p, wave.open(str(full), "rb") as f:
        assert p.readframes(p.getnframes()) == f.readframes(p.getnframes())

    cancelled = temp / "cancel.wav"
    env = dict(os.environ, CANCEL_BEATMATCH="1")
    assert subprocess.run([str(TOOL), "stretch", str(b), str(cancelled), str(ratio)],
                          env=env, capture_output=True).returncode != 0
    assert not cancelled.exists()
    assert subprocess.run([str(TOOL), "stretch", str(b), str(cancelled), "1.5"],
                          capture_output=True).returncode != 0
    assert not cancelled.exists()

print("PASS: BPM/phase, matched duration, preserved 440 Hz pitch, preview/export identity and cancellation.")
