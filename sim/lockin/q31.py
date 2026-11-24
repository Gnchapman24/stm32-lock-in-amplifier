"""Q31 fixed-point primitives.

A Q31 value is an int32 interpreted as value / 2**31, range [-1, 1).

Every function here corresponds to one Cortex-M4 instruction sequence or CMSIS
intrinsic, so the C port is mechanical. Arrays are NumPy int64 holding values
that fit in int32 unless a docstring says otherwise: arithmetic is done in
int64, then explicitly saturated or wrapped back to 32 bits.

Rules are recorded in sim/Q31-RULES.md.
"""
from __future__ import annotations

import numpy as np
from numpy.typing import ArrayLike, NDArray

Q31Array = NDArray[np.int64]

Q31_ONE = 1 << 31          # 2**31, i.e. "1.0" (not representable itself)
Q31_MAX = (1 << 31) - 1
Q31_MIN = -(1 << 31)
_HALF = 1 << 30            # 0.5 LSB of the 31-bit shift, for round-half-up


def _i64(x: ArrayLike) -> Q31Array:
    return np.asarray(x, dtype=np.int64)


def from_float(x: ArrayLike) -> Q31Array:
    """Float in [-1, 1) -> Q31, round-to-nearest (half up), saturating.

    +1.0 (not representable) saturates to Q31_MAX.
    """
    scaled = np.floor(np.asarray(x, dtype=np.float64) * Q31_ONE + 0.5)
    return sat(np.clip(scaled, Q31_MIN - 1, Q31_MAX + 1).astype(np.int64))


def to_float(q: ArrayLike) -> NDArray[np.float64]:
    """Q31 -> float64. Host-side only (never in firmware DSP). Exact."""
    return _i64(q).astype(np.float64) / Q31_ONE


def sat(x: ArrayLike) -> Q31Array:
    """Saturate int64 values to the int32 range.  ARM: __SSAT(x, 32)."""
    return np.clip(_i64(x), Q31_MIN, Q31_MAX)


def wrap(x: ArrayLike) -> Q31Array:
    """Wrap int64 values to int32 the way C int32_t arithmetic does in practice."""
    return ((_i64(x) + Q31_ONE) & 0xFFFFFFFF) - Q31_ONE


def add_sat(a: ArrayLike, b: ArrayLike) -> Q31Array:
    """Saturating add.  ARM: __QADD."""
    return sat(_i64(a) + _i64(b))


def sub_sat(a: ArrayLike, b: ArrayLike) -> Q31Array:
    """Saturating subtract.  ARM: __QSUB."""
    return sat(_i64(a) - _i64(b))


def mul(a: ArrayLike, b: ArrayLike) -> Q31Array:
    """Q31 x Q31 -> Q31, round half up, saturating.

    C:  (int32_t)__SSAT(((int64_t)a * b + (1 << 30)) >> 31, 32)
        (SMULL, then add/shift on the 64-bit pair.)

    The product of two int32 values is < 2**62 in magnitude, so it fits int64.
    The only overflow is (-1) x (-1) = +1, which saturates to Q31_MAX.
    Rounding (not truncation) avoids a -0.5 LSB bias, which in a lock-in
    would appear as a DC offset on X and Y.
    """
    p = _i64(a) * _i64(b)
    return sat((p + _HALF) >> 31)


def shr_round(x: ArrayLike, sh: int) -> Q31Array:
    """Arithmetic shift right by sh with round-half-up. sh == 0 returns x.

    C: (x + (1 << (sh - 1))) >> sh   on int64 (GCC: arithmetic shift).
    """
    x = _i64(x)
    if sh == 0:
        return x
    return (x + (1 << (sh - 1))) >> sh


def div_round(num: int, den: int) -> int:
    """Integer division rounded to nearest, halves away from zero, den > 0.

    Written so it mirrors C exactly (C '/' truncates toward zero; Python '//'
    floors, so we only ever divide non-negative numbers):
        n >= 0 ? (n + d/2) / d : -((-n + d/2) / d)
    """
    if den <= 0:
        raise ValueError("den must be positive")
    if num >= 0:
        return (num + den // 2) // den
    return -((-num + den // 2) // den)


def adc_to_q31(codes: ArrayLike, bits: int = 12) -> Q31Array:
    """Unsigned offset-binary ADC codes -> Q31.  12-bit: (code - 2048) << 19.

    The result lies in [-0.5, 0.5): one bit of headroom for free.
    """
    if not 1 <= bits <= 31:
        raise ValueError("bits must be in 1..31")
    return (_i64(codes) - (1 << (bits - 1))) << (31 - bits)


def check_range(x: ArrayLike, lo: int = Q31_MIN, hi: int = Q31_MAX,
                name: str = "value") -> None:
    """Raise OverflowError if any element of x is outside [lo, hi].

    Use at stage boundaries -- it catches overflow that NumPy would
    otherwise wrap silently.
    """
    arr = _i64(x)
    if arr.size == 0:
        return
    mn, mx = int(arr.min()), int(arr.max())
    if mn < lo or mx > hi:
        raise OverflowError(f"{name} out of range [{lo}, {hi}]: min={mn}, max={mx}")
