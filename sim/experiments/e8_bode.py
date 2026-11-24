"""E8 -- Simulated Bode analyzer with mismatched channels and through calibration.

Excitation: NCO -> 12-bit DAC sine (1 V peak about mid-rail).
DUT: RC low-pass, R = 1.6 kOhm, C = 100 nF (f_c ~ 995 Hz).
Channel models (deliberately mismatched, as real AFEs will be):
    CH1: AC coupling 0.5 Hz, gain 1.00
    CH2: AC coupling 0.7 Hz, gain 1.02, extra 30 kHz pole (INA bandwidth)
Each channel adds 1 mV rms noise, then the 12-bit ADC.

Calibration run: both channels on the DUT input -> H_cal(f).
Measurement run: CH1 on DUT input, CH2 on DUT output -> H_raw(f).
Reported: H_raw / H_cal, compared with the DUT's exact response. The analog
networks are simulated with the bilinear transform, which warps frequency
near Nyquist (~3% at 10 kHz), so the reference is the exact response of the
*simulated* network; the ideal analog curve is shown dotted.
"""
import numpy as np
from scipy import signal as sps

from lockin import meas, nco, pipeline, signals
from experiments.common import Row, plt, savefig

R_DUT, C_DUT = 1600.0, 100e-9
CH = {1: dict(fc_ac=0.5, gain=1.00, pole=None), 2: dict(fc_ac=0.7, gain=1.02, pole=30e3)}


def channel(v, fs, ch, rng):
    p = CH[ch]
    y = p["gain"] * signals.ac_coupling(v, fs, p["fc_ac"])
    if p["pole"]:
        y = signals.rc_lowpass(y, fs, 1.0, 1 / (2 * np.pi * p["pole"]))
    return signals.adc(y + signals.VREF / 2 + signals.white_noise(len(v), 1e-3, rng))


def measure(cfg, v1, v2, rng, t_avg=0.5):
    r1 = pipeline.run_q31(channel(v1, cfg.fs, 1, rng), cfg)
    r2 = pipeline.run_q31(channel(v2, cfg.fs, 2, rng), cfg)
    k = r1.t_out > r1.t_out[-1] - t_avg
    return meas.ratio(r1.x[k].mean(), r1.y[k].mean(), r2.x[k].mean(), r2.y[k].mean())


def run():
    rng = np.random.default_rng(8)
    freqs = np.logspace(1, 4, 16)
    fs, dur = 100_000.0, 3.0
    h_raw, h_cal, h_ref = [], [], []
    b, a = sps.bilinear([1.0], [R_DUT * C_DUT, 1.0], fs)
    for f in freqs:
        cfg = pipeline.Config(f_ref=f, tau=0.2, order=4, m=16)
        n = int(dur * fs)
        ph = nco.phases(cfg.delta, n)
        s, _ = nco.sincos_float(ph)
        exc = signals.dac(2048 + np.round(s * 1.0 / signals.VREF * 4096).astype(np.int64))
        dut_out = signals.rc_lowpass(exc, fs, R_DUT, C_DUT)
        h_cal.append(measure(cfg, exc, exc, rng))
        h_raw.append(measure(cfg, exc, dut_out, rng))
        _, hz = sps.freqz(b, a, worN=[cfg.f_actual], fs=fs)
        h_ref.append(hz[0])
    h_raw, h_cal, h_ref = map(np.array, (h_raw, h_cal, h_ref))
    h_cor = meas.apply_through_cal(h_raw, h_cal)
    h_analog = 1 / (1 + 1j * freqs * 2 * np.pi * R_DUT * C_DUT)

    def g_err(h):
        return meas.gain_db(h) - meas.gain_db(h_ref)

    def p_err(h):
        return np.degrees(np.angle(h / h_ref))

    fig, ax = plt.subplots(3, 1, figsize=(6.5, 7.5), sharex=True)
    ax[0].semilogx(freqs, meas.gain_db(h_analog), "k:", lw=0.8, label="ideal analog RC")
    ax[0].semilogx(freqs, meas.gain_db(h_ref), "k-", lw=0.8, label="simulated network (reference)")
    ax[0].semilogx(freqs, meas.gain_db(h_raw), "x", label="measured, uncalibrated")
    ax[0].semilogx(freqs, meas.gain_db(h_cor), "o", mfc="none", label="measured, through-calibrated")
    ax[0].set_ylabel("|H| (dB)")
    ax[0].legend(fontsize=7)
    ax[0].set_title("E8: simulated Bode analyzer, RC low-pass (f_c ≈ 995 Hz)")
    ax[1].semilogx(freqs, np.degrees(np.angle(h_analog)), "k:", lw=0.8)
    ax[1].semilogx(freqs, np.degrees(np.angle(h_ref)), "k-", lw=0.8)
    ax[1].semilogx(freqs, np.degrees(np.angle(h_raw)), "x")
    ax[1].semilogx(freqs, np.degrees(np.angle(h_cor)), "o", mfc="none")
    ax[1].set_ylabel("phase (deg)")
    ax[2].semilogx(freqs, p_err(h_raw), "x-", label="phase error, uncalibrated (deg)")
    ax[2].semilogx(freqs, p_err(h_cor), "o-", mfc="none", label="phase error, calibrated (deg)")
    ax[2].semilogx(freqs, g_err(h_raw), "x--", label="gain error, uncalibrated (dB)")
    ax[2].semilogx(freqs, g_err(h_cor), "o--", mfc="none", label="gain error, calibrated (dB)")
    ax[2].set_ylabel("error")
    ax[2].set_xlabel("frequency (Hz)")
    ax[2].legend(fontsize=7)
    savefig(fig, "e8_bode")

    return [
        Row("E8", "Bode gain error, uncalibrated → calibrated (max)", "→ ≈ 0",
            f"{np.max(np.abs(g_err(h_raw))):.3f} dB → {np.max(np.abs(g_err(h_cor))):.4f} dB",
            np.max(np.abs(g_err(h_cor))) < 0.01),
        Row("E8", "Bode phase error, uncalibrated → calibrated (max)", "→ ≈ 0",
            f"{np.max(np.abs(p_err(h_raw))):.2f}° → {np.max(np.abs(p_err(h_cor))):.3f}°",
            np.max(np.abs(p_err(h_cor))) < 0.1),
        Row("E8", "Bilinear warping of the simulated network at 10 kHz", "simulation artefact",
            f"{abs(meas.gain_db(h_ref[-1]) - meas.gain_db(h_analog[-1])):.2f} dB vs ideal analog", None),
    ]


if __name__ == "__main__":
    for r in run():
        print(r)
