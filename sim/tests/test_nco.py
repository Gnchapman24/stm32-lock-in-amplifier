import numpy as np
import pytest

from lockin import nco, q31


class TestDelta:
    def test_known_value(self):
        """f = fs/4 is exactly a quarter cycle per sample."""
        assert nco.delta_for(25_000.0, 100_000.0) == 1 << 30

    def test_rounding_and_actual_freq(self):
        d = nco.delta_for(1000.0, 100_000.0)
        assert d == round(0.01 * 2**32)
        assert abs(nco.actual_freq(d, 100_000.0) - 1000.0) <= 100_000.0 / 2**33

    @pytest.mark.parametrize("f", [0.0, -1.0, 50_000.0, 60_000.0])
    def test_rejects_out_of_band(self, f):
        with pytest.raises(ValueError):
            nco.delta_for(f, 100_000.0)


class TestPhases:
    def test_wraps(self):
        p = nco.phases(1 << 30, 6)
        np.testing.assert_array_equal(p, [0, 1 << 30, 1 << 31, 3 << 30, 0, 1 << 30])
        assert p.dtype == np.uint32

    def test_matches_accumulator_loop(self):
        """Vectorised version == the C-style running accumulator."""
        d, p0, n = 123_456_789, 4_000_000_000, 1000
        acc, expected = p0, []
        for _ in range(n):
            expected.append(acc)
            acc = (acc + d) & 0xFFFFFFFF
        np.testing.assert_array_equal(nco.phases(d, n, p0), expected)


class TestHarmonic:
    def test_values(self):
        np.testing.assert_array_equal(nco.harmonic([0, 1 << 30, 3 << 30], 2), [0, 1 << 31, 1 << 31])

    def test_frequency(self):
        """2nd harmonic of a phase ramp has twice the increment."""
        p = nco.phases(1000, 10)
        np.testing.assert_array_equal(np.diff(nco.harmonic(p, 2).astype(np.int64)), 2000)


class TestCordicAngle:
    def test_reinterpretation(self):
        """0 -> 0, half cycle -> -1.0 (= -pi), quarter -> +0.5 (= +pi/2)."""
        a = nco.to_cordic_angle(np.array([0, 1 << 31, 1 << 30, 3 << 30], dtype=np.uint32))
        np.testing.assert_array_equal(a, [0, -(1 << 31), 1 << 30, -(1 << 30)])


class TestSinCos:
    def test_quarter_points(self):
        s, c = nco.sincos_q31(np.array([0, 1 << 30, 1 << 31, 3 << 30], dtype=np.uint32))
        np.testing.assert_array_equal(s, [0, q31.Q31_MAX, 0, q31.Q31_MIN])
        np.testing.assert_array_equal(c, [q31.Q31_MAX, 0, q31.Q31_MIN, 0])

    def test_within_half_lsb_of_ideal(self):
        p = nco.phases(nco.delta_for(1234.5, 100_000.0), 50_000)
        s, c = nco.sincos_q31(p)
        sf, cf = nco.sincos_float(p)
        # ideal, clipped to the representable range (+1.0 saturates to Q31_MAX)
        ideal_s = np.clip(sf * 2**31, q31.Q31_MIN, q31.Q31_MAX)
        ideal_c = np.clip(cf * 2**31, q31.Q31_MIN, q31.Q31_MAX)
        assert np.max(np.abs(s - ideal_s)) <= 0.5 + 1e-6
        assert np.max(np.abs(c - ideal_c)) <= 0.5 + 1e-6
