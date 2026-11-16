"""Reference true-peak measurement, independent of the plugin.

Definition used throughout this project
---------------------------------------
"True peak" = the largest magnitude of the ideal band-limited (sinc)
reconstruction of the file, with silence before and after it. This is what
an ideal DAC would output. Real meters (BS.1770 4x) and real DACs use
shorter filters and read slightly differently; see docs/MEASUREMENT.md.

How it is computed
------------------
``upsample`` interpolates by an integer factor L with FFT zero-padding over
overlapping blocks. Each output block sees +-65536 input samples of context
(about 1.4 s at 48 kHz). FIR-based interpolators converge very slowly for
content near Nyquist (a 2048-zero-crossing Kaiser FIR still had ~0.01 dB
error on full-band noise); this block-FFT method was measured at <= 0.0002
absolute error on full-band white noise peaking at -0.6 dBFS.

``true_peak`` then refines each local maximum of the L-times grid with a
parabola through three points, so the result does not depend on where the
peak falls between grid points. Grid-only readings at 4/8/16/32x are also
available to show how much a plain grid can under-read.
"""

from __future__ import annotations

import math

import numpy as np

CONTEXT = 65536
BLOCK = 131072


def _db(x: float) -> float:
    return 20.0 * math.log10(x) if x > 0 else -math.inf


def upsample(x: np.ndarray, factor: int, context: int = CONTEXT, block: int = BLOCK) -> np.ndarray:
    """Ideal band-limited interpolation of a 1-D signal. y[k*factor] == x[k] (to rounding)."""
    x = np.asarray(x, dtype=np.float64)
    if factor == 1:
        return x.copy()
    n = len(x)
    out = np.empty(n * factor)
    xp = np.concatenate([np.zeros(context), x, np.zeros(context + block)])
    for b in range(0, n, block):
        seg = xp[b: b + block + 2 * context]
        y = np.fft.irfft(np.fft.rfft(seg), len(seg) * factor) * factor
        m = min(block, n - b)
        out[b * factor: (b + m) * factor] = y[context * factor: (context + m) * factor]
    return out


def _channels(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=np.float64)
    return x[:, None] if x.ndim == 1 else x


def grid_peak(x: np.ndarray, factor: int) -> float:
    """Max |y| on the factor-times grid (what a plain L-x oversampling meter sees)."""
    x = _channels(x)
    return max(float(np.max(np.abs(upsample(x[:, c], factor)))) for c in range(x.shape[1])) if x.size else 0.0


def _refined_peak_1d(y: np.ndarray) -> float:
    a = np.abs(y)
    if len(a) < 3:
        return float(np.max(a)) if len(a) else 0.0
    gmax = float(np.max(a))
    # Only local maxima within 0.1 dB of the grid maximum can hold the true
    # peak: a 32x grid under-reads by at most ~0.0074 dB below 20 kHz at 48 kHz.
    thresh = gmax * 10 ** (-0.1 / 20)
    idx = np.nonzero((a[1:-1] >= thresh) & (a[1:-1] >= a[:-2]) & (a[1:-1] >= a[2:]))[0] + 1
    best = gmax
    for i in idx:
        s = 1.0 if y[i] >= 0 else -1.0
        ym, y0, yp = s * y[i - 1], s * y[i], s * y[i + 1]
        den = ym - 2 * y0 + yp
        if den < 0:
            best = max(best, y0 - 0.125 * (yp - ym) ** 2 / den)
    return best


def true_peak(x: np.ndarray, factor: int = 32) -> float:
    """Linear true peak over all channels (grid + parabolic refinement)."""
    x = _channels(x)
    return max(_refined_peak_1d(upsample(x[:, c], factor)) for c in range(x.shape[1])) if x.size else 0.0


def true_peak_dbtp(x: np.ndarray, factor: int = 32) -> float:
    return _db(true_peak(x, factor))


def measure_all(x: np.ndarray) -> dict:
    """Sample peak, 4/8/16/32x grid peaks and the refined true peak, in dB."""
    x = _channels(x)
    out = {"sample_peak_dbfs": _db(float(np.max(np.abs(x)))) if x.size else -math.inf}
    for f in (4, 8, 16, 32):
        out[f"grid_{f}x_dbtp"] = _db(grid_peak(x, f))
    out["true_peak_dbtp"] = true_peak_dbtp(x, 32)
    return out


def sample_peak_dbfs(x: np.ndarray) -> float:
    return _db(float(np.max(np.abs(x))))


def grid_error_bound_db(freq: float, fs: float, factor: int) -> float:
    """Worst-case under-read (negative dB) of a sine peak on an L-times grid."""
    return _db(math.cos(math.pi * freq / (factor * fs)))
