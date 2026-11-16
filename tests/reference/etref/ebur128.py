"""ctypes binding to libebur128 (https://github.com/jiixyj/libebur128).

libebur128 is a widely used, independent C implementation of EBU R128 /
ITU-R BS.1770 loudness and true-peak measurement. We use it as a second
opinion next to our own Python reference. Returns None if the library is
not installed, so callers can report the check as NOT RUN.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import math

import numpy as np

MODE_M = 1 << 0
MODE_S = (1 << 1) | MODE_M
MODE_I = (1 << 2) | MODE_M
MODE_LRA = (1 << 3) | MODE_S
MODE_SAMPLE_PEAK = (1 << 4) | MODE_M
MODE_TRUE_PEAK = (1 << 5) | MODE_M | MODE_SAMPLE_PEAK

_lib = None


def _load():
    global _lib
    if _lib is not None:
        return _lib
    name = ctypes.util.find_library("ebur128")
    if not name:
        return None
    lib = ctypes.CDLL(name)
    lib.ebur128_init.restype = ctypes.c_void_p
    lib.ebur128_init.argtypes = [ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
    lib.ebur128_add_frames_double.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_size_t]
    lib.ebur128_true_peak.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_double)]
    lib.ebur128_sample_peak.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_double)]
    lib.ebur128_loudness_global.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double)]
    lib.ebur128_destroy.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    _lib = lib
    return lib


def available() -> bool:
    return _load() is not None


def measure(x: np.ndarray, fs: int) -> dict | None:
    """Return {'true_peak_dbtp', 'sample_peak_dbfs', 'integrated_lufs'} or None."""
    lib = _load()
    if lib is None:
        return None
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1:
        x = x[:, None]
    ch = x.shape[1]
    st = ctypes.c_void_p(lib.ebur128_init(ch, fs, MODE_TRUE_PEAK | MODE_I))
    data = np.ascontiguousarray(x)
    lib.ebur128_add_frames_double(st, data.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), x.shape[0])
    tp = sp = 0.0
    v = ctypes.c_double()
    for c in range(ch):
        lib.ebur128_true_peak(st, c, ctypes.byref(v))
        tp = max(tp, v.value)
        lib.ebur128_sample_peak(st, c, ctypes.byref(v))
        sp = max(sp, v.value)
    lib.ebur128_loudness_global(st, ctypes.byref(v))
    lufs = v.value
    lib.ebur128_destroy(ctypes.byref(st))
    to_db = lambda a: 20 * math.log10(a) if a > 0 else -math.inf  # noqa: E731
    return {"true_peak_dbtp": to_db(tp), "sample_peak_dbfs": to_db(sp), "integrated_lufs": lufs}
