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
