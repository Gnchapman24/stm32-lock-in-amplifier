"""E3 -- Processing gain: recovering a signal buried in broadband noise.

Input SNR (power) = (A^2/2) / sigma^2 over the full Nyquist band fs/2.
Output SNR on X (phase 0) = (A/2)^2 / var(X). Theory:
    SNR_out = SNR_in + 10 log10( (fs/2) / ENBW )
"""
import numpy as np

from lockin import demod, meas, nco, pipeline, signals
from experiments.common import (VOLT_TO_Q31, ADC_LSB_Q31, Row, adc_codes,
                                mix_and_decimate_q31, plt, savefig)

ENBW_K = {1: 1 / 4, 2: 1 / 8, 3: 3 / 32, 4: 5 / 64}


def run():
    fs, m, f_ref, sigma_v, order = 100_000.0, 64, 1000.0, 0.3, 2
    n = 4_000_000                                        # 40 s
    delta = nco.delta_for(f_ref, fs)
    f_act = nco.actual_freq(delta, fs)
    snr_in_db = np.arange(-40, 1, 5)
    taus = [0.03, 0.1]
    rng = np.random.default_rng(3)
    sigma_tot = np.sqrt(sigma_v ** 2 + (signals.VREF / 4096) ** 2 / 12)

    results = {tau: [] for tau in taus}
    for snr in snr_in_db:
        a = sigma_tot * np.sqrt(2) * 10 ** (snr / 20)
        codes = adc_codes(fs, n, [(a, f_act, 0.0)], noise_rms=sigma_v, rng=rng)
        di, _ = mix_and_decimate_q31(codes, delta, m)
        for tau in taus:
            x = demod.iir_q31(di, demod.alpha_q31_for(tau, m / fs), order)
            x = x[int(12 * tau * fs / m):].astype(float)
            results[tau].append(10 * np.log10(x.mean() ** 2 / x.var()))

    fig, ax = plt.subplots(figsize=(6, 4))
    rows, worst = [], 0.0
    for tau, mk in zip(taus, "os"):
        gain = 10 * np.log10((fs / 2) / (ENBW_K[order] / tau))
        ax.plot(snr_in_db, results[tau], mk, label=f"measured, τ={tau*1e3:.0f} ms")
        ax.plot(snr_in_db, snr_in_db + gain, "k-", lw=0.8)
        err = np.array(results[tau]) - (snr_in_db + gain)
        worst = max(worst, np.max(np.abs(err)))
        rows.append(Row("E3", f"Processing gain, τ={tau*1e3:.0f} ms, order {order}",
                        f"{gain:.1f} dB", f"{np.mean(np.array(results[tau]) - snr_in_db):.1f} dB",
                        np.max(np.abs(err)) < 1.0))
    ax.axhline(0, color="grey", lw=0.6)
    ax.set_xlabel("input SNR (dB, full 50 kHz band)")
    ax.set_ylabel("output SNR on X (dB)")
    ax.set_title(f"E3: processing gain (lines = theory), order {order}")
    ax.legend(fontsize=8)
    savefig(fig, "e3_processing_gain")

    # --- illustration: a -40 dB signal, before and after ---------------------
    cfg = pipeline.Config(f_ref=f_ref, tau=1.0, order=2, m=m)
    a = sigma_tot * np.sqrt(2) * 10 ** (-40 / 20)
    n2 = int(15 * fs)
    t = signals.sample_times(fs, n2)
    clean = signals.tones(t, [(a, cfg.f_actual, 0.0)], offset=0.0)
    codes = signals.adc(clean + signals.VREF / 2 + signals.white_noise(n2, sigma_v, rng))
    r = pipeline.run_q31(codes, cfg)
    R, _ = meas.polar_q31(r.x, r.y)
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(6, 5))
    w = slice(0, int(0.01 * fs))
    a1.plot(t[w] * 1e3, (signals.dac(codes[w]) - signals.VREF / 2) * 1e3, lw=0.5,
            label="ADC input (signal + noise)")
    a1.plot(t[w] * 1e3, clean[w] * 1e3, "r", lw=1.5, label=f"hidden signal ({a*1e3:.1f} mV peak)")
    a1.set_xlabel("time (ms)")
    a1.set_ylabel("mV")
    a1.set_title("E3: input at SNR = -40 dB")
    a1.legend(fontsize=8, loc="upper right")
    true_rms = a / np.sqrt(2)
    # predicted 1-sigma noise on X (and roughly on R), in volts rms
    sd_rms = np.sqrt(2) * sigma_tot * np.sqrt(ENBW_K[cfg.order] / cfg.tau / fs)
    r_v = meas.q31_to_volts_rms(R)
    a2.plot(r.t_out, r_v * 1e3, label=f"lock-in R (τ = {cfg.tau:g} s, 12 dB/oct)")
    a2.axhline(true_rms * 1e3, color="r", ls="--", label="true amplitude (rms)")
    a2.axhspan((true_rms - sd_rms) * 1e3, (true_rms + sd_rms) * 1e3, color="r", alpha=0.1,
               label="±1σ predicted output noise")
    a2.set_xlabel("time (s)")
    a2.set_ylabel("mV rms")
    a2.legend(fontsize=8)
    savefig(fig, "e3_before_after")
    final = r_v[r.t_out > 12.0].mean()
    rows.append(Row("E3", "Signal recovered at -40 dB input SNR (τ = 1 s)",
                    f"{true_rms*1e3:.3f} ± {sd_rms*1e3:.3f} mV rms (1σ)",
                    f"{final*1e3:.3f} mV rms (mean of last 3 s)",
                    abs(final - true_rms) < 3 * sd_rms))
    return rows


if __name__ == "__main__":
    for r in run():
        print(r)
