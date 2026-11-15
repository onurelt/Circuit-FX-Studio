"""Exercise vocal processing, stems, transition scope and preview/export."""
import math
import os
import pathlib
import struct
import subprocess
import tempfile
import wave

TOOL = pathlib.Path(os.environ.get("VOCAL_TOOL",
    str(pathlib.Path(__file__).resolve().parent / "vocal_tool")))
RATE = 44100


def write(path, seconds, kind):
    with wave.open(str(path), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        one_second = bytearray()
        for i in range(RATE):
            t = i / RATE
            voice = int(8000 * math.sin(2 * math.pi * 440 * t))
            side = int(5000 * math.sin(2 * math.pi * 80 * t))
            l, r = ((voice + side, voice - side) if kind == "mix" else
                    (side, -side) if kind == "instrumental" else (voice, voice))
            one_second += struct.pack("<hh", l, r)
        for _ in range(seconds):
            f.writeframes(one_second)


def samples(path, start=0, count=4096):
    with wave.open(str(path), "rb") as f:
        f.setpos(start)
        return list(memoryview(f.readframes(count)).cast("h"))


def energy(samples, combine):
    return sum(combine(samples[i], samples[i + 1])**2
               for i in range(0, len(samples), 2))


with tempfile.TemporaryDirectory() as directory:
    p = pathlib.Path(directory)
    original, inst, vocals = (p / name for name in ("song.wav", "song.instrumental.wav", "song.vocals.wav"))
    write(original, 8, "mix")
    write(inst, 8, "instrumental")
    write(vocals, 8, "vocals")
    lite, stem, halfway, voice_sub, identity = (
        p / name for name in ("lite.wav", "stem.wav", "half.wav", "sub.wav", "zero.wav"))
    def run(source, output, instrumental="-", vocal="-", strength=100, env=None):
        return subprocess.run([str(TOOL), "reduce", str(source), str(output),
                               str(instrumental), str(vocal), str(strength)],
                              env=env, capture_output=True)

    assert run(original, lite).returncode == 0
    assert run(original, identity, strength=0).returncode == 0
    assert samples(original) == samples(identity)
    src, reduced = samples(original), samples(lite)
    assert energy(reduced, lambda l, r: l + r) < .18 * energy(src, lambda l, r: l + r)
    assert abs(energy(reduced, lambda l, r: l - r) /
               energy(src, lambda l, r: l - r) - 1) < .001
    assert run(original, stem, instrumental=inst).returncode == 0
    assert samples(stem) == samples(inst)
    assert run(original, halfway, instrumental=inst, strength=50).returncode == 0
    assert samples(halfway) != samples(stem)
    assert run(original, voice_sub, vocal=vocals).returncode == 0
    assert samples(voice_sub) == samples(inst)

    bad = p / "short.wav"
    write(bad, 7, "instrumental")
    rejected = p / "rejected.wav"
    assert run(original, rejected, instrumental=bad).returncode != 0
    assert not rejected.exists()
    cancelled = p / "cancelled.wav"
    assert run(original, cancelled, env=dict(os.environ, CANCEL_VOCAL="1")).returncode != 0
    assert not cancelled.exists()

    preview, export = p / "preview.wav", p / "export.wav"
    def render(out, preview_flag):
        subprocess.run([str(TOOL), "render", str(original), str(original),
                        str(lite), str(lite), str(out), "2", "2", "2",
                        str(preview_flag)], check=True)
    render(preview, 1)
    render(export, 0)
    with wave.open(str(preview), "rb") as f:
        rendered = f.readframes(f.getnframes())
    with wave.open(str(export), "rb") as f:
        assert f.readframes(f.getnframes()) == rendered
    assert samples(export, 0) == samples(original, 0)  # original prefix
    assert samples(export, 5 * RATE) == samples(original, 5 * RATE)  # original B tail
    assert samples(export, 3 * RATE) != samples(original, 3 * RATE)  # overlap changed

    beat_tool = pathlib.Path(__file__).resolve().parent / "beatmatch_tool"
    raw_match, vocal_match = p / "raw-match.wav", p / "vocal-match.wav"
    for source, output in ((original, raw_match), (lite, vocal_match)):
        subprocess.run([str(beat_tool), "stretch", str(source), str(output), "1.0666666667"],
                       check=True)
    joined = p / "matched-export.wav"
    subprocess.run([str(TOOL), "render", str(original), str(raw_match),
                    str(lite), str(vocal_match), str(joined), "2", "1.875", "2", "0"],
                   check=True)
    with wave.open(str(joined), "rb") as f:
        assert f.getnframes() > 7 * RATE

print("PASS: vocal, stems, scope, beatmatch, preview/export, cancellation and mismatches.")
