# Version 03.11: genuinely distinct Auto Mix DSP profiles

This branch addresses the real hardware feedback that the 03.10 Auto Mix
candidates sounded effectively identical. In 03.10 all three candidates set
`FX_CROSSFADE | FX_BASS_SWAP`; most differences were cue locations and lengths.

## What has changed

- **Smooth**: long 32-beat, eased equal-power/correlation-compensated
  crossfade and gradual low-frequency exchange.
- **Energy**: short 16-beat mix with an actual rising-frequency sweep on
  outgoing A, early incoming B high frequencies, temporarily suppressed B
  bass, a later full-band/drop entrance, and a tempo-independent delay echo.
- **Vocal Blend with externally prepared stems**: hold the verified Track A
  vocal stem above the incoming Track B instrumental, fade its vocal out
  later, and ease B into the foreground. This is *not* on-Vita AI separation.
- **No stems**: the third option uses a distinct, audible filtered/echo
  handoff, displayed as a fallback. It never claims to separate vocals.

DSP is offline, on the fixed-block existing renderer; no extra libraries or
large audio buffers. Existing preview and export share the exact same DSP
settings and cached tempo-stretched source. Old settings use
`MIX_STYLE_LEGACY` and continue to render unchanged.

## Measurable tests

The test `tests/test_transition_profiles.py` uses two synthetic, unrelated
house-like WAVs. It keeps **both source cues, overlap duration, tempo and
song files identical** while varying only DSP profiles, then measures
difference on the *same* 8-second overlap:

- Smooth vs Energy: ~0.30 normalized waveform-difference ratio
- Smooth vs Echo fallback: ~0.26
- Energy vs Echo fallback: ~0.52
- Prepared stem vocal handoff vs no-stem fallback: ~1.16

These ratios are *not* human-perceived loudness, musical quality scores,
percentages of changed musical content, or proof the blends are good. They
establish that the sample processing is measurably different with the same
cue positions. The test also checks that Quick Preview bytes match the
equivalent full-export excerpt.

## Limitations / real Vita sign-off

- No true voice recognition, harmonic-key analysis or on-device neural
  separation. Stems must be prepared externally.
- No real-song pair has been subjectively auditioned on an actual Vita in
  CI. The synthetic same-cue test proves processing differences only.
- Overclocking may improve offline render time but does not increase Vita
  RAM. Do not make normal playback depend on overclocking.
- Test this on your own Vita using two familiar house tracks. Compare the
  same exact overlap in all three styles, first at moderate volume, then
  inspect EQ, phase stability, bass handoff, transient clicks and clipping.
  Check exported WAV and clean app shutdown. Use
  `ux0:data/CircuitFX/session.log` when reporting problems.
- Do not merge until actual Vita listening and preview/export confirmation.

Future work must focus on musical structure quality, hearing transitions
on real music, and safe fallback when beat/downbeat detection is unreliable,
rather than adding more preset names.
