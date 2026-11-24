"""E6 -- Q31 vs float error at each stage, against the ADC's own resolution.

Both models get the same quantised ADC input, the same integer NCO phases and
the same alpha, so the difference is purely fixed-point arithmetic.
Prediction: each stage ends in one round-to-nearest, so the error is about
uniform on [-0.5, 0.5] LSB -> RMS ~ 1/sqrt(12) = 0.29 Q31 LSB, i.e. ~2^-19 of
an ADC LSB. The ADC, not the arithmetic, sets the resolution.
"""
import numpy as np

from lockin import pipeline, q31
from experiments.common import ADC_LSB_Q31, Row, adc_codes, plt, savefig

STAGES = [("mix_i", "mixer"), ("dec_i", "decimator"), ("x", "IIR output X")]


def errors(cfg, codes):
    rq = pipeline.run_q31(codes, cfg)
    rf = pipeline.run_float(q31.to_float(q31.adc_to_q31(codes)), cfg)
    out = {}
    for k, _ in STAGES:
        d = rq.stages[k] - rf.stages[k] * q31.Q31_ONE
        out[k] = (np.sqrt(np.mean(d ** 2)), np.max(np.abs(d)))
    return out


def run():
    rng = np.random.default_rng(6)
    cfgs = {"boxcar M=16, order 4": pipeline.Config(f_ref=1234.0, tau=0.01, order=4, m=16),
            "sync filter, order 4": pipeline.Config(f_ref=1234.0, tau=0.01, order=4, sync=True)}
    fig, ax = plt.subplots(figsize=(6, 4))
    rows, width = [], 0.38
    for j, (name, cfg) in enumerate(cfgs.items()):
        codes = adc_codes(cfg.fs, 1_000_000, [(0.4, cfg.f_actual, 0.3)], noise_rms=0.2, rng=rng)
        e = errors(cfg, codes)
        rms = [e[k][0] for k, _ in STAGES]
        ax.bar(np.arange(len(STAGES)) + (j - 0.5) * width, rms, width, label=name)
        worst_max = max(e[k][1] for k, _ in STAGES)
        rows.append(Row("E6", f"Q31 vs float, {name}: RMS error per stage",
                        "≈ 0.29 LSB (1/√12)",
                        ", ".join(f"{lbl} {e[k][0]:.2f}" for k, lbl in STAGES) + " LSB",
                        all(0.2 < r < 0.45 for r in rms)))
        rows.append(Row("E6", f"Q31 vs float, {name}: max error", "≤ 1 LSB",
                        f"{worst_max:.2f} Q31 LSB = {worst_max / ADC_LSB_Q31:.1e} ADC LSB",
                        worst_max <= 1.0))
    ax.axhline(1 / np.sqrt(12), color="k", ls="--", lw=0.8, label="1/√12 (prediction)")
    ax.set_xticks(range(len(STAGES)), [lbl for _, lbl in STAGES])
    ax.set_ylabel("RMS error (Q31 LSB)")
    ax.set_title("E6: fixed-point error per stage  (1 ADC LSB = 524 288 Q31 LSB)")
    ax.legend(fontsize=8)
    savefig(fig, "e6_q31_error")
    return rows


if __name__ == "__main__":
    for r in run():
        print(r)
