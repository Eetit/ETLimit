#!/usr/bin/env python3
"""Validate the reference true-peak meter itself, before trusting it.

A measuring instrument has to be checked against known answers first:
  * analytic: a sine of amplitude A has a true peak of exactly A, wherever
    its samples happen to fall;
  * ideal reconstruction of a whole file computed in one giant FFT;
  * two meters we did not write (libebur128 and ffmpeg's ebur128 filter).
"""

from __future__ import annotations

import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from etref import ebur128, signals, truepeak  # noqa: E402
from etref.results import Results  # noqa: E402


def periodic_true_peak(x: np.ndarray, factor: int = 32) -> float:
    """True peak of a signal that is exactly periodic in its length: exact
    periodic interpolation, then the reference's parabolic peak refinement.
    (Accuracy of the block interpolator itself is checked separately in
    check_against_ideal.)"""
    y = np.fft.irfft(np.fft.rfft(x), len(x) * factor) * factor
    return truepeak._refined_peak_1d(np.concatenate([y[-1:], y, y[:1]]))


def check_sine_amplitude(res: Results):
    rows = []
    for fs in (44100, 48000, 96000, 192000):
        n = 8192
        for f_target in (50, 1000, 5000, 10000, 15000, 19000, 20000, 21000):
            if f_target >= fs / 2 * 0.98:
                continue
            cycles = int(round(f_target * n / fs))
            for phase in np.linspace(0, np.pi, 5):
                x = signals.periodic_sine(cycles, n, 0.5, phase)
                err = 20 * math.log10(periodic_true_peak(x) / 0.5)
                rows.append({"fs": fs, "f": cycles * fs / n, "phase": float(phase), "err_db": err})
    worst = max(abs(r["err_db"]) for r in rows)
    res.add("Reference TP (with peak refinement) reads sine amplitude",
            "sines 50 Hz-21 kHz, fs 44.1/48/96/192 kHz, 5 phases each, A=-6.02 dBFS",
            "|error| <= 0.001 dB",
            f"worst |error| {worst:.6f} dB over {len(rows)} cases",
            worst <= 0.001, {"rows": rows})


def check_isp_quarter_rate(res: Results):
    x = signals.isp_quarter_rate(48000, 0.1)[:, 0]
    sp = truepeak.sample_peak_dbfs(x)
    tp = 20 * math.log10(periodic_true_peak(x))
    expected = 20 * math.log10(math.sqrt(2.0))
    res.add("Reference TP on fs/4 45-degree sine (classic ISP)",
            "48 kHz, samples peak at 0 dBFS",
            f"sample peak 0.000 dBFS, true peak {expected:.4f} dBTP (+-0.001)",
            f"sample peak {sp:.4f} dBFS, true peak {tp:.4f} dBTP",
            abs(sp) < 1e-9 and abs(tp - expected) <= 0.001, {"sample_peak_dbfs": sp, "true_peak_dbtp": tp})


def check_grid_underread(res: Results):
    """Show how much plain 4/8/16/32x grids can under-read a near-Nyquist peak.

    The frequency is chosen so that every peak of the sine sits at the same
    offset from the oversampled grid (f = L*fs/K for integer K). K must be
    even: with odd K the negative peak sits half a step away from the
    positive one and |x| would hide the worst case.
    """
    fs = 48000
    rows = []
    ok = True
    for factor in (4, 8, 16, 32):
        k = 2 * int(round(factor * fs / 40000))
        f = factor * fs / k
        n = k * 600
        cycles = factor * 600
        worst = 0.0
        for phase in np.linspace(0, 2 * np.pi / k, 33):
            x = signals.periodic_sine(cycles, n, 1.0, phase)
            y = np.fft.irfft(np.fft.rfft(x), n * factor) * factor
            worst = min(worst, 20 * math.log10(np.max(np.abs(y))))
        bound = truepeak.grid_error_bound_db(f, fs, factor)
        rows.append({"factor": factor, "freq_hz": f, "worst_underread_db": worst, "analytic_bound_db": bound})
        ok &= (bound - 1e-6) <= worst <= 0.0 and abs(worst - bound) <= 0.01 * abs(bound) + 1e-5
    res.add("Grid under-read of a ~20 kHz peak at 48 kHz",
            "exact interpolation, every peak at the same grid offset, phase swept over one step",
            "worst case equals analytic bound 20log10(cos(pi f / (L fs))) within 1%",
            ", ".join(f"{r['factor']}x: {r['worst_underread_db']:.4f} dB (bound {r['analytic_bound_db']:.4f})"
                      for r in rows),
            ok, {"rows": rows})


def check_against_ideal(res: Results):
    """Non-periodic signals: compare the block method with one giant FFT
    over the whole zero-padded file (exact ideal reconstruction)."""
    fs = 48000
    cases = {
        "fs/4 45-degree sine, abrupt start": signals.isp_quarter_rate(fs, 0.5)[:, 0] * 0.5,
        "full-band white noise (to Nyquist)": signals.white_noise(fs, 3.0, 0.5, channels=1)[:, 0],
        "pink noise": signals.pink_noise(fs, 3.0, 0.5, channels=1)[:, 0],
        "single-sample impulse": np.concatenate([np.zeros(1000), [0.5], np.zeros(1000)]),
        "alternating +-1 burst (Nyquist)": np.concatenate([np.zeros(500), 0.5 * (-1.0) ** np.arange(64), np.zeros(500)]),
    }
    rows = []
    for name, x in cases.items():
        pad = 600000
        xp = np.concatenate([np.zeros(pad), x, np.zeros(pad)])
        y = np.fft.irfft(np.fft.rfft(xp), len(xp) * 32) * 32
        ideal = 20 * math.log10(truepeak._refined_peak_1d(y[pad * 32: (pad + len(x)) * 32]))
        ours = truepeak.true_peak_dbtp(x)
        rows.append({"case": name, "ideal_dbtp": ideal, "ours_dbtp": ours, "diff_db": ours - ideal})
    worst = max(abs(r["diff_db"]) for r in rows)
    res.add("Reference TP matches ideal reconstruction on non-periodic signals",
            "48 kHz: abrupt fs/4 sine, white noise, pink noise, impulse, Nyquist burst",
            "|ours - ideal| <= 0.003 dB",
            "; ".join(f"{r['case']}: ideal {r['ideal_dbtp']:.4f}, ours {r['ours_dbtp']:.4f}" for r in rows),
            worst <= 0.003, {"rows": rows})


def ffmpeg_true_peak(path: str) -> float | None:
    if shutil.which("ffmpeg") is None:
        return None
    p = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-filter_complex",
                        "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True)
    m = re.findall(r"Peak:\s+(-?[\d.]+|-inf)\s+dBFS", p.stderr)
    if not m:
        return None
    return -math.inf if m[-1] == "-inf" else float(m[-1])


def _faded(x: np.ndarray, fs: int, seconds: float = 0.1) -> np.ndarray:
    r = int(fs * seconds)
    w = np.ones(x.shape[0])
    ramp = np.sin(np.linspace(0, np.pi / 2, r)) ** 2
    w[:r] = ramp
    w[-r:] = ramp[::-1]
    return x * w[:, None]


def check_against_external_meters(res: Results, tmp: str):
    """BS.1770 meters oversample 4x at 48 kHz with short filters, so they
    may read lower than the ideal reference (up to ~0.5 dB near 20 kHz, more
    for content near Nyquist). They should never read meaningfully higher on
    smoothly starting signals."""
    fs = 48000
    cases = {  # name: (signal, band-limited to <= 20 kHz?)
        "sine 997 Hz -3 dB": (signals.sine(997, fs, 2.0, 10 ** (-3 / 20)), True),
        "fs/4 ISP sine": (signals.isp_quarter_rate(fs, 2.0) * 0.5, True),
        "sine 19 kHz": (signals.sine(19000, fs, 2.0, 0.5, 0.3), True),
        "pink noise": (signals.pink_noise(fs, 2.0, 0.5, channels=2), False),
        "white noise": (signals.white_noise(fs, 2.0, 0.5, channels=2), False),
    }
    have_ff = shutil.which("ffmpeg") is not None
    have_lib = ebur128.available()
    for meter, available in (("libebur128", have_lib), ("ffmpeg", have_ff)):
        rows = []
        ok = True
        if available:
            for name, (x, bandlimited) in cases.items():
                path = os.path.join(tmp, "ext.wav")
                sf.write(path, _faded(x, fs).astype(np.float32), fs, subtype="FLOAT")
                xr = sf.read(path, always_2d=True)[0]
                ref = truepeak.true_peak_dbtp(xr)
                low = -0.6 if bandlimited else -math.inf
                if meter == "libebur128":
                    val = ebur128.measure(xr, fs)["true_peak_dbtp"]
                    case_ok = low <= val - ref <= 0.02
                else:
                    # ffmpeg prints one decimal: allow 0.05 dB of display rounding.
                    val = ffmpeg_true_peak(path)
                    case_ok = val is not None and low - 0.05 <= val - ref <= 0.07
                ok &= case_ok
                rows.append({"case": name, "ours": ref, meter: val, "bandlimited_20k": bandlimited, "ok": case_ok})
        res.add(f"Cross-check vs {meter} true peak", "48 kHz float WAV, 5 faded signals",
                "never over-reads the reference (> +0.02 dB, ffmpeg +0.07 for 0.1 dB display); "
                "under-read <= 0.6 dB for signals band-limited to 20 kHz (full-band noise: recorded only)",
                "; ".join(f"{r['case']}: ours {r['ours']:.3f}, {meter} {r[meter]}" for r in rows)
                if available else f"{meter} not installed",
                ok if available else None, {"rows": rows})

    x = signals.isp_quarter_rate(fs, 2.0) * 0.5
    path = os.path.join(tmp, "onset.wav")
    sf.write(path, x.astype(np.float32), fs, subtype="FLOAT")
    xr = sf.read(path, always_2d=True)[0]
    info = {"ours": truepeak.true_peak_dbtp(xr),
            "libebur128": ebur128.measure(xr, fs)["true_peak_dbtp"] if have_lib else None,
            "ffmpeg": ffmpeg_true_peak(path) if have_ff else None}
    res.add("Hard-onset meter spread (informational)",
            "fs/4 ISP sine starting abruptly at full level, 48 kHz",
            "recorded only; documents meter-to-meter spread",
            f"ours {info['ours']:.3f}, libebur128 {info['libebur128']}, ffmpeg {info['ffmpeg']}",
            True, info)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    res = Results("reference_selfcheck")
    with tempfile.TemporaryDirectory() as tmp:
        check_sine_amplitude(res)
        check_isp_quarter_rate(res)
        check_grid_underread(res)
        check_against_ideal(res)
        check_against_external_meters(res, tmp)
    res.write(args.out)
    sys.exit(0 if res.all_passed else 1)


if __name__ == "__main__":
    main()
