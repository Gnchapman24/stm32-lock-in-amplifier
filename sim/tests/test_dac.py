import numpy as np
import pytest

from lockin import dac, nco, q31


def test_full_scale_and_zero():
    """Q31_MIN = -1.0 -> 2048 - 1600; Q31_MAX = 1 - 2^-31 rounds to +1600."""
    got = dac.code_from_sin([q31.Q31_MIN, 0, q31.Q31_MAX])
    np.testing.assert_array_equal(got, [448, 2048, 3648])


def test_round_half_up_not_away_from_zero():
    """+-2^24 * 1600 / 2^31 = +-12.5 exactly. Half up gives +13 and -12;
    round-half-away-from-zero would give -13. This pins the rounding rule."""
    got = dac.code_from_sin([1 << 24, -(1 << 24)])
    np.testing.assert_array_equal(got, [2048 + 13, 2048 - 12])


def test_within_half_code_of_ideal():
    s = np.linspace(q31.Q31_MIN, q31.Q31_MAX, 100_001).astype(np.int64)
    ideal = dac.DAC_MID + s * dac.DAC_AMP / 2**31
    err = dac.code_from_sin(s) - ideal
    assert err.min() >= -0.5 and err.max() < 0.5


def test_monotonic():
    s = np.sort(np.random.default_rng(1).integers(q31.Q31_MIN, q31.Q31_MAX, 10_000))
    assert np.all(np.diff(dac.code_from_sin(s)) >= 0)


def test_stays_in_window_for_a_real_sine():
    s, _c = nco.sincos_q31(nco.phases(nco.delta_for(1234.5, 100_000.0), 10_000))
    codes = dac.code_from_sin(s)
    assert codes.min() >= dac.DAC_CODE_MIN and codes.max() <= dac.DAC_CODE_MAX


@pytest.mark.parametrize("amp", [-1, 1801, 4000])
def test_rejects_amp_outside_window(amp):
    with pytest.raises(ValueError):
        dac.code_from_sin([0], amp=amp)


def test_exact_cycle_buffer():
    """B4 (D25 note): delta = k * 2^25 returns to phase0 after 128 samples."""
    for k in (1, 2, 5, 63):
        p = nco.phases(k << 25, 129, phase0=0x12345678)
        assert p[128] == p[0]
