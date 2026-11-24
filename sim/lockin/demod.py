"""Demodulation chain: mixer -> decimator -> IIR cascade (+ optional sync filter).

Each stage has a float version (ground truth) and a Q31 version (bit-exact
model of the firmware). The Q31 IIR and sync filter are plain loops over
Python ints so they read like the C they will become.
"""
from __future__ import annotations

import math

import numpy as np
from numpy.typing import ArrayLike, NDArray
from scipy import signal as sps

from . import q31

Floats = NDArray[np.float64]


# ---- mixer -----------------------------------------------------------------

def mix_float(x: ArrayLike, sin_ref: ArrayLike, cos_ref: ArrayLike) -> tuple[Floats, Floats]:
    """Return (i, q) = (x*sin, x*cos), float64."""
    x = np.asarray(x, dtype=np.float64)
    return x * np.asarray(sin_ref, dtype=np.float64), x * np.asarray(cos_ref, dtype=np.float64)


def mix_q31(x: ArrayLike, sin_ref: ArrayLike, cos_ref: ArrayLike) -> tuple[q31.Q31Array, q31.Q31Array]:
    """Return (i, q) in Q31 via q31.mul.

    x in [-0.5, 0.5) (ADC) and ref in [-1, 1) -> products in [-0.5, 0.5]:
    saturation can't trigger here, and the next stages keep the 0.5 headroom.
    """
    return q31.mul(x, sin_ref), q31.mul(x, cos_ref)


# ---- decimator ---------------------------------------------------------------

def _check_m(m: int) -> int:
    if m < 1 or m & (m - 1):
        raise ValueError("decimation factor m must be a power of two >= 1")
    return m.bit_length() - 1


def boxcar_float(x: ArrayLike, m: int) -> Floats:
    """Average non-overlapping blocks of m samples (trailing partial block dropped)."""
    _check_m(m)
    x = np.asarray(x, dtype=np.float64)
    nb = len(x) // m
    return x[: nb * m].reshape(nb, m).mean(axis=1)


def boxcar_q31(x: ArrayLike, m: int) -> q31.Q31Array:
    """Sum blocks of m in int64, then shift right by log2(m) with rounding.

    m is restricted to powers of two so normalisation is an exact shift.
    m <= 2**16 keeps the int64 sum far from overflow (2**31 * 2**16 = 2**47).
    """
    sh = _check_m(m)
    if m > 1 << 16:
        raise ValueError("m too large")
    x = np.asarray(x, dtype=np.int64)
    nb = len(x) // m
    sums = x[: nb * m].reshape(nb, m).sum(axis=1)
    return q31.shr_round(sums, sh)


# ---- IIR low-pass cascade ----------------------------------------------------

def alpha_for(tau: float, t_sample: float) -> float:
    """alpha = 1 - exp(-T/tau), computed in float on the host."""
    if tau <= 0 or t_sample <= 0:
        raise ValueError("tau and t_sample must be > 0")
    return -math.expm1(-t_sample / tau)


def alpha_q31_for(tau: float, t_sample: float) -> int:
    """alpha as a Q31 integer (>= 1). The firmware receives this, not tau."""
    return max(1, int(q31.from_float(alpha_for(tau, t_sample))))


def iir_float(x: ArrayLike, alpha: float, order: int) -> Floats:
    """`order` cascaded single-pole low-pass stages, float64, zero initial state.

    y[n] = y[n-1] + alpha*(x[n] - y[n-1])  <=>  b=[alpha], a=[1, alpha-1]
    """
    y = np.asarray(x, dtype=np.float64)
    for _ in range(order):
        y = sps.lfilter([alpha], [1.0, alpha - 1.0], y)
    return y


def iir_q31_naive(x: ArrayLike, alpha_q31: int, order: int) -> q31.Q31Array:
    """32-bit state, y += mul(alpha, x - y). Exists to demonstrate the dead band (E5).

    mul() rounds alpha*e/2**31 to zero whenever |e| < 2**30 / alpha, so the
    output stalls up to that far from a constant input.
    """
    ys = [0] * order
    out = np.empty(len(x), dtype=np.int64)
    half = 1 << 30
    for n, v in enumerate(np.asarray(x, dtype=np.int64).tolist()):
        for k in range(order):
            p = alpha_q31 * (v - ys[k])
            ys[k] = ys[k] + ((p + half) >> 31)
            v = ys[k]
        out[n] = v
    return out


def iir_q31(x: ArrayLike, alpha_q31: int, order: int,
            state: list[int] | None = None) -> q31.Q31Array:
    """Wide-state IIR cascade (fixes the dead band).

    Per stage, state s is int64 in Q62 (value * 2**62):
        y  = (s + 2**30) >> 31          # rounded Q31 view of the state
        e  = x - y                      # int32: |x|,|y| <= 0.5 -> |e| < 1
        s += alpha * e                  # ARM: SMLAL (64-bit multiply-accumulate)
        x  = (s + 2**30) >> 31          # output, feeds the next stage
    The increment alpha*e is never shifted down, so nothing is lost to
    rounding; the output converges to within 0.5 LSB of a constant input.
    Range: each stage output is a convex combination of its inputs, so
    |y| <= max|x| <= 0.5 and |s| < 2**61 -- no overflow.

    `state` (list of `order` ints) is updated in place if given, so a long
    signal can be processed in blocks exactly like the firmware does.
    """
    if not 1 <= alpha_q31 <= q31.Q31_MAX:
        raise ValueError("alpha_q31 out of range")
    s = state if state is not None else [0] * order
    if len(s) != order:
        raise ValueError("state length must equal order")
    half = 1 << 30
    out = np.empty(len(x), dtype=np.int64)
    for n, v in enumerate(np.asarray(x, dtype=np.int64).tolist()):
        for k in range(order):
            e = v - ((s[k] + half) >> 31)
            s[k] += alpha_q31 * e
            v = (s[k] + half) >> 31
        out[n] = v
    return out


# ---- sync filter -------------------------------------------------------------

def period_bounds(phase: ArrayLike) -> NDArray[np.int64]:
    """Indices where a new reference period starts (phase wrapped: p[n] < p[n-1])."""
    p = np.asarray(phase, dtype=np.uint32)
    return np.flatnonzero(p[1:] < p[:-1]) + 1


def sync_float(x: ArrayLike, phase: ArrayLike) -> Floats:
    """Average x over each complete reference period (between phase wraps)."""
    x = np.asarray(x, dtype=np.float64)
    b = period_bounds(phase)
    return np.array([x[b[i]:b[i + 1]].mean() for i in range(len(b) - 1)])


def sync_q31(x: ArrayLike, phase: ArrayLike) -> q31.Q31Array:
    """Q31 average over each complete period.

    Samples per period vary by +-1 when fs/f_ref isn't an integer, so the
    int64 sum is divided by the actual count with q31.div_round (round to
    nearest, halves away from zero -- mirrors the C expression). This is
    one division per reference period, not per sample.
    """
    x = np.asarray(x, dtype=np.int64)
    b = period_bounds(phase)
    out = np.empty(max(len(b) - 1, 0), dtype=np.int64)
    for i in range(len(b) - 1):
        seg = x[b[i]:b[i + 1]]
        out[i] = q31.div_round(int(seg.sum()), len(seg))
    return out
