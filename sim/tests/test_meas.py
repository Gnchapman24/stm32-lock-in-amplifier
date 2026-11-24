import numpy as np
import pytest

from lockin import meas, q31


class TestPolar:
    def test_float(self):
        r, th = meas.polar_float([3.0, 0.0, -1.0], [4.0, -2.0, 0.0])
        np.testing.assert_allclose(r, [5.0, 2.0, 1.0])
        np.testing.assert_allclose(th, [np.arctan2(4, 3), -np.pi / 2, np.pi])

    def test_q31(self):
        r, th = meas.polar_q31([3 << 20, 0, -(1 << 29)], [4 << 20, -(1 << 29), 0])
        np.testing.assert_array_equal(r, [5 << 20, 1 << 29, 1 << 29])
        np.testing.assert_array_equal(th[1:], [-(1 << 30), q31.Q31_MAX])   # -pi/2, +pi

    def test_q31_r_is_floor_sqrt(self):
        r, _ = meas.polar_q31([1], [1])
        assert r[0] == 1                         # floor(sqrt(2))


class TestUnits:
    def test_volts_rms(self):
        """A 1 V-peak input gives R = (1 V / VREF) * 2**31 / 2 -> 1/sqrt(2) V rms."""
        r = 0.5 * 2**31 / 3.3
        assert meas.q31_to_volts_rms(r) == pytest.approx(1 / np.sqrt(2))

    def test_theta(self):
        assert meas.theta_to_rad(1 << 30) == pytest.approx(np.pi / 2)


class TestRatio:
    def test_ratio_and_cal(self):
        h = meas.ratio(1.0, 0.0, 0.0, 2.0)
        assert h == pytest.approx(2j)
        assert meas.apply_through_cal(h, 2j) == pytest.approx(1.0)

    def test_gain_db(self):
        assert meas.gain_db(0.1) == pytest.approx(-20.0)
