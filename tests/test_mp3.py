"""Exercise the bundled MP3 decoder and feed its WAV output to the mixer."""
import os
import pathlib
import subprocess
import tempfile
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
IMPORT = pathlib.Path(os.environ.get("MP3_IMPORT_EXE", ROOT / "tests/mp3_convert"))
RENDER = pathlib.Path(os.environ.get("RENDER_EXE", ROOT / "tests/render"))
FIXTURES = ROOT / "tests/fixtures"

with tempfile.TemporaryDirectory() as tmp:
    work = pathlib.Path(tmp)
    converted = []
    for name in ["test-stereo-44100.mp3", "test-mono-22050.mp3",
                 "test-mono-8000.mp3", "test-stereo-48000.mp3"]:
        source = FIXTURES / name
        output = work / (name + ".wav")
        subprocess.run([str(IMPORT), str(source), str(output)], check=True)
        with wave.open(str(output), "rb") as f:
            assert (f.getframerate(), f.getnchannels(), f.getsampwidth()) == (44100, 2, 2)
            assert abs(f.getnframes() / f.getframerate() - .7) < .02
            pcm = f.readframes(f.getnframes())
        assert max(abs(x) for x in memoryview(pcm).cast("h")) > 1000
        if "mono" in name:
            samples = memoryview(pcm).cast("h")
            assert all(samples[i] == samples[i + 1] for i in range(0, len(samples), 2))
        converted.append(output)

    # Converted files must work with the existing exact-cue mixer and WAV export.
    mix = work / "mix.wav"
    subprocess.run([str(RENDER), str(converted[0]), str(converted[1]), str(mix),
                    "0", "0", ".2", "1", "0"], check=True)
    with wave.open(str(mix), "rb") as f:
        assert (f.getframerate(), f.getnchannels(), f.getsampwidth()) == (44100, 2, 2)
        assert f.getnframes() == round(.7 * 44100)

    # Damaged input, cancellation and overwrite protection must not leave or
    # destroy cache files.
    bad = work / "damaged.mp3"
    bad.write_bytes(bytes(range(256)) * 4)
    bad_out = work / "bad.wav"
    assert subprocess.run([str(IMPORT), str(bad), str(bad_out)]).returncode != 0
    assert not bad_out.exists()
    cancel_out = work / "cancel.wav"
    env = dict(os.environ, CANCEL_MP3_IMPORT="1")
    assert subprocess.run([str(IMPORT), str(FIXTURES / "test-stereo-44100.mp3"),
                           str(cancel_out)], env=env).returncode != 0
    assert not cancel_out.exists()
    protected = work / "protected.wav"
    protected.write_bytes(b"keep me")
    assert subprocess.run([str(IMPORT), str(FIXTURES / "test-stereo-44100.mp3"),
                           str(protected)]).returncode != 0
    assert protected.read_bytes() == b"keep me"

print("PASS: MP3 decode, mono/stereo, 8–48 kHz resampling, mixer/export, damaged input, cancel and overwrite protection.")
