"""Tests for the reference model: imports, signals.py and q31.py.

(nco, demod, meas, pipeline and vectors have their own test files.)

Structure: one class per function under test.

Run:   pytest -v              (all)
       pytest -v -k Adc       (just the TestAdc class)

Useful patterns:
    with pytest.raises(ValueError):  ...           # expect an exception
    assert x == pytest.approx(y, rel=1e-12)       # float compare
    np.testing.assert_array_equal(a, b)           # exact array compare
    np.testing.assert_allclose(a, b, atol=...)    # tolerant array compare
"""
import importlib

import numpy as np
import pytest

from lockin import signals, q31

MODULES = ["q31", "signals", "nco", "demod", "meas", "pipeline", "vectors"]


# ---------------------------------------------------------------- fixtures --

@pytest.fixture
def rng():
    """Seeded generator so noise-based tests are reproducible."""
    return np.random.default_rng(12345)


# ------------------------------------------------------------------- smoke --

@pytest.mark.parametrize("name", MODULES)
def test_imports(name):
    importlib.import_module(f"lockin.{name}")


# ================================================================ signals ===

class TestSampleTimes:
    def test_known_values(self):
        """fs=1000, n=5 -> [0, 0.001, ..., 0.004]."""
        # Arrange: inputs and the answer, worked out by hand (not by the code
        # under test -- otherwise the test just checks the code against itself).
        fs = 1000.0
        n = 5
        expected = np.array([0.0, 0.001, 0.002, 0.003, 0.004])

        # Act: one call to the function under test.
        t = signals.sample_times(fs, n)

        # Assert: floats are compared with a tolerance, never ==.
        # k/1000 is not exactly representable in binary, so allow a few ULP.
        np.testing.assert_allclose(t, expected, rtol=0, atol=1e-15)

    def test_spacing_is_one_over_fs(self):
        """np.diff(t) all ~= 1/fs."""
        fs = 1000.0
        n = 5

        t = signals.sample_times(fs, n)

        np.testing.assert_allclose(np.diff(t), 1/fs, rtol=0, atol=1e-15)

    @pytest.mark.parametrize("n", [1, 2, 999, 100_000])
    def test_length_and_dtype(self, n):
        """len == n, dtype float64."""
        fs = 1000.0

        t = signals.sample_times(fs, n)

        assert len(t) == n
        assert t.dtype == np.float64

    def test_accepts_numpy_int(self):
        assert len(signals.sample_times(1000.0, np.int64(7))) == 7

    @pytest.mark.parametrize("bad_n", [0, -1])
    def test_rejects_bad_n_value(self, bad_n):
        """ValueError."""
        with pytest.raises(ValueError):
            signals.sample_times(1000.0, bad_n)

    @pytest.mark.parametrize("bad_n", [2.5, "10", True])
    def test_rejects_bad_n_type(self, bad_n):
        """TypeError."""
        with pytest.raises(TypeError):
            signals.sample_times(1000.0, bad_n)

    @pytest.mark.parametrize("bad_fs", [0, -1000])
    def test_rejects_bad_fs(self, bad_fs):
        """ValueError."""
        with pytest.raises(ValueError):
            signals.sample_times(bad_fs, 10)


class TestTones:
    def test_no_components_gives_offset(self):
        """Empty components -> constant VREF/2."""
        v = signals.tones(signals.sample_times(1000.0, 10), [])
        np.testing.assert_array_equal(v, np.full(10, signals.VREF / 2))

    def test_single_tone_amplitude_and_mean(self):
        """Over whole periods: max-min ~= 2A, mean ~= offset."""
        fs, f, a = 100_000.0, 1000.0, 0.4
        t = signals.sample_times(fs, 1000)            # exactly 10 periods
        v = signals.tones(t, [(a, f, 0.0)], offset=1.0)
        assert v.max() - v.min() == pytest.approx(2 * a, rel=1e-3)
        assert v.mean() == pytest.approx(1.0, abs=1e-12)

    def test_phase(self):
        """phase=pi/2 -> first sample == offset + A."""
        v = signals.tones(np.array([0.0]), [(0.3, 50.0, np.pi / 2)], offset=1.0)
        assert v[0] == pytest.approx(1.3, abs=1e-15)

    def test_superposition(self):
        """tones(a+b) == tones(a) + tones(b) - offset."""
        t = signals.sample_times(10_000.0, 500)
        a, b = (0.2, 100.0, 0.1), (0.05, 730.0, -1.0)
        both = signals.tones(t, [a, b])
        np.testing.assert_allclose(
            both, signals.tones(t, [a]) + signals.tones(t, [b]) - signals.VREF / 2,
            rtol=0, atol=1e-14)


class TestWhiteNoise:
    def test_rms(self, rng):
        """Large n: measured RMS ~= requested.

        The standard error of an RMS estimate from n Gaussian samples is about
        rms / sqrt(2n); 5 standard errors makes a false failure ~1 in 10^6.
        """
        n, rms = 200_000, 0.25
        v = signals.white_noise(n, rms, rng)
        assert np.sqrt(np.mean(v**2)) == pytest.approx(rms, abs=5 * rms / np.sqrt(2 * n))

    def test_zero_mean(self, rng):
        """|mean| small compared to rms / sqrt(n) (5 standard errors)."""
        n, rms = 200_000, 0.25
        v = signals.white_noise(n, rms, rng)
        assert abs(v.mean()) < 5 * rms / np.sqrt(n)

    def test_reproducible(self):
        """Same seed -> identical output."""
        a = signals.white_noise(100, 1.0, np.random.default_rng(7))
        b = signals.white_noise(100, 1.0, np.random.default_rng(7))
        np.testing.assert_array_equal(a, b)

    def test_rejects_negative_rms(self, rng):
        with pytest.raises(ValueError):
            signals.white_noise(10, -1.0, rng)


class TestAdc:
    LSB = signals.VREF / 4096

    def test_endpoints(self):
        """0 V -> 0, VREF/2 -> 2048, VREF - 1 LSB -> 4095."""
        codes = signals.adc([0.0, signals.VREF / 2, signals.VREF - self.LSB])
        np.testing.assert_array_equal(codes, [0, 2048, 4095])

    def test_rounds_to_nearest(self):
        codes = signals.adc([0.49 * self.LSB, 0.51 * self.LSB, 1.49 * self.LSB])
        np.testing.assert_array_equal(codes, [0, 1, 1])

    def test_clips_low_and_high(self):
        """-1 V -> 0, +5 V -> 4095; clip count reports it."""
        codes, n_clipped = signals.adc([-1.0, 1.0, 5.0], return_clips=True)
        np.testing.assert_array_equal(codes, [0, signals.adc(1.0), 4095])
        assert n_clipped == 2

    def test_output_is_integer(self, rng):
        """Integer dtype, all values in [0, 4095]."""
        codes = signals.adc(rng.uniform(-1, 5, 1000))
        assert np.issubdtype(codes.dtype, np.integer)
        assert codes.min() >= 0 and codes.max() <= 4095

    def test_quantisation_noise(self):
        """Slow ramp: RMS(adc->volts - input) ~= LSB / sqrt(12)."""
        v = np.linspace(0.1, 3.0, 200_001)
        err = signals.dac(signals.adc(v)) - v
        assert np.sqrt(np.mean(err**2)) == pytest.approx(self.LSB / np.sqrt(12), rel=0.01)


class TestDac:
    def test_round_trip(self):
        """adc(dac(code)) == code for all 4096 codes."""
        codes = np.arange(4096)
        np.testing.assert_array_equal(signals.adc(signals.dac(codes)), codes)

    def test_rejects_out_of_range(self):
        with pytest.raises(ValueError):
            signals.dac([4096])


class TestRcLowpass:
    FS = 100_000.0

    def _steady_sine_response(self, f, out_fn):
        t = signals.sample_times(self.FS, 200_000)
        v = signals.tones(t, [(1.0, f, 0.0)], offset=0.0)
        y = out_fn(v)
        tail = slice(100_000, None)                    # discard the transient
        z = 2 * np.mean(y[tail] * np.exp(-2j * np.pi * f * t[tail]))  # complex amplitude
        z_in = 2 * np.mean(v[tail] * np.exp(-2j * np.pi * f * t[tail]))
        return z / z_in

    def test_gain_and_phase_at_corner(self):
        """Sine at f_c: steady-state gain ~= 1/sqrt(2), phase ~= -pi/4."""
        r, c = 1600.0, 100e-9
        fc = 1 / (2 * np.pi * r * c)
        h = self._steady_sine_response(fc, lambda v: signals.rc_lowpass(v, self.FS, r, c))
        assert abs(h) == pytest.approx(1 / np.sqrt(2), rel=2e-3)
        assert np.angle(h) == pytest.approx(-np.pi / 4, abs=2e-3)

    def test_dc_passes(self):
        """Constant input -> output equals it (filter starts in steady state)."""
        y = signals.rc_lowpass(np.full(1000, 1.65), self.FS, 1e3, 1e-6)
        np.testing.assert_allclose(y, 1.65, rtol=1e-12)


class TestAcCoupling:
    FS = 100_000.0

    def test_removes_dc(self):
        """Offset input settles to zero."""
        y = signals.ac_coupling(np.full(1000, 1.65), self.FS, 1.0)
        # Residual is float round-off (~1e-12 on some platforms); 1e-9 V is still far below signal scale.
        np.testing.assert_allclose(y, 0.0, atol=1e-9)

    def test_passes_high_frequency(self):
        """f >> f_corner: gain ~= 1, phase ~= 0 (phase = atan(fc/f) = 1 mrad)."""
        h = TestRcLowpass._steady_sine_response(
            TestRcLowpass(), 1000.0, lambda v: signals.ac_coupling(v, self.FS, 1.0))
        assert abs(h) == pytest.approx(1.0, rel=1e-3)
        assert np.angle(h) == pytest.approx(np.arctan(1.0 / 1000.0), abs=2e-4)


# ==================================================================== q31 ===

class TestFromToFloat:
    def test_known_values(self):
        """0.0 -> 0, 0.5 -> 2**30, -1.0 -> -2**31."""
        np.testing.assert_array_equal(q31.from_float([0.0, 0.5, -1.0]), [0, 1 << 30, -(1 << 31)])

    def test_plus_one(self):
        """+1.0 saturates to Q31_MAX (documented in Q31-RULES)."""
        assert q31.from_float(1.0) == q31.Q31_MAX
        assert q31.from_float(5.0) == q31.Q31_MAX
        assert q31.from_float(-5.0) == q31.Q31_MIN

    def test_round_trip(self, rng):
        """to_float(from_float(x)) within 0.5 LSB (2**-32) for random x in [-1, 1)."""
        x = rng.uniform(-1, 1, 10_000)
        np.testing.assert_allclose(q31.to_float(q31.from_float(x)), x, rtol=0, atol=2.0**-32)


class TestSat:
    def test_in_range_unchanged(self):
        v = [q31.Q31_MIN, -1, 0, 1, q31.Q31_MAX]
        np.testing.assert_array_equal(q31.sat(v), v)

    def test_clamps(self):
        """2**31 -> Q31_MAX, -2**31 - 1 -> Q31_MIN."""
        np.testing.assert_array_equal(q31.sat([1 << 31, -(1 << 31) - 1, 1 << 40]),
                                      [q31.Q31_MAX, q31.Q31_MIN, q31.Q31_MAX])


class TestWrap:
    def test_wraps_like_int32(self, rng):
        """Compare with NumPy's int64 -> int32 cast (two's-complement truncation)."""
        v = rng.integers(-(1 << 40), 1 << 40, 1000)
        np.testing.assert_array_equal(q31.wrap(v), v.astype(np.int32))
        assert q31.wrap(q31.Q31_MAX + 1) == q31.Q31_MIN


class TestAddSubSat:
    def test_normal(self):
        assert q31.add_sat(100, -30) == 70
        assert q31.sub_sat(100, -30) == 130

    def test_saturates(self):
        """Q31_MAX + 1 -> Q31_MAX; Q31_MIN - 1 -> Q31_MIN."""
        assert q31.add_sat(q31.Q31_MAX, 1) == q31.Q31_MAX
        assert q31.sub_sat(q31.Q31_MIN, 1) == q31.Q31_MIN


class TestMul:
    def test_known_values(self):
        """0.5 * 0.5 == 0.25 exactly; 0.5 * -0.5 == -0.25."""
        h = 1 << 30
        assert q31.mul(h, h) == 1 << 29
        assert q31.mul(h, -h) == -(1 << 29)

    def test_matches_exact_integer_math(self, rng):
        """Random pairs vs. Python-int arithmetic with round-half-up."""
        a = rng.integers(q31.Q31_MIN, q31.Q31_MAX, 5000)
        b = rng.integers(q31.Q31_MIN, q31.Q31_MAX, 5000)
        expected = [(int(x) * int(y) + (1 << 30)) >> 31 for x, y in zip(a, b)]
        np.testing.assert_array_equal(q31.mul(a, b), expected)

    def test_minus_one_squared(self):
        """mul(Q31_MIN, Q31_MIN): the one overflow case -> saturates to Q31_MAX."""
        assert q31.mul(q31.Q31_MIN, q31.Q31_MIN) == q31.Q31_MAX

    def test_rounding_of_negative_results(self):
        """-1.5 LSB rounds half up to -1 (truncation/floor would give -2)."""
        # a*b = -3 * 2**30  ->  exact result -1.5 LSB
        assert q31.mul(-3, 1 << 30) == -1
        assert q31.mul(3, 1 << 30) == 2      # +1.5 -> 2

    def test_rounding_error_is_unbiased(self, rng):
        a = rng.integers(-(1 << 30), 1 << 30, 20_000)
        b = rng.integers(-(1 << 30), 1 << 30, 20_000)
        err = q31.mul(a, b) - (a.astype(float) * b.astype(float)) / 2**31
        assert abs(err.mean()) < 0.02            # truncation would give ~ -0.5


class TestShrRound:
    def test_values(self):
        np.testing.assert_array_equal(q31.shr_round([5, 6, -5, -6, 7], 1), [3, 3, -2, -3, 4])
        np.testing.assert_array_equal(q31.shr_round([5, -5], 0), [5, -5])


class TestDivRound:
    @pytest.mark.parametrize("num,den,exp", [(7, 2, 4), (-7, 2, -4), (6, 4, 2), (-6, 4, -2),
                                             (5, 3, 2), (-5, 3, -2), (0, 9, 0)])
    def test_values(self, num, den, exp):
        assert q31.div_round(num, den) == exp

    def test_rejects_zero(self):
        with pytest.raises(ValueError):
            q31.div_round(1, 0)


class TestAdcToQ31:
    def test_mid_scale_is_zero(self):
        assert q31.adc_to_q31(2048) == 0

    def test_range(self):
        """0 -> -2**30 (-0.5), 4095 -> just under +0.5."""
        np.testing.assert_array_equal(q31.adc_to_q31([0, 4095]), [-(1 << 30), (1 << 30) - (1 << 19)])

    def test_16_bit(self):
        np.testing.assert_array_equal(q31.adc_to_q31([0, 32768], bits=16), [-(1 << 30), 0])


class TestCheckRange:
    def test_passes_in_range(self):
        q31.check_range([q31.Q31_MIN, 0, q31.Q31_MAX])

    def test_raises_out_of_range(self):
        """Raises, and the message includes `name`."""
        with pytest.raises(OverflowError, match="mixer"):
            q31.check_range([0, 1 << 31], name="mixer")
