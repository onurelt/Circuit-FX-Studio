"""Check strict sidecar matching and Vocal Blend render/export routing."""
import math
import os
import pathlib
import struct
import subprocess
import tempfile
import wave

TOOL = pathlib.Path(os.environ["PREPARED_STEMS_TOOL"])
RENDER = pathlib.Path(os.environ["VOCAL_TOOL"])
RATE = 44100


def write(path, seconds, tones):
    one_second = bytearray()
    for i in range(RATE):
        t = i / RATE
        value = sum(int(amplitude * math.sin(2 * math.pi * hz * t))
                    for hz, amplitude in tones)
        value = max(-32768, min(32767, value))
        one_second += struct.pack("<hh", value, value)
    with wave.open(str(path), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        for _ in range(seconds):
            f.writeframesraw(one_second)


def inspect(track_a, pcm_a, track_b, pcm_b):
    result = subprocess.run([str(TOOL), str(track_a), str(pcm_a),
                             str(track_b), str(pcm_b)],
                            check=True, capture_output=True, text=True)
    return result.stdout.strip().splitlines()


def channel(path, start, count):
    with wave.open(str(path), "rb") as f:
        f.setpos(start)
        raw = f.readframes(count)
    return list(struct.unpack("<" + "h" * (len(raw) // 2), raw)[::2])


def projection(values, hz, first_frame):
    numerator = denominator = 0.0
    for i, sample in enumerate(values):
        basis = math.sin(2 * math.pi * hz * (first_frame + i) / RATE)
        numerator += sample * basis
        denominator += basis * basis
    return numerator / denominator


with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    music = root / "music"
    music.mkdir()
    track_a = music / "Artist.Live.mp3"
    track_b = music / "Incoming.Set.mp3"
    track_a.touch()
    track_b.touch()
    pcm_a = root / "decoded-a.wav"
    pcm_b = root / "decoded-b.wav"
    vocal_a = music / "Artist.Live.vocals.wav"
    instrumental_b = music / "Incoming.Set.instrumental.wav"

    write(pcm_a, 8, [(440, 9000), (110, 3000)])
    write(pcm_b, 8, [(330, 6000), (80, 6000)])
    write(vocal_a, 8, [(440, 9000)])
    write(instrumental_b, 8, [(80, 6000)])
    assert inspect(track_a, pcm_a, track_b, pcm_b) == [
        "1", str(vocal_a), str(instrumental_b)]

    # Missing, malformed and wrong-length sidecars must choose the safe fallback.
    write(instrumental_b, 7, [(80, 6000)])
    assert inspect(track_a, pcm_a, track_b, pcm_b) == ["0"]
    write(instrumental_b, 8, [(80, 6000)])
    vocal_a.unlink()
    assert inspect(track_a, pcm_a, track_b, pcm_b) == ["0"]
    vocal_a.write_bytes(b"not a WAV")
    assert inspect(track_a, pcm_a, track_b, pcm_b) == ["0"]
    write(vocal_a, 8, [(440, 9000)])

    # The renderer substitutes outgoing vocals and incoming instrumental
    # only during overlap. Preview and export use identical sample routing.
    preview, export = root / "preview.wav", root / "export.wav"
    def render(output, preview_flag):
        subprocess.run([
            str(RENDER), "render", str(pcm_a), str(pcm_b),
            str(vocal_a), str(instrumental_b), str(output),
            "3", "3", "2", str(preview_flag)], check=True)
    render(preview, 1)
    render(export, 0)
    with wave.open(str(preview), "rb") as f:
        preview_pcm = f.readframes(f.getnframes())
        preview_frames = f.getnframes()
    with wave.open(str(export), "rb") as f:
        export_pcm = f.readframes(f.getnframes())
        export_frames = f.getnframes()
    assert preview_frames == export_frames == 8 * RATE
    assert preview_pcm == export_pcm

    section_start = int(3.5 * RATE)
    overlap = channel(export, section_start, RATE)
    assert projection(overlap, 440, section_start) > 2500
    assert projection(overlap, 80, section_start) > 1400
    assert abs(projection(overlap, 110, section_start)) < 350
    assert abs(projection(overlap, 330, section_start)) < 350

    # The pre-transition prefix and post-transition tail remain the originals.
    prefix = channel(export, 0, 512)
    original_prefix = channel(pcm_a, 0, 512)
    tail = channel(export, 5 * RATE, 512)
    original_tail = channel(pcm_b, 5 * RATE, 512)
    assert prefix == original_prefix
    assert tail == original_tail

print("PASS: aligned sidecar detection, fallback validation, vocal handoff, and preview/export identity.")
