"""E2 -- Step response vs filter order (full Q31 chain).

A 1 kHz tone switches on at t0. Theory (n identical poles):
    y(t) = 1 - exp(-t/tau) * sum_{k<n} (t/tau)^k / k!
99% settling: 4.61, 6.64, 8.41, 10.05 tau.
"""
import math

import numpy as np

from lockin import pipeline, signals
from experiments.common import Row, plt, savefig

T99 = {1: 4.61, 2: 6.64, 3: 8.41, 4: 10.05}


def step_theory(t_over_tau, n):
    s = sum(t_over_tau ** k / math.factorial(k) for k in range(n))
    return 1 - np.exp(-t_over_tau) * s


def run():
    # f_ref = 10 kHz: with a single pole, the mixer's 2f ripple leaking through
    # (~1/(2*pi*2f*tau)) shifts the 99% crossing. At 1 kHz that is ~0.15 tau;
    # at 10 kHz it is negligible. (Higher orders suppress it anyway.)
    tau, t0, a = 0.05, 0.2, 0.5
    rows = []
    fig, ax = plt.subplots(figsize=(6, 4))
    for order in range(1, 5):
        cfg = pipeline.Config(f_ref=10_000.0, tau=tau, order=order, m=16)
        n = int((t0 + 14 * tau) * cfg.fs)
        t = signals.sample_times(cfg.fs, n)
        v = signals.tones(t, [(a, cfg.f_actual, 0.0)]) - signals.VREF / 2
        v[t < t0] = 0.0
        r = pipeline.run_q31(signals.adc(v + signals.VREF / 2), cfg)
        x = r.x.astype(float)
        final = x[-50:].mean()
        tt = r.t_out - t0
        resp = x / final
        idx = np.flatnonzero((tt > 0) & (resp >= 0.99))
        t99 = tt[idx[0]] / tau
        ax.plot(tt / tau, resp, label=f"order {order} (99% at {t99:.2f}τ)")
        ax.plot(tt[tt > 0] / tau, step_theory(tt[tt > 0] / tau, order), "k:", lw=0.8)
        rows.append(Row("E2", f"99% settling, order {order}", f"{T99[order]} τ",
                        f"{t99:.2f} τ", abs(t99 / T99[order] - 1) < 0.03))
    ax.axhline(0.99, color="grey", lw=0.6)
    ax.set_xlim(-1, 13)
    ax.set_xlabel("time since signal on (τ)")
    ax.set_ylabel("X / final value")
    ax.set_title("E2: step response (dotted = theory)")
    ax.legend(fontsize=8)
    savefig(fig, "e2_settling")
    return rows


if __name__ == "__main__":
    for r in run():
        print(r)
