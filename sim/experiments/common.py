"""Shared helpers for the Phase 0 experiments."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

from lockin import demod, nco, q31, signals  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "results" / "phase0"
VOLT_TO_Q31 = q31.Q31_ONE / signals.VREF        # Q31 units per volt
ADC_LSB_Q31 = 1 << 19                          # one 12-bit ADC code in Q31 units

plt.rcParams.update({"figure.dpi": 110, "axes.grid": True, "grid.alpha": 0.3,
                     "font.size": 9})


@dataclass
class Row:
    exp: str
    item: str
    theory: str
    measured: str
    ok: bool | None = None      # None = informational

    def __post_init__(self):
        # experiments often produce numpy.bool_, which fails `is True` checks
        if self.ok is not None:
            self.ok = bool(self.ok)

    @property
    def verdict(self) -> str:
        return {True: "pass", False: "**FAIL**", None: "—"}[self.ok]


def savefig(fig, name: str) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    p = OUT / f"{name}.png"
    fig.tight_layout()
    fig.savefig(p)
    plt.close(fig)
    return p


def adc_codes(fs: float, n: int, components, noise_rms: float = 0.0,
              rng: np.random.Generator | None = None):
    """Mid-rail-biased tones (+ noise) through the 12-bit ADC model."""
    t = signals.sample_times(fs, n)
    v = signals.tones(t, components)
    if noise_rms:
        v = v + signals.white_noise(n, noise_rms, rng or np.random.default_rng(0))
    return signals.adc(v)


def mix_and_decimate_q31(codes, delta: int, m: int, phase0: int = 0):
    """Q31 front half of the chain (shared across many IIR settings)."""
    ph = nco.phases(delta, len(codes), phase0)
    s, c = nco.sincos_q31(ph)
    mi, mq = demod.mix_q31(q31.adc_to_q31(codes), s, c)
    return demod.boxcar_q31(mi, m), demod.boxcar_q31(mq, m)


def fmt(x: float, unit: str = "", digits: int = 3) -> str:
    return f"{x:.{digits}g}{(' ' + unit) if unit else ''}"
