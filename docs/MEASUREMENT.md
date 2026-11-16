# How ETLimit is measured

## Definition of "true peak"

The project's pass/fail decisions use this definition:

> **True peak** = the largest magnitude of the ideal band-limited (sinc)
> reconstruction of the file, assuming silence before and after it.

That is what an ideal DAC would output. It is computed by
`tests/reference/etref/truepeak.py`:

* FFT zero-padding interpolation in overlapping blocks, each with ±65 536
  samples of context;
* 32× grid, then a parabolic refinement of every local maximum within 0.1 dB
  of the largest, so the result does not depend on where a peak falls
  between grid points.

Plain 4×/8×/16×/32× grid readings are reported alongside, as the brief
requires.

**Acceptance target:** with a −1.0 dBTP ceiling, the reference must read
≤ −1.0 dBTP. As a second, standards-based check, libebur128 (an independent
BS.1770 implementation) must also read ≤ −1.0 dBTP.

## Validation of the reference itself

The reference was checked against known answers before being trusted
(`tests/reference/selfcheck_truepeak.py`, run by ctest):

| Check | Result (stage 1) |
|---|---|
| Sine amplitude, 50 Hz–21 kHz, 44.1–192 kHz, 160 cases | worst error < 0.000001 dB |
| fs/4 sine at 45° (sample peak 0 dBFS) | +3.0103 dBTP (exact: +3.0103) |
| Grid under-read vs analytic bound `20·log10(cos(π f / (L fs)))` | matches at 4/8/16/32× |
| Non-periodic signals vs one whole-file FFT (exact) | ≤ 0.0009 dB difference (full-band white noise is the worst case) |

## Things we learned (measurement uncertainty)

* **FIR interpolators converge slowly near Nyquist.** The first version of
  the reference used a 128-zero-crossing Kaiser-windowed sinc FIR. On
  full-band white noise it under-read by 0.07 dB; even 2048 zero crossings
  left about 0.01 dB. That is enough to make a limiter falsely "pass", so the
  reference was switched to block FFT interpolation.
* **BS.1770 meters disagree with the ideal on content near Nyquist.** Same
  signals, all with smooth fades:

  | Signal | Ideal reference | libebur128 | ffmpeg ebur128 |
  |---|---|---|---|
  | 997 Hz sine, −3 dB | −3.000 | −2.995 | −3.0 |
  | fs/4 ISP sine | −3.010 | −3.044 | −3.0 |
  | 19 kHz sine | −6.021 | −6.027 | −6.0 |
  | pink noise | −5.925 | −6.005 | −6.0 |
  | white noise (to 24 kHz) | −0.105 | −1.660 | −0.8 |

  libebur128 reads full-band white noise 1.5 dB lower than the ideal. Its
  short interpolation filter attenuates content near Nyquist. A limiter that
  only satisfies a 4× BS.1770 meter can therefore still exceed the ceiling
  on an ideal reconstruction.
* **Hard onsets are filter-dependent.** For a full-level fs/4 sine that starts
  abruptly, the ideal reconstruction reads −2.922 dBTP, libebur128 −2.907,
  and ffmpeg −2.4. Real program material starts from silence or is faded, but
  any meter's true-peak reading of an abrupt edge depends on its filter.
* **Real DACs are not ideal.** Their reconstruction filters roll off before
  Nyquist and have their own ringing, so a physical DAC output can differ
  from any meter by a small amount. This cannot be removed by the plugin and
  is listed as a residual uncertainty.

## Independent tools used

| Tool | Purpose |
|---|---|
| `etref.truepeak` (our Python reference; no shared code with the plugin) | true peak, grid readings |
| libebur128 (via ctypes) | BS.1770 true peak and loudness (stage 7) |
| ffmpeg `ebur128` filter | second BS.1770 implementation |
| pluginval (Tracktion) | plugin format/host-behaviour validation |
