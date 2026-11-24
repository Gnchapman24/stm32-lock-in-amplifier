import numpy as np
import pytest

from lockin import demod, nco, q31


@pytest.fixture
def rng():
    return np.random.default_rng(2024)


class TestMixer:
    def test_q31_matches_mul(self, rng):
        x = rng.integers(-(1 << 30), 1 << 30, 1000)
        s = rng.integers(q31.Q31_MIN, q31.Q31_MAX, 1000)
        c = rng.integers(q31.Q31_MIN, q31.Q31_MAX, 1000)
        i, q = demod.mix_q31(x, s, c)
        np.testing.assert_array_equal(i, q31.mul(x, s))
        np.testing.assert_array_equal(q, q31.mul(x, c))

    def test_dc_term_is_half_amplitude(self):
        """A sin(wt + phi) * sin(wt) averages to (A/2) cos(phi)."""
        p = nco.phases(nco.delta_for(1000.0, 100_000.0), 100_000)
        s, c = nco.sincos_float(p)
        a, phi = 0.4, 0.3
        x = a * (s * np.cos(phi) + c * np.sin(phi))   # = a sin(wt + phi)
        i, q = demod.mix_float(x, s, c)
        assert i.mean() == pytest.approx(a / 2 * np.cos(phi), abs=1e-6)
        assert q.mean() == pytest.approx(a / 2 * np.sin(phi), abs=1e-6)


class TestBoxcar:
    def test_float(self):
        np.testing.assert_allclose(demod.boxcar_float([1, 2, 3, 4, 5, 6, 7], 2), [1.5, 3.5, 5.5])

    def test_q31_rounds(self):
        np.testing.assert_array_equal(demod.boxcar_q31([1, 2, 3, 4, -1, -2, 9], 2), [2, 4, -1])

    def test_q31_m1_is_identity(self):
        np.testing.assert_array_equal(demod.boxcar_q31([5, -7], 1), [5, -7])

    @pytest.mark.parametrize("m", [0, 3, 12])
    def test_rejects_non_power_of_two(self, m):
        with pytest.raises(ValueError):
            demod.boxcar_q31([1, 2, 3], m)

    def test_q31_close_to_float(self, rng):
        x = rng.integers(-(1 << 30), 1 << 30, 4096)
        bq = demod.boxcar_q31(x, 64)
        bf = demod.boxcar_float(x.astype(float), 64)
        assert np.max(np.abs(bq - bf)) <= 0.5


class TestAlpha:
    def test_value(self):
        assert demod.alpha_for(1.0, 1.0) == pytest.approx(1 - np.exp(-1))

    def test_small_ratio_matches_t_over_tau(self):
        assert demod.alpha_for(10.0, 1e-5) == pytest.approx(1e-6, rel=1e-6)

    def test_q31_minimum_is_one(self):
        assert demod.alpha_q31_for(1e9, 1e-6) == 1


class TestIirFloat:
    def test_dc_gain_is_one(self):
        y = demod.iir_float(np.ones(20_000), 0.01, 3)
        assert y[-1] == pytest.approx(1.0, abs=1e-9)

    @pytest.mark.parametrize("order,t99", [(1, 4.61), (2, 6.64), (3, 8.41), (4, 10.05)])
    def test_settling_matches_theory(self, order, t99):
        """With small alpha (~ continuous), 99% settling ~ t99 * tau samples."""
        alpha = 1e-3                       # tau ~ 1000 samples
        y = demod.iir_float(np.ones(20_000), alpha, order)
        n99 = np.argmax(y >= 0.99)
        assert n99 * alpha == pytest.approx(t99, rel=0.01)


class TestIirQ31:
    def test_tracks_float(self, rng):
        x = rng.integers(-(1 << 29), 1 << 29, 3000)
        a = 50_000_000
        yq = demod.iir_q31(x, a, 2)
        yf = demod.iir_float(x.astype(float), a / 2**31, 2)
        assert np.max(np.abs(yq - yf)) <= 1.0

    def test_converges_to_constant(self):
        """Wide state: reaches the input exactly (no dead band)."""
        target = 1_234_567
        alpha = 1 << 19                    # 2**-12: tau = 4096 samples
        y = demod.iir_q31(np.full(100_000, target), alpha, 1)
        assert y[-1] == target
        # ... whereas the naive filter stalls up to 2**30/alpha = 2048 LSB short
        yn = demod.iir_q31_naive(np.full(100_000, target), alpha, 1)
        assert 0 < target - yn[-1] <= (1 << 30) // alpha

    def test_naive_stalls_at_predicted_offset(self):
        """Naive 32-bit state stalls once alpha*e < 2**30, i.e. e < 2**30/alpha."""
        alpha = 2048                       # 2**-20
        target = 4 << 19                   # 4 ADC LSB
        y = demod.iir_q31_naive(np.full(3_000_000, target), alpha, 1)
        stall = target - y[-1]
        assert 0 < stall <= (1 << 30) // alpha
        assert stall > 0.9 * ((1 << 30) // alpha)

    def test_block_processing_with_state_equals_one_shot(self, rng):
        x = rng.integers(-(1 << 29), 1 << 29, 1000)
        whole = demod.iir_q31(x, 10_000_000, 3)
        st = [0, 0, 0]
        parts = np.concatenate([demod.iir_q31(x[:333], 10_000_000, 3, st),
                                demod.iir_q31(x[333:], 10_000_000, 3, st)])
        np.testing.assert_array_equal(parts, whole)

    def test_rejects_bad_alpha(self):
        with pytest.raises(ValueError):
            demod.iir_q31([0], 0, 1)


class TestSync:
    def test_period_bounds(self):
        p = nco.phases(1 << 30, 10)       # wraps every 4 samples
        np.testing.assert_array_equal(demod.period_bounds(p), [4, 8])

    def test_float_removes_2f_exactly_for_integer_period(self):
        p = nco.phases(1 << 28, 1000)     # 16 samples per period
        s, c = nco.sincos_float(p)
        out = demod.sync_float(s * s, p)  # sin^2 = 1/2 - cos(2wt)/2
        np.testing.assert_allclose(out, 0.5, atol=1e-12)

    def test_q31_matches_rounded_float(self, rng):
        p = nco.phases(nco.delta_for(1030.0, 100_000.0), 20_000)
        x = rng.integers(-(1 << 30), 1 << 30, 20_000)
        sq = demod.sync_q31(x, p)
        sf = demod.sync_float(x.astype(float), p)
        assert np.max(np.abs(sq - sf)) <= 0.5
        assert len(sq) == len(demod.period_bounds(p)) - 1
