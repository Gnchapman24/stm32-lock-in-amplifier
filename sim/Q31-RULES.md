# Q31 Design Rules

The contract between the Python model (`lockin/`) and the firmware. The C code
must reproduce every rule here exactly; `sim/vectors/` contains the expected
output of every stage for checking.

Notation: a *Q31* value is an `int32_t` read as `value / 2^31`, range [−1, 1).
"LSB" means one Q31 unit (2⁻³¹) unless it says "ADC LSB" (= 2¹⁹ Q31 LSB).

## 1. Multiply rounding

`mul(a, b) = SSAT32( ((int64_t)a * b + (1 << 30)) >> 31 )` — **round half up**.

- `>>` on a negative `int64_t` is an arithmetic shift (floor) on GCC/ARM, which is
  what makes "+ half, then shift" round half up. The Python model uses NumPy's
  `>>`, which behaves identically.
- Why round instead of truncate: truncation biases every product by −0.5 LSB.
  After the mixer that is a DC offset on X and Y, the one thing a lock-in must
  not have. (At 2⁻³² it is tiny, but it costs nothing to avoid.) Verified
  unbiased in `test_smoke.py::TestMul::test_rounding_error_is_unbiased`.
- The only overflow is `(−1)·(−1)`, which saturates to `Q31_MAX`.

## 2. Q format and range at each stage boundary

| Stage | Format | Range | Headroom argument |
|---|---|---|---|
| ADC → Q31 | Q31 in int32 | [−0.5, 0.5 − 2⁻¹²] | `(code − 2048) << 19`; 12-bit code can't exceed ±2¹¹ |
| NCO sin/cos | Q31 | [−1, 1), +1 → Q31_MAX | ideal value rounded, saturated |
| Mixer output | Q31 | [−0.5, 0.5] | \|x\| ≤ 0.5, \|ref\| < 1 |
| Decimator output | Q31 | [−0.5, 0.5] | an average can't exceed its inputs; int64 sum of ≤ 2¹⁶ terms < 2⁴⁷ |
| IIR state | int64, Q62 | \|s\| ≤ 2⁶¹ | stage output is a convex combination of its inputs → \|y\| ≤ 0.5 |
| IIR output (X, Y) | Q31 | [−0.5, 0.5] | same |
| R (CORDIC modulus) | Q31 | [0, 0.71] | √(0.5² + 0.5²) |
| θ (CORDIC phase) | Q31 = angle/π | [−1, 1), +π → Q31_MAX | — |

The model asserts these ranges at every stage (`q31.check_range` in `pipeline.run_q31`).

## 3. Saturation

- **Used:** `mul` (for the (−1)·(−1) case), `from_float`, NCO sin/cos (+1.0),
  CORDIC phase (+π).
- **Proven unnecessary:** mixer, decimator, IIR (section 2). The firmware may still
  saturate there for robustness, as it can never change a result.

## 4. Decimation factor M

- Power of two, 1 ≤ M ≤ 2¹⁶. Normalisation is `(sum + (1 << (k−1))) >> k`, k = log₂M:
  exact, with no division.
- M also sets the IIR rate and therefore α (section 5). Rule of thumb: pick M so the
  IIR rate is ≥ ~10/τ. Larger M also shrinks the IIR dead band (E5).

## 5. α

- `α = 1 − exp(−T/τ)`, computed **on the host in double precision** with `expm1`,
  then `α_q31 = round(α · 2³¹)`, minimum 1. T is the IIR sample period: M/f_s
  (boxcar), or 1/f_ref (sync filter).
- The firmware receives `α_q31`, never τ.
- The float model uses exactly `α_q31 / 2³¹`, so float and Q31 share coefficients.

## 6. IIR state format and overflow argument

Per stage, `int64_t s` holds the output in Q62:

```c
int32_t y = (int32_t)((s + (1LL << 30)) >> 31);   // rounded Q31 view
int32_t e = x - y;                                 // |x|,|y| <= 0.5 -> no overflow
s += (int64_t)alpha * e;                           // SMLAL
x = (int32_t)((s + (1LL << 30)) >> 31);            // output -> next stage
```

The increment `α·e` is accumulated at full 62-bit precision and never shifted
down, so there is no dead band. The output converges to within 0.5 LSB of a constant
input. With a naive 32-bit state, `y += mul(α, x − y)` stalls up to `2³⁰/α` LSB short
of the input (E5: exactly 1 ADC LSB for α = 2⁻²⁰).

## 7. Sync-filter normalisation

- A period starts where the NCO phase wraps (`phase[n] < phase[n−1]`). Only complete
  periods are output; a partial first period is discarded.
- Average = `div_round(sum, count)`, int64 sum, rounded to nearest with halves away
  from zero, written so C's truncating `/` gives the same answer:
  `sum >= 0 ? (sum + count/2) / count : -((-sum + count/2) / count)`.
- One 64-bit division per reference period (not per sample).
- The IIR after the sync filter runs once per period with T = 1/f_actual.

## 8. CORDIC handling (option B)

- ST does not document the CORDIC's exact internal rounding, so it cannot be
  emulated bit-exactly. The model uses **ideal values rounded to nearest**:
  sin/cos = `round(sin(2π·phase/2³²)·2³¹)` (saturating), R = `floor(√(x²+y²))`
  (exact integer sqrt), θ = `round(atan2(y,x)/π · 2³¹)`.
- **Host-side C tests** read `sin.bin`/`cos.bin` from the vectors and feed them to
  the mixer, so everything *after* the NCO is checked bit-exactly. The NCO phase
  (`phase.bin`) is integer and must match exactly.
- **On hardware**, the CORDIC output is compared with the ideal value with a
  tolerance of a few LSB. Measure and report the actual deviation (a Phase 2 result).

## Test vectors

`python -m experiments.export_vectors` → `sim/vectors/<case>/`: `config.json` plus one
little-endian int32 `.bin` per stage (`adc, phase, sin, cos, x_in, mix_i, mix_q,
dec_i, dec_q, x, y`). `phase` is the uint32 bit pattern. The output is
deterministic: any diff in git means the model changed.
