"""Numerically controlled oscillator: 32-bit phase accumulator.

Phase is uint32; 2**32 == one cycle == 2*pi. Reinterpreted as int32 it is
exactly the STM32G4 CORDIC q1.31 angle encoding (+-1 <-> +-pi).
"""
from __future__ import annotations

import numpy as np
from numpy.typing import ArrayLike, NDArray

from . import q31

PHASE_BITS = 32
PHASE_MOD = 1 << PHASE_BITS
_MASK = PHASE_MOD - 1

Phase = NDArray[np.uint32]


def delta_for(f_ref: float, fs: float) -> int:
    """Phase increment for f_ref at sample rate fs, rounded to nearest.

    Requires 0 < f_ref < fs/2 (below Nyquist).
    """
    if fs <= 0:
        raise ValueError("fs must be > 0")
    if not 0 < f_ref < fs / 2:
        raise ValueError("f_ref must be in (0, fs/2)")
    return int(np.floor(f_ref / fs * PHASE_MOD + 0.5))


def actual_freq(delta: int, fs: float) -> float:
    """Frequency actually produced by a given increment."""
    return delta * fs / PHASE_MOD


def phases(delta: int, n: int, phase0: int = 0) -> Phase:
    """n consecutive uint32 phases starting at phase0 (wraps mod 2**32).

    C: phase += delta  (uint32_t arithmetic wraps for free).
    """
    k = np.arange(n, dtype=np.uint64)
    # delta < 2**32 and k < 2**32, so delta*k < 2**64: no uint64 overflow.
    return ((np.uint64(phase0 & _MASK) + np.uint64(delta & _MASK) * k)
            & np.uint64(_MASK)).astype(np.uint32)


def harmonic(phase: ArrayLike, k: int) -> Phase:
    """Phase of the k-th harmonic: (k * phase) mod 2**32.  C: uint32_t multiply."""
    if k < 1:
        raise ValueError("harmonic k must be >= 1")
    p = np.asarray(phase, dtype=np.uint64)
    return ((p * np.uint64(k)) & np.uint64(_MASK)).astype(np.uint32)


def to_cordic_angle(phase: ArrayLike) -> NDArray[np.int32]:
    """uint32 phase -> int32 q1.31 angle (a reinterpretation, not arithmetic)."""
    return np.asarray(phase, dtype=np.uint32).view(np.int32)


def sincos_float(phase: ArrayLike) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Ideal (sin, cos) as float64 -- the float model's reference."""
    theta = 2 * np.pi * np.asarray(phase, dtype=np.float64) / PHASE_MOD
    return np.sin(theta), np.cos(theta)


def sincos_q31(phase: ArrayLike) -> tuple[q31.Q31Array, q31.Q31Array]:
    """(sin, cos) in Q31: ideal values rounded to nearest, saturating (+1 -> MAX).

    This is option B of the Phase 0 guide: the hardware CORDIC's exact rounding
    is undocumented, so the model uses the ideal rounded value and the firmware
    is compared against it with a small tolerance AT THIS STAGE ONLY. Test
    vectors include these sin/cos arrays, so every later stage can be checked
    bit-exactly by feeding the C code the same references.
    """
    s, c = sincos_float(phase)
    return q31.from_float(s), q31.from_float(c)
