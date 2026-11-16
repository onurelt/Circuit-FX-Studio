"""Validate aligned prepared-stem discovery, mixer levels, clipping and cancel."""
import math
import os
import pathlib
import struct
import subprocess
import tempfile
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = pathlib.Path(os.environ.get("STEM_MIX_TOOL", ROOT / "tests/stem_mix_tool"))
LIST = pathlib.Path(os.environ.get("STEM_LIST_TOOL", ROOT / "tests/stem_list_tool"))
RATE = 44100


def write(path, seconds, hz=440, amplitude=3000, phase=0):
    frames = int(seconds * RATE)
    with wave.open(str(path), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        block = bytearray()
        for i in range(frames):
            sample = int(amplitude * math.sin(2 * math.pi * hz * i / RATE + phase))
            block += struct.pack("<hh", sample, sample)
        f.writeframes(block)
    return frames


def run_mix(out, paths, mask=15, gains=(1, 1, 1, 1), cancel_at=0, ok=True):
    args = [str(TOOL), str(out), str(mask), str(cancel_at),
            *(str(x) for x in gains), *(str(x) for x in paths)]
    result = subprocess.run(args, capture_output=True, text=True)
    if ok:
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0, result.stderr
    return result


with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    source = root / "mix.wav"
    frames = write(source, 1.0)
    names = ("vocals", "drums", "bass", "other")
    paths = [root / f"mix.{name}.wav" for name in names]
    for i, path in enumerate(paths):
        write(path, 1.0, 440 + i * 100, 3000, i * .4)

    # All supplied stems are enumerated; names and frame count are checked.
    listed = subprocess.check_output([str(LIST), str(root / "mix.mp3"),
                                      str(source)], text=True).splitlines()
    assert int(listed[0]) == 15
    assert [pathlib.Path(x).name for x in listed[1:]] == [p.name for p in paths]

    # A selected stem at 0.5 gain renders deterministically and preserves frames.
    one, one_copy = root / "one.wav", root / "one-copy.wav"
    run_mix(one, paths, mask=1, gains=(.5, 1, 1, 1))
    run_mix(one_copy, paths, mask=1, gains=(.5, 1, 1, 1))
    assert one.read_bytes() == one_copy.read_bytes()
    with wave.open(str(one), "rb") as f:
        assert f.getnframes() == frames
        f.setpos(250)
        output_sample = struct.unpack("<hh", f.readframes(1))[0]
    with wave.open(str(paths[0]), "rb") as f:
        f.setpos(250)
        source_sample = struct.unpack("<hh", f.readframes(1))[0]
    assert abs(output_sample - round(source_sample * .5)) <= 1

    # Four high-gain stems cannot clip; the global guard only attenuates peaks.
    loud = [root / f"loud{i}.wav" for i in range(4)]
    for path in loud:
        write(path, .25, 110, 15000)
    clipped = root / "safe.wav"
    run_mix(clipped, loud, gains=(2, 2, 2, 2))
    with wave.open(str(clipped), "rb") as f:
        pcm = memoryview(f.readframes(f.getnframes())).cast("h")
        assert max(abs(x) for x in pcm) <= 30000
        assert max(abs(x) for x in pcm) > 1000

    # Misaligned inputs and cancellation are rejected, and partial files removed.
    short = root / "short.wav"
    write(short, .5)
    bad = root / "bad.wav"
    run_mix(bad, [paths[0], short, paths[2], paths[3]], mask=3, ok=False)
    assert not bad.exists()

    cancelled = root / "cancelled.wav"
    run_mix(cancelled, paths, cancel_at=frames + 256, ok=False)
    assert not cancelled.exists()

    # Discovery skips malformed or length-mismatched sidecars independently.
    (root / "mix.drums.wav").write_bytes(b"not a wav")
    write(root / "mix.bass.wav", .5)
    listed = subprocess.check_output([str(LIST), str(root / "mix.mp3"),
                                      str(source)], text=True).splitlines()
    assert int(listed[0]) == 9  # vocals and other only
    assert pathlib.Path(listed[1]).name == "mix.vocals.wav"
    assert listed[2] == ""
    assert listed[3] == ""
    assert pathlib.Path(listed[4]).name == "mix.other.wav"

print("PASS: prepared stem alignment, deterministic gain, clipping guard, and cancellation.")
