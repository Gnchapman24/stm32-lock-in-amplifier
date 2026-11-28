"""DAC excitation: Q31 sine -> 12-bit DAC code (decision D30).

    code = DAC_MID + ((sin_q31 * amp + 2**30) >> 31)

The multiply rounds half up, exactly like q31.mul (D6, Q31-RULES section 1),
but the result is a code offset, not a Q31 value, so there is no saturation:
|sin| <= 1 keeps the offset within +-amp. The firmware does this in C on the
CORDIC output; the C host test checks it against the exported vector.
"""
from __future__ import annotations

import numpy as np
from numpy.typing import ArrayLike, NDArray

from . import nco

DAC_BITS = 12
DAC_MID = 1 << (DAC_BITS - 1)      # 2048: mid-scale, 0 V of the excitation
DAC_AMP = 1600                     # peak amplitude in codes (~1.29 V peak at 3.3 V)

# The buffered output only swings 0.2 V .. VREF+ - 0.2 V (DS12288), i.e. codes
# 248 .. 3847 at 3.3 V. D30 keeps the sine inside that window.
DAC_CODE_MIN = 248
DAC_CODE_MAX = 3847

_HALF = 1 << 30


def check_amp(amp: int) -> None:
    """Raise ValueError unless DAC_MID +- amp stays inside the linear window."""
    if amp < 0 or DAC_MID - amp < DAC_CODE_MIN or DAC_MID + amp > DAC_CODE_MAX:
        raise ValueError(f"amp {amp} leaves the DAC window {DAC_CODE_MIN}..{DAC_CODE_MAX}")


def code_from_sin(sin_q31: ArrayLike, amp: int = DAC_AMP) -> NDArray[np.int64]:
    """Q31 sine values -> DAC codes. C: DAC_MID + (int32_t)(((int64_t)s * amp + (1 << 30)) >> 31)."""
    check_amp(amp)
    s = np.asarray(sin_q31, dtype=np.int64)
    return DAC_MID + ((s * amp + _HALF) >> 31)


def vector_stages(cfg: dict) -> dict:
    """Stages of a "kind": "dac_map" test vector (export_vectors.py, test_vectors.py).

    sin: an NCO sine sweep (n_sweep samples of f_ref from phase0), then edge_sin.
    dac: code_from_sin(sin, amp).
    """
    ph = nco.phases(nco.delta_for(cfg["f_ref"], cfg["fs"]), cfg["n_sweep"], cfg["phase0"])
    s, _ = nco.sincos_q31(ph)
    s = np.concatenate([s, np.asarray(cfg["edge_sin"], dtype=np.int64)])
    return {"sin": s, "dac": code_from_sin(s, cfg["amp"])}
