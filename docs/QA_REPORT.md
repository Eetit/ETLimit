# ETLimit QA report

- Stage: 1 - architecture and VST3 shell (Linux run)
- Generated: 2026-11-16T21:13:02+00:00
- Code revision: `uncommitted` (report generated before commit if this is the parent revision)
- Platform: Linux 6.18.44-fc-v470 x86_64

Every row below comes from a test that was executed while generating this report.

**Totals: 35 passed, 0 failed, 1 not run.**

## C++ unit tests and test suites (ctest)

| Test | Result | Time (s) |
|---|---|---|
| Engine survives extreme input levels without overflow | PASS | 0.00 |
| DelayLine with zero delay is an exact identity | PASS | 0.00 |
| DelayLine keeps channels independent | PASS | 0.00 |
| DelayLine delays an impulse by exactly N samples, across block boundaries | PASS | 0.00 |
| LinearSmoother reaches its target exactly after the ramp length | PASS | 0.00 |
| Engine replaces NaN and infinity with silence and counts them | PASS | 0.00 |
| Engine handles zero-length and null blocks | PASS | 0.00 |
| Engine keeps digital silence silent | PASS | 0.00 |
| Engine input gain is accurate once settled | PASS | 0.00 |
| LinearSmoother steps are uniform (no jumps) | PASS | 0.00 |
| Engine gain changes ramp smoothly over the documented time | PASS | 0.00 |
| Engine at 0 dB is bit-exact pass-through for every block size, rate and channel count | PASS | 0.03 |
| DelayLine clamps delays beyond its capacity | PASS | 0.00 |
| Engine silences channels it was not prepared for | PASS | 0.00 |
| DelayLine reset clears history | PASS | 0.00 |
| WAV float32 write/read round trip is bit-exact | PASS | 0.00 |
| Engine bypass outputs the exact dry signal and crossfades without jumps | PASS | 0.00 |
| LinearSmoother re-targeting mid-ramp is continuous | PASS | 0.00 |
| LinearSmoother setting the same target does not restart a ramp | PASS | 0.00 |
| reference_selfcheck | PASS | 67.47 |
| reference_stage1 | PASS | 1.40 |

## Suite `reference_selfcheck`

Executed 2026-11-16T21:12:23+00:00 on Linux x86_64 / Python 3.13.18.

| Test | Conditions | Expected | Actual | Result |
|---|---|---|---|---|
| Reference TP (with peak refinement) reads sine amplitude | sines 50 Hz-21 kHz, fs 44.1/48/96/192 kHz, 5 phases each, A=-6.02 dBFS | \|error\| <= 0.001 dB | worst \|error\| 0.000000 dB over 160 cases | **PASS** |
| Reference TP on fs/4 45-degree sine (classic ISP) | 48 kHz, samples peak at 0 dBFS | sample peak 0.000 dBFS, true peak 3.0103 dBTP (+-0.001) | sample peak 0.0000 dBFS, true peak 3.0103 dBTP | **PASS** |
| Grid under-read of a ~20 kHz peak at 48 kHz | exact interpolation, every peak at the same grid offset, phase swept over one step | worst case equals analytic bound 20log10(cos(pi f / (L fs))) within 1% | 4x: -0.4359 dB (bound -0.4359), 8x: -0.1076 dB (bound -0.1076), 16x: -0.0297 dB (bound -0.0297), 32x: -0.0074 dB (bound -0.0074) | **PASS** |
| Reference TP matches ideal reconstruction on non-periodic signals | 48 kHz: abrupt fs/4 sine, white noise, pink noise, impulse, Nyquist burst | \|ours - ideal\| <= 0.003 dB | fs/4 45-degree sine, abrupt start: ideal -2.9223, ours -2.9223; full-band white noise (to Nyquist): ideal 0.6316, ours 0.6325; pink noise: ideal -5.9254, ours -5.9258; single-sample impulse: ideal -6.0206, ours -6.0206; alternating +-1 burst (Nyquist): ideal 0.1484, ours 0.1487 | **PASS** |
| Cross-check vs libebur128 true peak | 48 kHz float WAV, 5 faded signals | never over-reads the reference (> +0.02 dB, ffmpeg +0.07 for 0.1 dB display); under-read <= 0.6 dB for signals band-limited to 20 kHz (full-band noise: recorded only) | sine 997 Hz -3 dB: ours -3.000, libebur128 -2.9945252839787444; fs/4 ISP sine: ours -3.010, libebur128 -3.043541724948253; sine 19 kHz: ours -6.021, libebur128 -6.02693911102132; pink noise: ours -5.925, libebur128 -6.004535449889261; white noise: ours -0.105, libebur128 -1.6597199125626623 | **PASS** |
| Cross-check vs ffmpeg true peak | 48 kHz float WAV, 5 faded signals | never over-reads the reference (> +0.02 dB, ffmpeg +0.07 for 0.1 dB display); under-read <= 0.6 dB for signals band-limited to 20 kHz (full-band noise: recorded only) | sine 997 Hz -3 dB: ours -3.000, ffmpeg -3.0; fs/4 ISP sine: ours -3.010, ffmpeg -3.0; sine 19 kHz: ours -6.021, ffmpeg -6.0; pink noise: ours -5.925, ffmpeg -6.0; white noise: ours -0.105, ffmpeg -0.8 | **PASS** |
| Hard-onset meter spread (informational) | fs/4 ISP sine starting abruptly at full level, 48 kHz | recorded only; documents meter-to-meter spread | ours -2.922, libebur128 -2.906774789754472, ffmpeg -2.4 | **PASS** |

Full measurement data: `reference_selfcheck.json` (CI artifact / build/qa).

## Suite `stage1_render`

Executed 2026-11-16T21:12:25+00:00 on Linux x86_64 / Python 3.13.18.

| Test | Conditions | Expected | Actual | Result |
|---|---|---|---|---|
| Bit-exact pass-through at 0 dB | 5 signals x 5 rates x 5 block patterns (32, 64, 2048, random 1-2048, 4096 > announced 512) | output identical to input in every bit; latency 0 | 125/125 identical | **PASS** |
| Input gain accuracy | white noise, 48 kHz, random blocks, gain -12..+36 dB set before playback | gain error <= 0.0001 dB; per-sample error <= float32 rounding (1.2e-7 relative) | worst gain error 0.0000005 dB; worst per-sample relative error 4.72e-08 | **PASS** |
| Input gain automation is a smooth ramp | 0 -> +12 dB step at an arbitrary sample, random host blocks, all rates | monotonic linear ramp, completes in 20 ms, no step larger than ideal | 44100: settled after 882 samples (expected 882); 48000: settled after 960 samples (expected 960); 88200: settled after 1764 samples (expected 1764); 96000: settled after 1920 samples (expected 1920); 192000: settled after 3840 samples (expected 3840) | **PASS** |
| Bypass is exact and click-free | 1 kHz sine, +6 dB gain, bypass on then off at arbitrary samples, random blocks, all rates | bit-exact dry signal while bypassed; no sample-to-sample jump beyond signal slope + crossfade slope | 44100: dry exact=True, max jump 0.14202 <= 0.14327; 48000: dry exact=True, max jump 0.13022 <= 0.13163; 88200: dry exact=True, max jump 0.07105 <= 0.07163; 96000: dry exact=True, max jump 0.06525 <= 0.06581; 192000: dry exact=True, max jump 0.03264 <= 0.03291 | **PASS** |
| Reported latency equals measured latency | impulse at sample 1000, all rates, processed and bypassed | measured impulse delay == reported latency | 10/10 match (reported [0]) | **PASS** |
| NaN/Inf input is neutralised | 440 Hz sine with NaN, +Inf, -Inf injected, +6 dB gain | output all finite, bad samples -> 0, counter == 3 | finite=True, zeroed=True, counter=3 | **PASS** |

Full measurement data: `stage1.json` (CI artifact / build/qa).

## pluginval

- Strictness level: 10
- Result: **PASS**
- NOT RUN inside pluginval: vst3 validator as validator path hasn't been set

## Known limitations (stage 1)

* **No limiting yet.** Stage 1 is the plugin shell, the engine framework (input gain, bypass, latency path, safety) and the test infrastructure. The ceiling is not enforced until stage 2.
* **Steinberg's VST3 validator was not run.** pluginval's "vst3 validator" sub-test was skipped because the validator binary is not installed. It is planned for stage 10.
* **Windows results come from CI.** This report was produced on Linux. Windows build, tests and pluginval run in GitHub Actions (`.github/workflows/build.yml`); see that workflow's QA artifact for the Windows results.
* **The editor is a temporary generic one.** The real interface is stage 8.
* **Bypass crossfade is linear.** That is click-free for the time-aligned dry/wet signals measured here. Once limiting is added, it will be re-checked on material with heavy gain reduction.

