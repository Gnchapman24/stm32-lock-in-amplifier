"""E5 -- IIR dead band: naive 32-bit state vs. wide (64-bit) state.

Naive update y += mul(alpha, x - y) rounds alpha*e/2^31 to zero once
|e| < 2^30 / alpha, so the output stalls that far from the input.
alpha = 2^-20 (tau ~ 10.5 s at 100 kS/s, i.e. no decimation) gives a predicted
dead band of 2^30 / 2^11 = 2^19 Q31 units = exactly 1 ADC LSB.
"""
import numpy as np

from lockin import demod
from experiments.common import ADC_LSB_Q31, Row, plt, savefig


def run():
    fs = 100_000.0
    alpha = 1 << 11                                  # 2**-20 in Q31
    target = 4 * ADC_LSB_Q31                         # 4 ADC LSB step
    n = 6_000_000                                    # 60 s
    x = np.full(n, target, dtype=np.int64)
    naive = demod.iir_q31_naive(x, alpha, 1)
    wide = demod.iir_q31(x, alpha, 1)
    pred = (1 << 30) // alpha
    t = np.arange(n) / fs

    fig, ax = plt.subplots(figsize=(6, 4))
    k = slice(0, n, 500)
    ax.semilogy(t[k], np.maximum(target - naive[k], 0.5) / ADC_LSB_Q31, label="naive 32-bit state")
    ax.semilogy(t[k], np.maximum(np.abs(target - wide[k]), 0.5) / ADC_LSB_Q31, label="64-bit state")
    ax.axhline(pred / ADC_LSB_Q31, color="r", ls="--", label="predicted dead band 2³⁰/α")
    ax.axhline(1 / ADC_LSB_Q31, color="grey", ls=":", label="1 Q31 LSB")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("|input − output| (ADC LSB)")
    ax.set_title("E5: IIR dead band, α = 2⁻²⁰ (τ ≈ 10.5 s, no decimation)")
    ax.legend(fontsize=8)
    savefig(fig, "e5_dead_band")

    stall = int(target - naive[-1])
    wide_err = int(abs(target - wide[-1]))
    # Wide state: still decaying exponentially, straight through the dead band.
    # After n samples the ideal error is target * (1 - alpha)^n.
    wide_pred = target * (1 - alpha / 2**31) ** n
    return [
        Row("E5", "Naive IIR stall offset", f"≤ 2³⁰/α = {pred} Q31 (1 ADC LSB)",
            f"{stall} Q31 ({stall / ADC_LSB_Q31:.3f} ADC LSB)", 0.9 * pred < stall <= pred),
        Row("E5", "Wide-state IIR error after 60 s (5.7τ)",
            f"target·(1−α)ⁿ = {wide_pred:.0f} Q31 (below the dead band, still decaying)",
            f"{wide_err} Q31 LSB", abs(wide_err / wide_pred - 1) < 0.01),
        Row("E5", "Same τ with decimation M=64", "α 64× larger → dead band 1/64 ADC LSB",
            "(design note: decimating first also shrinks the dead band)", None),
    ]


if __name__ == "__main__":
    for r in run():
        print(r)
