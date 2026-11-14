# Vita Beta 1 hardware acceptance checklist

No physical Vita was available during development. Host tests exercise the same C audio renderer linked into the native VPK. Compilation and package checks do not verify hardware audio, shell behavior, suspend/resume or storage throughput.

1. Install with VitaShell. Confirm icon and LiveArea launch, readable 960×544 layout, D-pad focus and touch targets.
2. Use two 44.1 kHz/stereo/16-bit WAV songs. Verify browser names, durations, actual waveform peaks, millisecond editing and held-button repeat.
3. Preview Crossfade, each other effect individually, and all five together. Check pitch/speed, both headphone channels, no buffer buzzing/dropouts, and expected cues.
4. Stop with Circle and START; replay with Square 20 times. Confirm audio ports release and repeated preview does not hang.
5. Preview then export. Open exported WAV on a computer. Check duration = A cue + B length - B cue, transition positions, clean tails and matching preview audio.
6. Export two 7-minute songs with all effects. Measure render time. Confirm controls cancel promptly. Check subsequent launch succeeds.
7. Cancel preview/export at different progress points. No partial output should remain. Existing exports and input files must be unchanged.
8. Reject unsupported and damaged WAVs. Test long filenames, empty music folder and missing/removed songs.
9. Test low free space with expendable test files; verify errors and partial-file cleanup. Avoid disconnecting storage during writes.
10. Exit using Triangle while idle. Separately test PS → LiveArea → swipe close after playback and during rendering, then reopen this app and another game. This build uses normal app lifetime and no background-audio shell locks or custom process attributes.
11. Test PS button interruptions and power-button suspend/resume while previewing and exporting. Screen-locked/background playback is not supported or promised. Report any hangs; use Triangle from idle as the initial exit path.

When reporting a problem, include Vita model/firmware, source WAV format, selected cues/duration/effects, action immediately before failure, and `ux0:data/CircuitFX/session.log`. Copy the log before relaunching because startup resets it.

## Version 01.02 additions

- Tap A/B cue fields and duration. Enter 155.250, 48.125 and 15.000 seconds; Apply, cancel and delete digits. Confirm D-pad exact adjustment still works.
- Select each quick variation and check its effect toggles before previewing.
- Compare timeline cue/overlap positions with the exported audio. Source waveforms are sampled approximations, not a post-FX waveform.
- Repeatedly preview and stop using touch and buttons. Check the playhead animates while audio remains clear; playback now uses a dedicated worker.
- Browse more than ten songs using touch previous/next and verify the correct file is selected.

## Version 01.03 additions

- Browse all four banks, both pages, and apply presets by touch and D-pad. Verify cue positions are preserved and shortened durations are reported.
- Audition a simple blend, stereo echo, gate, roll, high-pass lift, and ring/noise sweep. Test 0%, 50%, 100% intensity and 60/128/200 effect BPM. BPM must not change source-track pitch or playback speed.
- Preview and export the same settings; compare the transition segment. Verify endpoints and stereo behavior, particularly alternating echoes and source rolls.
- Test a long 64-beat recipe with two long songs, cancel it, and replay/export afterward. Monitor for clipping, discontinuities, freezes and export speed.
- Repeatedly stop preview and close the app via LiveArea. All new DSP still renders offline before playback.

## Version 01.04 additions

- Install over 01.03 using VitaShell and confirm BETA 1.4. Open the WAV FX tab; both pages must list 16 included sounds without extra files in ux0:data.
- Without loading songs, audition all 16 with Square and touch. Stop/restart at least 20 times; verify both channels and clean exit afterward. Audition uses the WAV's parsed data offset and length, including non-44-byte headers.
- Apply each sound and preview/export with songs. Verify whooshes center in the overlap and risers end with it; test 0%, 50%, 100% intensity and 0.1/2/8/30-second durations.
- Toggle processing effects while a sampled preset is active. Verify preview and export include the sample and toggled processing; compare the rendered WAV on another device.
- Confirm auditioning does not overwrite an existing mix preview or alter its export-ready state. Return from WAV auditions to Square replay on the dashboard.
- Test PS/LiveArea close and suspend/resume after standalone sample auditions. No real hardware verification has been performed.

## Version 01.05 additions

- Confirm all eight library tabs respond to touch; both WAV tabs list 25 sounds total, with no empty or invalid rows. Browse the DJ Paulo and Honey Dijon banks and apply recipes.
- Audition each WAV at default speed; test reverse, speeds 0.5x, 1.0x, 1.5x, and 2.0x. Confirm audition playback duration and direction respond. Test repeated stop/replay.
- For several sounds, compare audition, transition preview and exported WAV at forward/reverse and slow/fast settings. Try all five core effect combinations with sampled FX, 0/50/100% level, 0.1-second overlap and longer overlaps.
- Check short riser overlaps use the riser's end so each resolves on the cut. Check whooshes/impacts/glitches stay centered in the overlap. Listen for clicks, audio-port release, latency and stutter.
- The renderer uses bounded sample memory and linear resampling. Confirm export speed and system responsiveness on Vita, especially 0.5x and 2.0x, and after many consecutive auditions.
- Install over 01.04 with VitaShell. Confirm VPK size, LiveArea launch and effect resources load from `app0:sfx`.

## Version 01.06 additions (historical)

- Install over 01.05. Copy MP3 and WAV songs into the music folder; refresh with SELECT and verify both extensions appear once and show the right track name.
- Load stereo 44.1 kHz MP3 and mono 22.05 kHz MP3. Confirm a conversion progress screen, correct duration, stereo output for mono input, visible peaks, exact cue editing, preview and WAV export.
- Try common 48 kHz and 32 kHz MP3 files, VBR MP3, large ID3 tags, a damaged MP3 and a file over the 64 MB compressed limit. Errors must leave the original file and the previously loaded deck intact.
- Cancel a long decode with Circle; select another track afterward. Confirm no `.part` file remains and mixing stays responsive once conversion ends.
- Watch free storage while loading two long MP3 tracks and exporting. Each decoded cache uses about 10.1 MiB per song-minute; confirm low-storage errors clean up partial WAVs.
- Re-select the same MP3 on one deck to confirm the in-session cache is reused. Switch that deck to WAV and confirm its old cache is freed.
- Exit with Triangle and verify decoded caches are removed. Separately force-close during conversion, relaunch, and verify startup removes stale cache files without touching originals.
- Compare the converted-cache preview with the exported mix and check long-track decoding time, audio quality, memory use and app responsiveness on real hardware.

## Version 01.07 performance and stability additions

- Install over 01.06, load a long MP3, exit with Triangle, relaunch, and select it again. The second load should log `TIMING mp3_cache_hit=1` and should not show a decode progress screen. Select the same MP3 on both decks and confirm the second selection reuses the same cached WAV.
- Repeat switching between at least 10 WAV/MP3 songs, including the same files on both decks. Confirm names, durations, cues, waveform peaks, preview and export remain correct. Repeated loads should report waveform cache hits.
- While importing a long MP3, move through physical buttons and tap the progress dialog to cancel. Retry the same MP3 and complete an import. Check that no `.part` file is left behind and no controls remain stuck.
- Test a malformed MP3 and an MP3 whose decoded output would exceed 512 MiB. The app should show an error, retain the prior deck, and remove partial files. Separately test nearly-full storage with expendable files; expect a clear write/cache error and a usable app after freeing space.
- Fill the conversion cache past 512 MiB using expendable MP3s. Confirm oldest unpinned entries are evicted, selected songs keep playing, and restart prunes the cache back under the limit when possible.
- Change cues/effects repeatedly while watching `session.log`. `TIMING timeline_ms` should stay small and cue/effect edits must not trigger waveform-file reads. Dashboard idle should not increment draw calls continuously; navigating should redraw promptly.
- Verify `session.log` includes scan, song selection, import/cache, waveform, timeline, preview/export and final `ui_draw_*` timing entries. Capture it after a clean Triangle exit; launch resets the session log.
- Exit while idle after a cache hit and after a canceled import. Relaunch, open another app, and confirm shutdown is clean and valid caches remain available.
