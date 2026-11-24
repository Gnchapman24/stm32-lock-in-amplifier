"""Complete single-channel lock-in chain, in float and in bit-exact Q31.

    ADC codes -> Q31 -> mix with NCO sin/cos -> decimate (boxcar m, or sync)
              -> IIR cascade -> X, Y

Both versions consume the SAME integer NCO phases and the SAME alpha_q31, so
their difference is purely the fixed-point arithmetic (experiment E6).
Two channels: run each with the same Config and phase0 (they share the NCO).
"""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np
from numpy.typing import ArrayLike, NDArray

from . import demod, nco, q31


@dataclass(frozen=True)
class Config:
    fs: float = 100_000.0     # ADC sample rate per channel (170 MHz / 1700)
    f_ref: float = 1_000.0    # requested reference frequency
    harmonic: int = 1         # demodulate at k * f_ref
    tau: float = 0.01         # IIR time constant (s)
    order: int = 2            # cascaded poles (1..4)
    m: int = 16               # boxcar decimation (power of two); unused if sync
    sync: bool = False        # True: average over whole reference periods instead
    phase0: int = 0           # initial NCO phase

    def __post_init__(self):
        if not 1 <= self.order <= 4:
            raise ValueError("order must be 1..4")
        demod._check_m(self.m)
        nco.delta_for(self.f_ref, self.fs)          # validates f_ref
        if self.harmonic * self.f_ref >= self.fs / 2:
            raise ValueError("harmonic frequency must be below fs/2")

    @property
    def delta(self) -> int:
        return nco.delta_for(self.f_ref, self.fs)

    @property
    def f_actual(self) -> float:
        return nco.actual_freq(self.delta, self.fs)

    @property
    def t_iir(self) -> float:
        """Sample period of the IIR stage."""
        return 1.0 / self.f_actual if self.sync else self.m / self.fs

    @property
    def alpha_q31(self) -> int:
        return demod.alpha_q31_for(self.tau, self.t_iir)

    @property
    def alpha(self) -> float:
        """Exactly the quantised alpha, as float (so float model == Q31 coefficients)."""
        return self.alpha_q31 / q31.Q31_ONE

    def as_dict(self) -> dict:
        return {
            "fs": self.fs, "f_ref": self.f_ref, "f_actual": self.f_actual,
            "harmonic": self.harmonic, "tau": self.tau, "order": self.order,
            "m": self.m, "sync": self.sync, "phase0": self.phase0,
            "delta": self.delta, "alpha_q31": self.alpha_q31, "t_iir": self.t_iir,
        }


@dataclass
class Result:
    """All intermediate stages, so a mismatch can be located (and exported)."""
    stages: dict[str, NDArray] = field(default_factory=dict)
    t_out: NDArray[np.float64] | None = None   # time of each X/Y output sample (s)

    @property
    def x(self) -> NDArray:
        return self.stages["x"]

    @property
    def y(self) -> NDArray:
        return self.stages["y"]


def _references(cfg: Config, n: int):
    ph = nco.phases(cfg.delta, n, cfg.phase0)
    ph_k = nco.harmonic(ph, cfg.harmonic) if cfg.harmonic > 1 else ph
    return ph, ph_k


def _t_out(cfg: Config, ph, n_dec: int) -> NDArray[np.float64]:
    if cfg.sync:
        b = demod.period_bounds(ph)
        return b[1:n_dec + 1] / cfg.fs
    return (np.arange(n_dec) + 1) * cfg.m / cfg.fs


def run_q31(codes: ArrayLike, cfg: Config, bits: int = 12) -> Result:
    """Bit-exact firmware model. codes: unsigned ADC codes for ONE channel."""
    codes = np.asarray(codes, dtype=np.int64)
    ph, ph_k = _references(cfg, len(codes))
    x = q31.adc_to_q31(codes, bits)
    s, c = nco.sincos_q31(ph_k)
    mi, mq = demod.mix_q31(x, s, c)
    if cfg.sync:
        di, dq = demod.sync_q31(mi, ph), demod.sync_q31(mq, ph)
    else:
        di, dq = demod.boxcar_q31(mi, cfg.m), demod.boxcar_q31(mq, cfg.m)
    xo = demod.iir_q31(di, cfg.alpha_q31, cfg.order)
    yo = demod.iir_q31(dq, cfg.alpha_q31, cfg.order)
    for name, arr in (("x_in", x), ("mix_i", mi), ("mix_q", mq), ("dec_i", di),
                      ("dec_q", dq), ("x", xo), ("y", yo)):
        q31.check_range(arr, name=name)
    return Result(
        stages={"adc": codes, "phase": ph, "sin": s, "cos": c, "x_in": x,
                "mix_i": mi, "mix_q": mq, "dec_i": di, "dec_q": dq, "x": xo, "y": yo},
        t_out=_t_out(cfg, ph, len(xo)),
    )


def run_float(x: ArrayLike, cfg: Config) -> Result:
    """Ideal model. x: input in Q31 scale as float (e.g. q31.to_float(adc_to_q31(codes)),
    or unquantised volts converted with (v - VREF/2) / VREF)."""
    x = np.asarray(x, dtype=np.float64)
    ph, ph_k = _references(cfg, len(x))
    s, c = nco.sincos_float(ph_k)
    mi, mq = demod.mix_float(x, s, c)
    if cfg.sync:
        di, dq = demod.sync_float(mi, ph), demod.sync_float(mq, ph)
    else:
        di, dq = demod.boxcar_float(mi, cfg.m), demod.boxcar_float(mq, cfg.m)
    xo = demod.iir_float(di, cfg.alpha, cfg.order)
    yo = demod.iir_float(dq, cfg.alpha, cfg.order)
    return Result(
        stages={"phase": ph, "sin": s, "cos": c, "x_in": x, "mix_i": mi, "mix_q": mq,
                "dec_i": di, "dec_q": dq, "x": xo, "y": yo},
        t_out=_t_out(cfg, ph, len(xo)),
    )


def settle_time(cfg: Config) -> float:
    """Time for the IIR cascade to settle to 99% (continuous-time approximation)."""
    return {1: 4.61, 2: 6.64, 3: 8.41, 4: 10.05}[cfg.order] * cfg.tau
