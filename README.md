# Circuit FX Studio — native PS Vita Beta 1

A standalone VitaSDK DJ transition editor for **960×544**. Replaces the PSP target. No PC, server, Adrenaline, overclock, or extra runtime plugins are needed for mixing.

## Install

1. Copy `CircuitFX-Beta1.vpk` to your homebrew-enabled Vita and install with VitaShell.
2. Launch once to create the folders, then exit with Triangle.
3. Put songs in `ux0:data/CircuitFX/music/` using VitaShell USB or FTP.
4. Launch Circuit FX Studio. Select Track A and Track B.

**Input:** 44,100 Hz, stereo, signed 16-bit PCM `.wav`. Up to 512 files, directly inside the music folder. MP3, mono, 48 kHz, float and 24-bit WAV are not supported in this beta. Unsupported files show an error rather than playing incorrectly. Non-ASCII filename characters display as `?` but paths retain original bytes.

Optional conversion on a computer: `ffmpeg -i input.mp3 -ar 44100 -ac 2 -c:a pcm_s16le output.wav`.

## Make a mix

- D-pad up/down: select a control. X: open a song picker, toggle an effect, preview, or export.
- D-pad left/right on A cue / B cue / duration: adjust by **0.1 second**. Hold **L** for **1 millisecond**, **R** for **10 seconds**. Holding a direction repeats.
- Touch a deck, effect or action to activate it; touch a time field to focus it and adjust with D-pad. The file picker uses D-pad/X.
- START: render and play a preview. Square: replay the current preview.
- Circle: cancel rendering, stop playback, close a picker, or cancel export confirmation.
- SELECT: refresh the song list. Triangle: cleanly exit from the dashboard.
- Vita hardware volume buttons control listening volume.

The **A cue** is where the transition starts in the outgoing song. The **B cue** is where the incoming song begins. Both cues plus duration must fit within their songs. Duration: **0.100–120.000 seconds**. Cue positions map to the nearest audio frame; duration maps to complete frames.

Export consists of **A [0 → A cue], then the overlap, then B [B cue + duration → end]**. A after the overlap and B before its cue are intentionally omitted. Preview contains up to four seconds before and after the transition and uses the same renderer/settings as export. Any settings change invalidates the old preview. Preview is required before export; export asks for X confirmation.

## Five stackable effects

| Effect | Beta 1 behavior |
|---|---|
| Crossfade | Linear complementary A/B gain envelope. |
| Echo Out | 250 ms delay on A, 35% feedback, wet level grows to 30% during overlap. |
| Filter Sweep | A low-pass sweep from 18 kHz toward 220 Hz, blended in over the transition. |
| Bass Swap | Low-frequency separation at approximately 150 Hz; progressively removes A bass and introduces B bass. |
| Quick Cut | Switches A to B at overlap midpoint with a short 20 ms click-reduction ramp. Overrides the crossfade envelope while other effects remain active. |

Toggle any combination. With Crossfade off, a short midpoint handoff is used. Effect parameters are fixed in Beta 1; all processing is baked into preview/export. Final samples are saturated to prevent integer wraparound. Echo tails end with the overlap.

Exports are numbered `ux0:data/CircuitFX/exports/MIX0001.WAV`, etc. Existing files and source songs are not overwritten. Cancellation and failed writes remove partial output. Outputs are capped below 2 GiB. Budget about **180 MB free** for a 14-minute mix plus the longest preview. Rendering streams fixed-size buffers, so RAM does not grow with song length. The temporary preview is removed on clean exit and on next launch after a forced close.

## Build and validation

GitHub Actions runs the host audio regression suite, sanitizer checks, UI compilation, VitaSDK build and VPK structure checks on every push. Download the `Circuit-FX-Studio-Vita-Beta1` artifact from a successful run. The workflow pins the SDK archive and SHA-256; no third-party graphics/audio libraries are needed.

Local Vita build:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build
cmake --build build --parallel 2
python3 tests/check_vpk.py build/CircuitFX-Beta1.vpk
```

Host tests:

```sh
gcc -std=c99 -O2 -Wall -Wextra -Werror -DHOST_TEST src/render.c src/wav.c -lm -o tests/render
python3 tests/test_audio.py
```

The real UI can also render on a host with `gcc -O2 -DHOST_UI -Isrc vita/ui.c -o /tmp/ui && /tmp/ui` (writes `ui-preview.ppm`). Font raster data is bundled; regeneration needs Pillow and Open Sans. See `assets/FONT-LICENSE.txt`.

**Not included:** MP3 decoding, automatic tempo/beat matching, BPM detection, variable effect strengths, saved presets, album artwork, a draggable timeline, or background/locked-screen playback. Waveform strips show sampled song peaks, not a zoomable editing timeline. The reference image informs the dark cyan/magenta dashboard, but its extra controls are not shown as fake features.

This is a compiled, host-tested **hardware-test beta**, not a claim of verified playback on a physical Vita. See [HARDWARE_TESTING.md](HARDWARE_TESTING.md). Diagnostic messages are recorded in `ux0:data/CircuitFX/session.log` (reset on launch).
