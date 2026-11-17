# Circuit FX Studio hardware validation

## Auto Mix and prepared-stem candidate build 3.1

This checklist is for the simplified two-tool build. The previous project checklist below describes archived beta screens and does not apply to this UI.

- Install the VPK at stock Vita clocks. Confirm it launches at native 960×544 and the home screen presents only VOCAL REMOVER and AUTO MIX.
- Open VOCAL REMOVER, choose one song, and verify the four stem rows. With no sidecars, the UI must say they are missing and that Vita does not run AI. Place `<track-base>.vocals.wav`, `.drums.wav`, `.bass.wav`, and `.other.wav` beside the song in `ux0:data/CircuitFX/music`. Each must be 44.1 kHz, stereo, 16-bit PCM and exactly sample-aligned. Rescan; confirm loaded rows, drag each gain slider, test Mute/Solo, Listen, per-stem Export WAV, Preview Mix, Stop, and Export Mix. Check output files and confirm originals remain untouched.
- Open AUTO MIX and choose two legal test songs. Confirm both waveforms, BPM values, overlap region and cue positions are visible and touch responsive.
- **03.12 sliding controls:** With both songs loaded, drag Track A or B horizontally anywhere on its waveform. Each song must move independently beneath its anchored white transition-start line; A CUE/B CUE times must change immediately. Moving right chooses earlier audio; moving left chooses later audio. Pinch zoom for finer placement, then drag again. Release and verify a status message displays the exact start time. Preview must use manually placed cues, and export must match the preview. Tap a variation card to reset to automatic positions. Check near-zero, near-end, long tracks and matched tempos. No drag should delete the original song.
- Generate/select SMOOTH, ENERGY and VOCAL BLEND. Without sidecars, confirm Vocal Blend reports the clean fallback. For a prepared-stem check, place `TrackA.vocals.wav` beside Track A and `TrackB.instrumental.wav` beside Track B; each must be 44.1 kHz, stereo, 16-bit PCM and exactly sample-aligned. Confirm the preview status says whether those stems were used, then compare its transition with the export. Confirm Next Variation and Regenerate update the selected candidate and cue labels.
- Preview each candidate, stop, replay, then export it. Listen for clicks, clipping, tempo artifacts and unintended timing changes. Compare the selected preview's transition section with the exported WAV.
- Test Auto Beatmatch on/off, manual BPM, large BPM differences, short tracks, repeated song switching, cancellation, low storage, close/relaunch, and waveform gestures at stock clocks.
- Capture ux0:data/CircuitFX/session.log and a photo of the home screen, candidate screen, and processing/export overlays. These device-only checks have not been completed by CI.

## Automated validation limits

GitHub Actions host tests exercise the C planner, stem sidecar validation/mixer, renderer and UI layout and compile/check the VPK. They are not Vita emulator tests and do not prove touchscreen behavior, audio output, stock-clock performance, suspend/resume, or hardware graphics. No physical Vita test is claimed for this build.

---
# Historical hardware checklist (archived beta screens)

No physical Vita was available during development. Host tests exercise the same C audio renderer linked into the native VPK. Compilation and package checks do not verify hardware audio, shell behavior, suspend/resume or storage throughput.

## Version 02.02 complete interface check

- Install over 02.01 at stock clocks. On **Transitions**, verify all five cyan/magenta curves are solid and buttons and Auto Beatmatch respond. On **Effects**, verify all six curves, six card touch targets, Vocal Lite enable/strength/target and the main waveform. On **Presets**, verify the eight categories, recipe cards, Detail / Apply, left/right page controls and effect BPM when Auto is off.
- Open the song picker, preset detail browser, cue/BPM keypad editor, processing progress overlay and export confirmation. Verify panel fills, text, icon shapes, touch targets and dialog dismissal. Drag both cached waveforms, pinch zoom and adjust cue points. Photograph each tab and any failed menu; save `session.log` after Triangle exit for `TIMING gpu_fallbacks`.
- Preview, stop, replay and export with vocal reduction and beatmatching. Compare visual cue and overlap positions to audio. Test cancellation, repeated navigation, LiveArea close and suspend/resume. These require the physical device; the automated GPU call-path test is not a substitute.

## Version 02.01 graphics and vocal checks — required on real Vita

- Install over 02.00 with VitaShell at stock clocks. Before choosing a song, verify the entire 960×544 dashboard: solid panels and buttons, all five transition curves, six Effects cards, three tabs, cyan and magenta labels, and no scattered dots. Load both songs and verify the full waveform bars, dragging, cue markers, transition shading, pinch zoom and modal backgrounds. Take a clear screenshot or photo of each tab. If anything is still missing, provide the screenshot and `ux0:data/CircuitFX/session.log` after a Triangle exit, including the `TIMING gpu_fallbacks` count.
- In Effects, select Vocal Lite, toggle Enabled, set 0/50/100% and cycle A, B, Transition, Both. Preview and export each: at 0% audio should be unchanged; A/B targets should affect that whole song; Transition should affect the overlap while leaving the A prefix and B tail unchanged. Try all five effects, WAV sounds and Auto Beatmatch in combination.
- Put `song.instrumental.wav` and `song.vocals.wav` beside `song.mp3` or `song.wav`, with exactly matching sample counts. Verify the instrumental stem is used; remove it and confirm vocals-only subtraction; remove both and check center reduction. Try a deliberately mismatched stem and ensure the error does not alter the selected song.
- Cancel first-time vocal preparation with Circle and by tapping the progress dialog. Check `.part` cleanup, retry, repeat preview to see `TIMING vocal_cache_hit=1`, then exit and relaunch to verify persistence. Test low free space, repeated playback/replay/stop/export, PS/LiveArea close, and a 5–7 minute pair at stock clocks. Listen for boundary clicks, vocal artifacts and playback stutter. Capture timings before and after with the same songs.

## Version 02.00 beatmatch checks

- Install over 01.09 at normal clocks. Load two dance tracks with clearly different steady tempos (for example, 120 and 128 BPM). Confirm the displayed BPM for each deck, tap each BPM to set a manual value, and check the asterisk, edit cancel and reselect behavior. With Auto off, original speed and effect BPM controls must still work.
- With Auto on, set cues between beats, Preview and confirm the visible cues snap to nearby beats. Check the blend sounds aligned and Track B's pitch stays recognizable despite the tempo change. Try manual BPM correction, two different presets, a WAV effect and all five stackable effects. Listen to transients for warbling, clicks or stereo imbalance.
- Preview, Stop, Replay and export the same mix. Compare preview samples to the matching exported segment and verify the adjusted cue times and expected output duration. Repeat the preview; `session.log` should contain `beatmatch_cache_hit=1`. Relaunch and load the same pair; BPM and matched-audio caches should be reused.
- Cancel waveform analysis, MP3 conversion, matched-song preparation and export, then retry. Verify no stale `.part` files, no altered source WAV/MP3, and another song still plays. Test unstable tempo and silent songs; enter BPM manually or disable Auto. Try a ratio beyond 25%, missing files and low free storage.
- Capture `TIMING waveform_ms`, `beatmatch_ms`, `beatmatch_cache_hit`, preview/export times, `ui_draw_avg_us`, and total elapsed preview time before and after on the same Vita and songs. Check free storage and stability after switching songs, repeated playback, PS/LiveArea close and suspend/resume. Host timing is only a diagnostic and does not prove stock-clock performance.

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

## Version 01.08 GPU and waveform checks

- Install 01.08 over 01.07 and check the full 960×544 display, glyphs, gradients, touch targets, modal dialogs, LiveArea exit and restart on stock clocks. Compare `TIMING ui_draw_avg_us` and navigation feel with a saved 01.07 `session.log` from the same Vita and songs; host software timing is not comparable to Vita GPU time.
- Select two long WAVs and two MP3s, with both decks using the same song and then different songs. Confirm both whole-song waveforms appear, cached waveform selection is faster on repeat, and idle UI does not redraw. Drag/pinch both rows and drag each white cue marker; verify time fields, precise edit, preview and export match the chosen cues.
- Select a fresh MP3: song name/duration should appear before PCM conversion. Generate a preview to trigger conversion, cancel once, then retry. Relaunch and confirm `mp3_cache_hit` and `waveform_cache_hit` with no repeated full conversion.
- Switch songs repeatedly while a waveform is still building; verify no stale waveform is shown for the newly selected song. Try a damaged MP3, missing source, nearly full storage and a forced close during a build. Relaunch, check no partial cache remains, then load/preview/export successfully.
- Preview, replay and export with every effect and sample setting. Listen for stutter; check exported duration and sample content against 01.07. Test Triangle exit, PS/LiveArea close and suspend/resume after playback and during waveform generation. Save `session.log` before relaunch; attach it with Vita model and firmware for performance comparison.

## Version 01.09 interface checks

- Install over 01.08 at stock clocks. Verify both compact track bars open the music browser and show the actual names and durations. Waveforms should take about half the 960×544 display and remain visible on all three tabs.
- Drag the waves in both rows, pinch in/out, use +/− and FIT, then drag both cue markers. Verify cue fields, transition shading/endpoints, precise keypad edits, and resulting preview/export timestamps. Try cues at 0, near song end, and songs of very different durations.
- Tap each of five Transitions cards and compare its audio to the displayed behavior. In Effects, toggle each card on/off, combine effects, adjust global intensity and verify preview/export changes. BPM controls should appear only when relevant to a preset.
- Browse every bank and four pages in Presets; select a card, open Detail / Apply, audition WAV effects, reverse/speed them and apply. Check selected recipe, BPM, cues, duration, preview and export. Test physical D-pad, Square, START, L/R tab changes, SELECT rescan and Triangle exit.
- During playback, tap either Stop or Preview, then Replay and Export. Repeat navigation and song changes while a waveform is building; check cached waveform hits and absence of UI stalls. Test modal cancellation, low storage, PS/LiveArea close and re-launch. Capture `session.log` before relaunch for CPU submission timing.
