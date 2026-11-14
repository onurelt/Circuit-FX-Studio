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
