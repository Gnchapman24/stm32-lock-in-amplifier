"""Test-bench signals and converter models (float volts <-> integer codes)."""
from __future__ import annotations

import operator
from collections.abc import Iterable

import numpy as np
from numpy.typing import ArrayLike, NDArray
from scipy import signal as sps

VREF = 3.3          # volts; on the Nucleo this is the 3.3 V supply
ADC_BITS = 12
DAC_BITS = 12

Volts = NDArray[np.float64]
Codes = NDArray[np.int64]


def sample_times(fs: float, n: int) -> NDArray[np.float64]:
    """Return n sample instants at rate fs (s^-1) : (seconds)."""
    if isinstance(n, bool):    # bool is an int subclass; check before index()
        raise TypeError("n must be an integer, not bool")
    n = operator.index(n)      # accepts Python/NumPy ints; TypeError for 2.5, "10"
    if n < 1:
        raise ValueError("n must be greater than 0")
    if fs <= 0:
        raise ValueError("fs must be greater than 0")
    return np.arange(n) / fs


def tones(t: ArrayLike, components: Iterable[tuple[float, float, float]],
          offset: float = VREF / 2) -> Volts:
    """Sum of sines plus a DC offset, in volts.

    components: iterable of (amplitude_peak_V, freq_Hz, phase_rad).
    """
    t = np.asarray(t, dtype=np.float64)
    out = np.full(t.shape, float(offset))
    for amp, freq, phase in components:
        out += amp * np.sin(2 * np.pi * freq * t + phase)
    return out


def white_noise(n: int, rms: float, rng: np.random.Generator) -> Volts:
    """Gaussian white noise with the given RMS (volts). rng: np.random.Generator."""
    if rms < 0:
        raise ValueError("rms must be >= 0")
    return rng.normal(0.0, rms, operator.index(n))


def adc(volts: ArrayLike, bits: int = ADC_BITS, vref: float = VREF,
        return_clips: bool = False):
    """Ideal ADC: volts -> unsigned codes, rounded, CLIPPED to [0, 2**bits - 1].

    Transfer: code = round(v / LSB), LSB = vref / 2**bits, so vref/2 -> 2**(bits-1)
    (2048 for 12 bits) and the top code 4095 represents vref - 1 LSB.

    With return_clips=True returns (codes, n_clipped) -- the number of samples
    that hit either rail, used for dynamic-reserve experiments.
    """
    v = np.asarray(volts, dtype=np.float64)
    top = (1 << bits) - 1
    raw = np.floor(v / vref * (1 << bits) + 0.5)
    n_clipped = int(np.count_nonzero((raw < 0) | (raw > top)))
    codes = np.clip(raw, 0, top).astype(np.int64)
    return (codes, n_clipped) if return_clips else codes


def dac(codes: ArrayLike, bits: int = DAC_BITS, vref: float = VREF) -> Volts:
    """Ideal DAC: codes -> volts (code * LSB)."""
    c = np.asarray(codes, dtype=np.int64)
    if c.size and (c.min() < 0 or c.max() > (1 << bits) - 1):
        raise ValueError("DAC code out of range")
    return c.astype(np.float64) * (vref / (1 << bits))


def _rc_filter(b_s, a_s, volts: ArrayLike, fs: float) -> Volts:
    """Discretise analog H(s)=b_s/a_s (bilinear) and filter, starting in the
    steady state for the first sample's DC level (no start-up step)."""
    v = np.asarray(volts, dtype=np.float64)
    b, a = sps.bilinear(b_s, a_s, fs)
    zi = sps.lfilter_zi(b, a) * v[0]
    y, _ = sps.lfilter(b, a, v, zi=zi)
    return y


def rc_lowpass(volts: ArrayLike, fs: float, r_ohm: float, c_farad: float) -> Volts:
    """First-order RC low-pass, H(s) = 1 / (1 + sRC) (bilinear transform).

    Accurate for frequencies well below fs/2 (bilinear warping grows near Nyquist).
    """
    rc = r_ohm * c_farad
    return _rc_filter([1.0], [rc, 1.0], volts, fs)


def ac_coupling(volts: ArrayLike, fs: float, f_corner: float) -> Volts:
    """First-order high-pass (the AFE's AC coupling), H(s) = sRC / (1 + sRC).

    Output is centred on 0 V (the DC level is removed).
    """
    rc = 1.0 / (2 * np.pi * f_corner)
    return _rc_filter([rc, 0.0], [rc, 1.0], volts, fs)
