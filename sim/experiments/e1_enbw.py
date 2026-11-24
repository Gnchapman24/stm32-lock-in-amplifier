"""E1 -- Output noise vs time constant and filter order.

Input: white noise only (no signal). Theory: the X output's standard deviation
is sigma_in * sqrt(ENBW / fs), with ENBW = 1/(4tau), 1/(8tau), 3/(32tau),
5/(64tau) for 1..4 poles (continuous-time formulas).
"""
import numpy as np

from lockin import demod, nco
from experiments.common import (ADC_LSB_Q31, VOLT_TO_Q31, Row, adc_codes, fmt,
                                mix_and_decimate_q31, plt, savefig)

ENBW_K = {1: 1 / 4, 2: 1 / 8, 3: 3 / 32, 4: 5 / 64}


def run():
    fs, m, f_ref = 100_000.0, 64, 1000.0
    sigma_v = 0.3
    n = 4_000_000                                    # 40 s
    codes = adc_codes(fs, n, [], noise_rms=sigma_v, rng=np.random.default_rng(1))
    # total input noise in Q31 units, including ADC quantisation (LSB^2/12)
    sigma_q = np.sqrt((sigma_v * VOLT_TO_Q31) ** 2 + ADC_LSB_Q31 ** 2 / 12)
    di, dq = mix_and_decimate_q31(codes, nco.delta_for(f_ref, fs), m)
    t_dec = m / fs

    taus = [0.01, 0.03, 0.1]
    fig, ax = plt.subplots(figsize=(6, 4))
    worst, rows = 0.0, []
    for order, marker in zip(range(1, 5), "osD^"):
        enbw, meas_sd = [], []
        for tau in taus:
            a = demod.alpha_q31_for(tau, t_dec)
            skip = int(12 * tau / t_dec)             # discard the settling
            x = demod.iir_q31(di, a, order)[skip:]
            y = demod.iir_q31(dq, a, order)[skip:]
            sd = np.sqrt((x.astype(float).var() + y.astype(float).var()) / 2)
            b = ENBW_K[order] / tau
            pred = sigma_q * np.sqrt(b / fs)
            worst = max(worst, abs(sd / pred - 1))
            enbw.append(b)
            meas_sd.append(sd)
        ax.loglog(enbw, meas_sd, marker, label=f"measured, order {order}")
    bb = np.logspace(np.log10(0.5), np.log10(30), 50)
    ax.loglog(bb, sigma_q * np.sqrt(bb / fs), "k-", lw=1, label=r"theory $\sigma\sqrt{ENBW/f_s}$")
    ax.set_xlabel("ENBW (Hz)")
    ax.set_ylabel("std of X, Y (Q31 units)")
    ax.set_title("E1: output noise vs equivalent noise bandwidth")
    ax.legend(fontsize=8)
    savefig(fig, "e1_enbw")

    rows.append(Row("E1", "Output noise vs √ENBW (12 settings)",
                    "σ_X = σ_in·√(ENBW/f_s)",
                    f"worst deviation {worst * 100:.1f}%", worst < 0.15))
    return rows


if __name__ == "__main__":
    for r in run():
        print(r)
