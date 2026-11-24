"""Export test vectors for the firmware's host-side C unit tests.

Layout per vector set:
    vectors/<name>/config.json        Config fields + derived delta, alpha_q31, ...
                                      + "files": {stage: {"len": n, "dtype": "int32"}}
    vectors/<name>/<stage>.bin        little-endian int32, one per stage

Stages: adc, phase (uint32 stored as its int32 bit pattern), sin, cos, x_in,
mix_i, mix_q, dec_i, dec_q, x, y. Stage-by-stage files let a failing C test
say WHICH stage diverged; the C test can also feed sin/cos from the file so
everything after the NCO is compared bit-exactly (see Q31-RULES.md section 8).
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from numpy.typing import ArrayLike

from . import q31


def write_bin(path: str | Path, arr: ArrayLike) -> int:
    """Write an integer array as little-endian int32 (range-checked). Returns length.

    uint32 arrays (NCO phase) are stored as their int32 bit pattern.
    """
    a = np.asarray(arr)
    if a.dtype == np.uint32:
        a = a.view(np.int32)
    elif not np.issubdtype(a.dtype, np.integer):
        raise TypeError("only integer arrays can be exported")
    else:
        q31.check_range(a, name=str(path))
    a.astype("<i4").tofile(path)
    return int(a.size)


def read_bin(path: str | Path) -> np.ndarray:
    """Read a little-endian int32 file back (as int64, for Python-side checks)."""
    return np.fromfile(path, dtype="<i4").astype(np.int64)


def export(name: str, config: dict, stages: dict, out_dir: str | Path = "vectors") -> Path:
    """config: dict -> config.json; stages: dict[str, int array] -> <key>.bin."""
    d = Path(out_dir) / name
    d.mkdir(parents=True, exist_ok=True)
    files = {}
    for key, arr in stages.items():
        n = write_bin(d / f"{key}.bin", arr)
        files[key] = {"len": n, "dtype": "int32"}
    meta = dict(config)
    meta["files"] = files
    (d / "config.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    return d
