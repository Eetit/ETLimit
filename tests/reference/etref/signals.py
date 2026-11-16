"""Test signal generators. All return float64 arrays shaped (samples, channels)."""

from __future__ import annotations

import numpy as np


def _shape(x: np.ndarray, channels: int) -> np.ndarray:
    return np.repeat(x[:, None], channels, axis=1) if x.ndim == 1 else x


def sine(freq: float, fs: float, seconds: float, amplitude: float = 1.0,
         phase: float = 0.0, channels: int = 1) -> np.ndarray:
    n = np.arange(int(round(seconds * fs)))
    return _shape(amplitude * np.sin(2 * np.pi * freq * n / fs + phase), channels)


def periodic_sine(cycles: int, length: int, amplitude: float = 1.0, phase: float = 0.0) -> np.ndarray:
    """A sine with an integer number of cycles in `length` samples (exactly periodic)."""
    n = np.arange(length)
    return amplitude * np.sin(2 * np.pi * cycles * n / length + phase)


def white_noise(fs: float, seconds: float, amplitude: float = 1.0, channels: int = 2, seed: int = 1) -> np.ndarray:
    rng = np.random.default_rng(seed)
    x = rng.uniform(-1.0, 1.0, size=(int(round(seconds * fs)), channels))
    return amplitude * x / np.max(np.abs(x))


def pink_noise(fs: float, seconds: float, amplitude: float = 1.0, channels: int = 2, seed: int = 2) -> np.ndarray:
    """Pink (1/f power) noise by spectral shaping of white Gaussian noise."""
    rng = np.random.default_rng(seed)
    n = int(round(seconds * fs))
    out = np.empty((n, channels))
    for ch in range(channels):
        spec = np.fft.rfft(rng.standard_normal(n))
        f = np.fft.rfftfreq(n, 1.0 / fs)
        f[0] = f[1]
        spec /= np.sqrt(f)
        spec[0] = 0.0
        out[:, ch] = np.fft.irfft(spec, n)
    return amplitude * out / np.max(np.abs(out))


def impulses(fs: float, seconds: float, spacing_s: float = 0.25, amplitude: float = 1.0, channels: int = 2) -> np.ndarray:
    x = np.zeros((int(round(seconds * fs)), channels))
    x[:: max(1, int(spacing_s * fs)), :] = amplitude
    return x


def isp_quarter_rate(fs: float, seconds: float, channels: int = 1) -> np.ndarray:
    """fs/4 sine at 45 degrees: samples peak at 0 dBFS, true peak +3.01 dBTP."""
    n = np.arange(int(round(seconds * fs)))
    x = np.sqrt(2.0) * np.sin(np.pi / 2 * n + np.pi / 4)
    return _shape(x, channels)
