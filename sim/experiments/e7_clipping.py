"""E7 -- ADC clipping sets the dynamic reserve.

Signal: 0.1 V peak at 1 kHz. Interferer: 1.37 kHz, amplitude swept.
The lock-in rejects the interferer completely (it is 370 Hz away and the
filter is 24 dB/oct, tau = 100 ms) -- until signal + interferer peaks leave
the ADC's 0..3.3 V range. Predicted onset: A_sig + A_int > VREF/2.
"""
import numpy as np

from lockin import meas, pipeline, signals
from experiments.common import Row, plt, savefig


def run():
    cfg = pipeline.Config(f_ref=1000.0, tau=0.1, order=4, m=64)
    a_sig, f_int = 0.1, 1370.0
    amps = np.unique(np.concatenate([np.logspace(np.log10(0.05), np.log10(3.0), 20),
                                     np.linspace(1.40, 1.70, 31)]))   # 10 mV steps near onset
    n = int((pipeline.settle_time(cfg) + 0.5) * cfg.fs)
    t = signals.sample_times(cfg.fs, n)
    err, clip = [], []
    for ai in amps:
        codes, nc = signals.adc(signals.tones(t, [(a_sig, cfg.f_actual, 0.0), (ai, f_int, 0.0)]),
                                return_clips=True)
        r = pipeline.run_q31(codes, cfg)
        R, _ = meas.polar_q31(r.x[-100:], r.y[-100:])
        err.append(meas.q31_to_volts_rms(R).mean() / (a_sig / np.sqrt(2)) - 1)
        clip.append(nc / n)
    err, clip = np.abs(np.array(err)), np.array(clip)
    onset_pred = signals.VREF / 2 - a_sig
    clipped = amps[clip > 0]
    onset_meas = clipped[0] if len(clipped) else np.nan
    bad = amps[err > 0.01]
    err_1pct = bad[0] if len(bad) else np.nan

    fig, ax = plt.subplots(figsize=(6, 4))
    ax.loglog(amps, np.maximum(err, 1e-7) * 100, "o-", ms=3, label="|R error| (%)")
    ax.loglog(amps, np.maximum(clip, 1e-7) * 100, "s-", ms=3, label="samples clipped (%)")
    ax.axvline(onset_pred, color="r", ls="--", label=f"predicted clipping onset {onset_pred:.2f} V")
    ax.axhline(1, color="grey", ls=":", lw=0.8)
    ax.set_xlabel("interferer amplitude (V peak)")
    ax.set_ylabel("%")
    ax.set_title("E7: dynamic reserve is set by ADC headroom")
    ax.legend(fontsize=8)
    savefig(fig, "e7_clipping")

    ok_before = err[amps < onset_meas].max()
    return [
        Row("E7", "R error before clipping onset", "≈ 0 (interferer fully rejected)",
            f"max {ok_before*100:.3f}%", ok_before < 1e-3),
        Row("E7", "Interferer amplitude where clipping starts",
            f"{onset_pred:.2f} V (A_sig + A_int = VREF/2)", f"{onset_meas:.2f} V",
            bool(abs(onset_meas - onset_pred) <= 0.011)),
        Row("E7", "Interferer amplitude where R error exceeds 1%", "(above onset: mild clipping "
            "distorts R only slowly)", f"{err_1pct:.2f} V", None),
        Row("E7", "Dynamic reserve for this signal", "20·log10(A_int,max / A_sig)",
            f"{20*np.log10(onset_meas/a_sig):.0f} dB  (no AFE gain: ≈ 0 dB relative to full scale)", None),
    ]


if __name__ == "__main__":
    for r in run():
        print(r)
