# ETLimit architecture

This document records the design decisions and why they were made. It is
updated at every stage.

## Code structure

* **core/** – the complete DSP engine (`etlimit::Engine`) in plain C++20 with
  no JUCE dependency. This lets every algorithm be unit-tested and measured
  offline, deterministically, without a DAW.
* **plugin/** – a thin JUCE layer: it declares parameters, saves/restores
  state, reads parameter values at the start of each block, passes them to
  the engine, and reports latency.
* **tests/** – C++ unit tests, an offline renderer, and an independent Python
  reference that shares no code with the plugin.

## Real-time rules (enforced from stage 1)

| Rule | How |
|---|---|
| No allocation on the audio thread | all buffers sized in `Engine::prepare()`; host blocks larger than announced are processed in chunks instead of reallocating |
| No locks / blocking | parameters read through JUCE's atomic raw values; counters are `std::atomic` with relaxed ordering |
| No NaN / Inf propagation | input sanitised first thing in every block; bad samples become 0 and are counted |
| No overflow | samples clamped to ±1e9 (+180 dBFS) after sanitising and after gain |
| No denormal slowdowns | `juce::ScopedNoDenormals` around processing |
| No clicks on parameter changes | per-sample linear ramps (gain 20 ms, bypass crossfade 10 ms) |
| Correct bypass | dry path delayed by exactly the engine latency, then crossfaded |
| No fast-math | `-ffast-math`/`/fp:fast` are never enabled: they let the compiler drop NaN checks and change rounding |

## Parameters

Parameter IDs (`plugin/source/Parameters.h`) are permanent once released: DAWs
store them in projects and automation. A parameter is added only when the DSP
behind it works, so the plugin never shows a control that does nothing.

Stage 1 parameters: `inputGain` (−12…+36 dB), `bypass`.

## Signal flow

Stage 1 (implemented):

```
in → sanitise → input gain (ramped) ──────────────┐
          └→ dry copy → delay (= latency) ────────┴→ bypass crossfade → out
```

Planned (stages 2–6):

```
in → input gain → [upsample N×] → optional soft clipper
   → detector → required gain → stereo link
   → min-hold (lookahead) → release (manual/auto) → FIR smoothing kernel
   → × delayed audio → [downsample N×]
   → true-peak guard (ISP estimate of final output → short guaranteed stage)
   → out   (dry path delayed by the same latency for bypass)
```

### Why "min-hold + FIR smoothing" for the main limiter

For every sample we compute the gain it needs, `g_req = min(1, ceiling/|peak|)`.
We take the minimum of `g_req` over the lookahead window, let the release
stage only ever *raise* gain slowly (never above that held minimum), then
smooth with an FIR kernel whose weights sum to 1 and whose length does not
exceed the window. Every output gain value is then an average of values that
are all at or below what the peak needs, so the ceiling holds by construction,
for any signal. The kernel shape (linear ramp, Hann-like, …) determines the
attack character and the distortion sidebands.

Stereo link blends each channel's gain towards the minimum across channels
(geometric blend in dB). The result is never above the channel's own gain, so
the guarantee survives any link setting.

### Why a separate true-peak guard

Oversampling alone does not guarantee true-peak compliance:

1. A detector on an L-times grid can miss a peak lying between grid points.
   Measured worst case for a ~20 kHz sine at 48 kHz: 4× −0.436 dB,
   8× −0.108 dB, 16× −0.030 dB, 32× −0.007 dB (see `reference_selfcheck`).
2. The downsampling filter after an oversampled limiter rings and can create
   new peaks above a ceiling that was respected internally.
3. Gain modulation itself adds bandwidth.

So a final stage measures the inter-sample peaks of the *actual output* and
applies a short, guaranteed correction.
