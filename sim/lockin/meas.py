"""Output stage: X, Y -> R, theta, ratios and physical units.

Scaling to volts RMS and radians happens ONLY here (spec B6).

Scale chain: 1 ADC code = 2**19 Q31 units = VREF/4096 V, so 1 Q31 unit
= VREF / 2**31 V. Mixing a peak-amplitude-A input with a unit reference gives
X = (A/2) cos(phi), so  V_rms = A / sqrt(2) = sqrt(2) * R * VREF / 2**31.
"""
from __future__ import annotations

import math

import numpy as np
from numpy.typing import ArrayLike, NDArray

from . import q31
from .signals import VREF


def polar_float(x: ArrayLike, y: ArrayLike) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """(R, theta) with theta in (-pi, pi]."""
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)
    return np.hypot(x, y), np.arctan2(y, x)


def polar_q31(x: ArrayLike, y: ArrayLike) -> tuple[q31.Q31Array, q31.Q31Array]:
    """(R, theta/pi) in Q31 -- model of the CORDIC modulus/phase function.

    R = floor(sqrt(x^2 + y^2)) computed exactly with integer sqrt;
    theta/pi rounded to nearest, saturating (+pi -> Q31_MAX).
    Like sin/cos, the hardware CORDIC may differ by a few LSB here.
    """
    xs = np.asarray(x, dtype=np.int64).ravel().tolist()
    ys = np.asarray(y, dtype=np.int64).ravel().tolist()
    r = np.array([math.isqrt(a * a + b * b) for a, b in zip(xs, ys)], dtype=np.int64)
    th = q31.from_float(np.arctan2(np.asarray(ys, dtype=np.float64),
                                   np.asarray(xs, dtype=np.float64)) / np.pi)
    return q31.sat(r), th


def q31_to_volts_rms(v_q31: ArrayLike, vref: float = VREF) -> NDArray[np.float64]:
    """X, Y or R in Q31 -> volts RMS (see module docstring for the derivation).

    Applies to the ADC-referred signal; AFE gain is divided out by the caller.
    """
    return math.sqrt(2.0) * np.asarray(v_q31, dtype=np.float64) * vref / q31.Q31_ONE


def to_volts_rms(r_q31: ArrayLike, vref: float = VREF, adc_bits: int = 12) -> NDArray[np.float64]:
    """Alias kept from the scaffold. adc_bits is irrelevant: the ADC->Q31 shift
    always maps full scale to +-VREF/2 <-> +-0.5, whatever the resolution."""
    return q31_to_volts_rms(r_q31, vref)


def theta_to_rad(theta_q31: ArrayLike) -> NDArray[np.float64]:
    """Q31 angle/pi -> radians."""
    return q31.to_float(theta_q31) * np.pi


def ratio(x1: ArrayLike, y1: ArrayLike, x2: ArrayLike, y2: ArrayLike) -> NDArray[np.complex128]:
    """Complex H = Z2 / Z1 (float; runs in the firmware main loop, not the ISR)."""
    z1 = np.asarray(x1, dtype=np.float64) + 1j * np.asarray(y1, dtype=np.float64)
    z2 = np.asarray(x2, dtype=np.float64) + 1j * np.asarray(y2, dtype=np.float64)
    return z2 / z1


def apply_through_cal(h: ArrayLike, h_cal: ArrayLike) -> NDArray[np.complex128]:
    """H corrected by a through-calibration measurement at the same frequency."""
    return np.asarray(h, dtype=np.complex128) / np.asarray(h_cal, dtype=np.complex128)


def gain_db(h: ArrayLike) -> NDArray[np.float64]:
    return 20 * np.log10(np.abs(h))
