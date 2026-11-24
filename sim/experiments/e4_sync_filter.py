"""E4 -- 2f ripple at low reference frequency, with and without the sync filter.

f_ref = 10.3 Hz (fs/f_ref = 9708.7 samples: NOT an integer, so the sync filter
cannot be perfect). Pure sine input, order-1 filter.
Theory (IIR only): ripple / DC = |H(2f)| = 1 / sqrt(1 + (2*pi*2f*tau)^2).
"""
import numpy as np

from lockin import pipeline, signals
from experiments.common import Row, plt, savefig


def ripple(cfg, a=0.5, seconds_after_settle=3.0):
    # wait 12 tau, not the 99% time: a residual exponential transient inside the
    # window would be mistaken for ripple (it was, at tau = 1 s, in the first run)
    t_settle = 12 * cfg.tau + 1.0
    n = int((t_settle + seconds_after_settle) * cfg.fs)
    t = signals.sample_times(cfg.fs, n)
    r = pipeline.run_q31(signals.adc(signals.tones(t, [(a, cfg.f_actual, 0.0)])), cfg)
    x = r.x[r.t_out > t_settle].astype(float)
    return (x.max() - x.min()) / 2 / abs(x.mean())


def run():
    f_ref, order = 10.3, 1
    taus = [0.03, 0.1, 0.3, 1.0]
    iir, sync = [], []
    for tau in taus:
        iir.append(ripple(pipeline.Config(f_ref=f_ref, tau=tau, order=order, m=64)))
        sync.append(ripple(pipeline.Config(f_ref=f_ref, tau=tau, order=order, sync=True)))
    f2 = 2 * f_ref
    tt = np.logspace(np.log10(0.02), 0.1, 60)
    theory = 1 / np.sqrt(1 + (2 * np.pi * f2 * tt) ** 2)

    fig, ax = plt.subplots(figsize=(6, 4))
    ax.loglog(tt, theory, "k-", lw=0.8, label="theory, IIR only (1 pole)")
    ax.loglog(taus, iir, "o", label="measured, IIR only")
    ax.loglog(taus, np.maximum(sync, 1e-9), "s", label="measured, sync filter + IIR")
    ax.set_xlabel("τ (s)")
    ax.set_ylabel("2f ripple / DC on X")
    ax.set_title(f"E4: 2f ripple at f_ref = {f_ref} Hz, 6 dB/oct")
    ax.legend(fontsize=8)
    savefig(fig, "e4_sync_filter")

    th = 1 / np.sqrt(1 + (2 * np.pi * f2 * np.array(taus)) ** 2)
    dev = np.max(np.abs(np.array(iir) / th - 1))
    improvement = 20 * np.log10(np.min(np.array(iir) / np.array(sync)))
    return [
        Row("E4", "2f ripple, IIR only (4 τ values)", "1/√(1+(4πfτ)²)",
            f"worst deviation {dev*100:.1f}%", dev < 0.05),
        Row("E4", "Sync filter ripple reduction (worst case over τ)", "large (non-integer period → not ∞)",
            f"≥ {improvement:.0f} dB", improvement > 20),
    ]


if __name__ == "__main__":
    for r in run():
        print(r)
