"""Generate the bit-exact test vectors for the firmware's host-side C tests.

    cd sim
    python -m experiments.export_vectors        -> sim/vectors/<case>/

Deterministic (fixed seeds): re-running produces identical files, so a diff in
git means the model's behaviour changed.
"""
from pathlib import Path

import numpy as np

from lockin import dac, pipeline, q31, vectors
from experiments.common import adc_codes

OUT = Path(__file__).resolve().parents[1] / "vectors"
N = 4096

CASES = {
    # name: (config, components [(A_peak V, f, phase)], noise V rms, description)
    "basic_1k": (pipeline.Config(f_ref=1000.0, tau=0.002, order=2, m=16),
                 [(0.4, None, 0.5)], 0.05, "1 kHz sine + noise, boxcar M=16, 2 poles"),
    "sync_1030": (pipeline.Config(f_ref=1030.0, tau=0.005, order=3, sync=True),
                  [(0.3, None, -1.2)], 0.05, "sync filter, ~97.1 samples/period, 3 poles"),
    "harm2_order4": (pipeline.Config(f_ref=500.0, harmonic=2, tau=0.002, order=4, m=8),
                     [(0.3, None, 0.0), (0.1, "2f", 0.7)], 0.02, "2nd harmonic, 4 poles, M=8"),
    "clip_edge": (pipeline.Config(f_ref=2000.0, tau=0.001, order=1, m=32, phase0=0x9E3779B9),
                  [(2.0, None, 0.3)], 0.0, "input clipped at both rails, nonzero phase0, 1 pole"),
}


# DAC code mapping (D30). Not a pipeline case: config.json has "kind": "dac_map",
# and sin.bin is the INPUT (on hardware it comes from the CORDIC, which the model
# can't reproduce bit-exactly, D12). The sine sweep is followed by hand-picked
# edge values: full scale, zero, and the exact rounding halves at +-2^24.
DAC_CASE = "dac_map"
DAC_CFG = {"kind": "dac_map", "fs": 100_000.0, "f_ref": 1234.5, "phase0": 0x40000000,
           "n_sweep": N, "amp": dac.DAC_AMP, "dac_mid": dac.DAC_MID,
           "edge_sin": [q31.Q31_MIN, q31.Q31_MAX, 0, 1, -1,
                        1 << 24, -(1 << 24), (1 << 24) - 1, -(1 << 24) - 1],
           "description": "Q31 sine sweep + edge values -> 12-bit DAC codes"}


def main():
    for i, (name, (cfg, comps, noise, desc)) in enumerate(CASES.items()):
        tones = [(a, cfg.f_actual if f is None else 2 * cfg.f_actual, p) for a, f, p in comps]
        codes = adc_codes(cfg.fs, N, tones, noise_rms=noise, rng=np.random.default_rng(100 + i))
        r = pipeline.run_q31(codes, cfg)
        meta = cfg.as_dict()
        meta.update(description=desc, adc_bits=12, n_samples=N,
                    input_tones=[list(t) for t in tones], input_noise_v_rms=noise)
        d = vectors.export(name, meta, r.stages, out_dir=OUT)
        print(f"{name:14s} -> {d}  ({len(r.x)} outputs)")

    st = dac.vector_stages(DAC_CFG)
    d = vectors.export(DAC_CASE, DAC_CFG, st, out_dir=OUT)
    print(f"{DAC_CASE:14s} -> {d}  ({len(st['dac'])} codes)")


if __name__ == "__main__":
    main()
