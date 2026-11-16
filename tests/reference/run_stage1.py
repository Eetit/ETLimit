#!/usr/bin/env python3
"""Stage 1 end-to-end checks: WAV -> etlimit_render -> WAV, analysed in Python.

Stage 1 contains no limiting yet, so these checks prove the plumbing that
every later stage relies on: bit-exact pass-through, accurate gain, smooth
automation, click-free bypass, correct latency and NaN safety, across sample
rates and host block sizes.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from etref import signals  # noqa: E402
from etref.results import Results  # noqa: E402

RATES = (44100, 48000, 88200, 96000, 192000)
GAIN_RAMP_S = 0.020    # documented in core/include/etlimit/Engine.h
BYPASS_RAMP_S = 0.010


class Renderer:
    def __init__(self, exe: str, work: str):
        self.exe, self.work = exe, work
        os.makedirs(work, exist_ok=True)
        self.count = 0

    def __call__(self, x: np.ndarray, fs: int, *args: str) -> tuple[np.ndarray, dict]:
        self.count += 1
        src = os.path.join(self.work, f"in_{self.count}.wav")
        dst = os.path.join(self.work, f"out_{self.count}.wav")
        sf.write(src, x.astype(np.float32), fs, subtype="FLOAT")
        p = subprocess.run([self.exe, src, dst, *args], capture_output=True, text=True)
        if p.returncode != 0:
            raise RuntimeError(f"render failed: {p.stderr}")
        y, fs_out = sf.read(dst, dtype="float32", always_2d=True)
        assert fs_out == fs
        os.remove(src)
        os.remove(dst)
        return y, json.loads(p.stdout)


def corpus(fs: int) -> dict[str, np.ndarray]:
    return {
        "white noise stereo": signals.white_noise(fs, 1.0, 0.9, channels=2),
        "pink noise stereo": signals.pink_noise(fs, 1.0, 0.9, channels=2),
        "sine 997 Hz mono": signals.sine(997, fs, 1.0, 0.5),
        "impulses stereo": signals.impulses(fs, 1.0, 0.1, 1.0),
        "fs/4 ISP sine mono": signals.isp_quarter_rate(fs, 1.0) * 0.99,
    }


def check_passthrough(res: Results, render: Renderer):
    block_modes = [["--block", "32"], ["--block", "64"], ["--block", "2048"],
                   ["--random-blocks", "11"], ["--block", "4096", "--max-block", "512"]]
    cases, failures = 0, []
    for fs in RATES:
        for name, x in corpus(fs).items():
            ref = x.astype(np.float32)
            for mode in block_modes:
                y, info = render(x, fs, *mode)
                cases += 1
                if not np.array_equal(y, ref) or info["latency_samples"] != 0:
                    failures.append(f"{fs} {name} {' '.join(mode)}")
    res.add("Bit-exact pass-through at 0 dB",
            f"5 signals x {len(RATES)} rates x {len(block_modes)} block patterns (32, 64, 2048, random 1-2048, "
            "4096 > announced 512)",
            "output identical to input in every bit; latency 0",
            f"{cases - len(failures)}/{cases} identical" + (f"; failures: {failures[:5]}" if failures else ""),
            not failures, {"cases": cases, "failures": failures})


def check_gain_accuracy(res: Results, render: Renderer):
    rows = []
    x = signals.white_noise(48000, 0.5, 0.01, channels=2)
    xf = x.astype(np.float32).astype(np.float64)
    for g_db in (-12.0, -6.0, 3.0, 12.0, 24.0, 36.0):
        y, _ = render(x, 48000, "--input-gain", str(g_db), "--random-blocks", "3")
        g = 10.0 ** (g_db / 20.0)
        measured_db = 20 * math.log10(np.sum(np.abs(y)) / np.sum(np.abs(xf)))
        max_rel = float(np.max(np.abs(y - xf * g)) / np.max(np.abs(xf * g)))
        rows.append({"gain_db": g_db, "measured_db": measured_db, "max_rel_err": max_rel})
    worst_db = max(abs(r["measured_db"] - r["gain_db"]) for r in rows)
    worst_rel = max(r["max_rel_err"] for r in rows)
    res.add("Input gain accuracy",
            "white noise, 48 kHz, random blocks, gain -12..+36 dB set before playback",
            "gain error <= 0.0001 dB; per-sample error <= float32 rounding (1.2e-7 relative)",
            f"worst gain error {worst_db:.7f} dB; worst per-sample relative error {worst_rel:.2e}",
            worst_db <= 1e-4 and worst_rel <= 1.2e-7, {"rows": rows})


def check_gain_automation(res: Results, render: Renderer):
    rows, ok = [], True
    for fs in RATES:
        n = int(fs * 0.5)
        x = np.full((n, 1), 0.25)
        at = int(fs * 0.1) + 7
        y, _ = render(x, fs, "--random-blocks", "5", "--change-at", str(at), "input-gain", "12")
        gain = y[:, 0].astype(np.float64) / 0.25
        g1 = 10 ** (12 / 20)
        ramp = int(round(fs * GAIN_RAMP_S))
        seg = gain[at - 1: at + ramp]          # last unramped sample + the ramp itself
        steps = np.diff(seg)
        settled = int(np.argmax(np.abs(gain - g1) < 1e-6))
        case_ok = (np.all(gain[:at] == 1.0) and np.all(steps > 0) and
                   float(np.max(steps)) <= (g1 - 1) / ramp * 1.001 and settled == at + ramp - 1)
        ok &= bool(case_ok)
        rows.append({"fs": fs, "ramp_samples_expected": ramp, "settled_at_offset": settled - at,
                     "max_step": float(np.max(steps)), "ideal_step": (g1 - 1) / ramp, "ok": bool(case_ok)})
    res.add("Input gain automation is a smooth ramp",
            "0 -> +12 dB step at an arbitrary sample, random host blocks, all rates",
            f"monotonic linear ramp, completes in {GAIN_RAMP_S * 1000:.0f} ms, no step larger than ideal",
            "; ".join(f"{r['fs']}: settled after {r['settled_at_offset'] + 1} samples "
                      f"(expected {r['ramp_samples_expected']})" for r in rows),
            ok, {"rows": rows})


def check_bypass(res: Results, render: Renderer):
    rows, ok = [], True
    for fs in RATES:
        n = int(fs * 0.6)
        x = signals.sine(1000, fs, n / fs, 0.5, channels=2)
        on, off = int(fs * 0.1) + 3, int(fs * 0.35) + 11
        y, _ = render(x, fs, "--input-gain", "6", "--random-blocks", "9",
                      "--change-at", str(on), "bypass", "1", "--change-at", str(off), "bypass", "0")
        xf = x.astype(np.float32)
        ramp = int(round(fs * BYPASS_RAMP_S))
        exact_dry = np.array_equal(y[on + ramp: off], xf[on + ramp: off])
        g = 10 ** (6 / 20)
        max_jump = float(np.max(np.abs(np.diff(y[:, 0].astype(np.float64)))))
        allowed = 0.5 * g * 2 * math.pi * 1000 / fs + 0.5 * (g - 1) / ramp
        case_ok = exact_dry and max_jump <= allowed * 1.001
        ok &= case_ok
        rows.append({"fs": fs, "exact_dry_while_bypassed": exact_dry, "max_jump": max_jump,
                     "allowed_jump": allowed, "ok": case_ok})
    res.add("Bypass is exact and click-free",
            "1 kHz sine, +6 dB gain, bypass on then off at arbitrary samples, random blocks, all rates",
            "bit-exact dry signal while bypassed; no sample-to-sample jump beyond signal slope + crossfade slope",
            "; ".join(f"{r['fs']}: dry exact={r['exact_dry_while_bypassed']}, "
                      f"max jump {r['max_jump']:.5f} <= {r['allowed_jump']:.5f}" for r in rows),
            ok, {"rows": rows})


def check_latency(res: Results, render: Renderer):
    rows, ok = [], True
    for fs in RATES:
        x = np.zeros((fs // 2, 2))
        x[1000, :] = 1.0
        for extra in ([], ["--bypass", "1"]):
            y, info = render(x, fs, "--random-blocks", "2", *extra)
            measured = int(np.argmax(np.abs(y[:, 0]))) - 1000
            case_ok = measured == info["latency_samples"]
            ok &= case_ok
            rows.append({"fs": fs, "bypass": bool(extra), "reported": info["latency_samples"],
                         "measured": measured})
    res.add("Reported latency equals measured latency",
            "impulse at sample 1000, all rates, processed and bypassed",
            "measured impulse delay == reported latency",
            f"{sum(r['reported'] == r['measured'] for r in rows)}/{len(rows)} match "
            f"(reported {sorted(set(r['reported'] for r in rows))})",
            ok, {"rows": rows})


def check_non_finite(res: Results, render: Renderer):
    x = signals.sine(440, 48000, 0.5, 0.5, channels=2)
    x[100, 0] = np.nan
    x[2000, 1] = np.inf
    x[3000, 0] = -np.inf
    y, info = render(x, 48000, "--random-blocks", "4", "--input-gain", "6")
    finite = bool(np.all(np.isfinite(y)))
    zeroed = y[100, 0] == 0 and y[2000, 1] == 0 and y[3000, 0] == 0
    ok = finite and zeroed and info["non_finite_inputs"] == 3
    res.add("NaN/Inf input is neutralised",
            "440 Hz sine with NaN, +Inf, -Inf injected, +6 dB gain",
            "output all finite, bad samples -> 0, counter == 3",
            f"finite={finite}, zeroed={zeroed}, counter={info['non_finite_inputs']}", ok)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    render = Renderer(args.render, args.work)
    res = Results("stage1_render")
    check_passthrough(res, render)
    check_gain_accuracy(res, render)
    check_gain_automation(res, render)
    check_bypass(res, render)
    check_latency(res, render)
    check_non_finite(res, render)
    res.write(args.out)
    sys.exit(0 if res.all_passed else 1)


if __name__ == "__main__":
    main()
