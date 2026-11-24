"""End-to-end checks of the full chain (float vs Q31, and against known inputs)."""
import numpy as np
import pytest

from lockin import meas, pipeline, q31, signals


def _codes(cfg, n, comps, noise=0.0, seed=1):
    t = signals.sample_times(cfg.fs, n)
    v = signals.tones(t, comps)
    if noise:
        v = v + signals.white_noise(n, noise, np.random.default_rng(seed))
    return signals.adc(v)


class TestConfig:
    def test_rejects_bad_order(self):
        with pytest.raises(ValueError):
            pipeline.Config(order=5)

    def test_rejects_harmonic_above_nyquist(self):
        with pytest.raises(ValueError):
            pipeline.Config(f_ref=20_000, harmonic=3)

    def test_alpha_float_equals_quantised(self):
        cfg = pipeline.Config()
        assert cfg.alpha * 2**31 == cfg.alpha_q31


class TestKnownSignal:
    @pytest.mark.parametrize("sync", [False, True])
    def test_amplitude_and_phase(self, sync):
        cfg = pipeline.Config(f_ref=1000.0, tau=0.005, order=2, sync=sync)
        a, phi = 0.5, -2.0
        r = pipeline.run_q31(_codes(cfg, 30_000, [(a, cfg.f_actual, phi)]), cfg)
        R, th = meas.polar_q31(r.x[-20:], r.y[-20:])
        assert meas.q31_to_volts_rms(R).mean() == pytest.approx(a / np.sqrt(2), rel=1e-3)
        assert meas.theta_to_rad(th).mean() == pytest.approx(phi, abs=1e-3)

    def test_second_harmonic(self):
        # The fundamental is only f_ref = 500 Hz away from 2f, so tau must be long
        # enough to reject it: order 4, tau 50 ms -> attenuation ~ 1e-9 at 500 Hz.
        cfg = pipeline.Config(f_ref=500.0, harmonic=2, tau=0.05, order=4)
        r = pipeline.run_q31(_codes(cfg, 80_000, [(0.3, cfg.f_actual, 0.0),
                                                 (0.1, 2 * cfg.f_actual, 0.5)]), cfg)
        R, th = meas.polar_q31(r.x[-20:], r.y[-20:])
        assert meas.q31_to_volts_rms(R).mean() == pytest.approx(0.1 / np.sqrt(2), rel=2e-3)
        assert meas.theta_to_rad(th).mean() == pytest.approx(0.5, abs=3e-3)


class TestQ31VsFloat:
    @pytest.mark.parametrize("sync", [False, True])
    def test_agree_within_one_lsb(self, sync):
        cfg = pipeline.Config(f_ref=1234.0, tau=0.01, order=4, sync=sync)
        codes = _codes(cfg, 40_000, [(0.2, cfg.f_actual, 1.0)], noise=0.3)
        rq = pipeline.run_q31(codes, cfg)
        rf = pipeline.run_float(q31.to_float(q31.adc_to_q31(codes)), cfg)
        for k in ("mix_i", "dec_i", "x", "y"):
            assert np.max(np.abs(rq.stages[k] - rf.stages[k] * 2**31)) <= 1.0, k


class TestTwoChannel:
    def test_ratio_of_scaled_shifted_copy(self):
        cfg = pipeline.Config(f_ref=800.0, tau=0.005)
        g, dphi = 0.5, 0.8
        c1 = _codes(cfg, 30_000, [(0.6, cfg.f_actual, 0.2)])
        c2 = _codes(cfg, 30_000, [(0.6 * g, cfg.f_actual, 0.2 + dphi)])
        r1, r2 = pipeline.run_q31(c1, cfg), pipeline.run_q31(c2, cfg)
        h = meas.ratio(r1.x[-10:], r1.y[-10:], r2.x[-10:], r2.y[-10:]).mean()
        assert abs(h) == pytest.approx(g, rel=1e-3)
        assert np.angle(h) == pytest.approx(dphi, abs=1e-3)
