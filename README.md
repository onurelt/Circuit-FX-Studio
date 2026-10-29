# Circuit FX Studio — Beta 1 (0.6)
Standalone PSP WAV transition mixer. Built from the v0.5 streaming engine, with a song browser, safe exports, cancellation/progress and four transition curves.

Copy PSP to the root of your memory stick. Launch Circuit FX Studio from Game > Memory Stick on a homebrew-capable PSP. Put your own songs in PSP/GAME/CIRCUITFX/music/ (up to 512 files, no subfolders). Songs must be 44,100 Hz stereo signed 16-bit PCM WAV. MP3, float WAV, mono, 24-bit and other rates are not supported in this beta.

Use UP/DOWN to select a row and X to open it. LEFT/RIGHT changes transition and duration (1–120 seconds). Select two songs, then Preview. SQUARE replays it. Preview includes four seconds before and after the overlap. Export asks for explicit X confirmation and writes a new exports/MIX0001.WAV, incrementing the number. O cancels processing, deleting partial output. Originals are opened read-only. HOME exits.

The mix contains all of A and B, sharing the overlap: length = A + B - overlap. Four curves: linear, normalized sine (peak-safe), smooth, and dip to silence. This is not yet the planned 300-effect library; no beat detection, MP3, or separate intensity control yet. The beta uses duration as overlap. Export limit is 2GB. Budget roughly 150MB free for a 14-minute mix plus temporary preview space. Use the PSP's normal volume buttons.

The original v0.5 renderer and WAV writer informed this implementation. Processing uses fixed-size buffers, independent of song length. Normalized fades avoid the correlated-signal clipping of the former unnormalized equal-power fade. Input chunk bounds, format, truncation, frame alignment and output-size checks precede processing.

Validation: native host audio regression tests and PSPSDK compilation. PSP hardware behavior must still be verified on device; no hardware test is claimed.
