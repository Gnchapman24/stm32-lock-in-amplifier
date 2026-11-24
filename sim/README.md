# sim — reference model

Float and bit-exact Q31 models of the lock-in DSP chain, experiments that check
them against theory, and test-vector export for the firmware's unit tests.

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
pytest                                   # unit + end-to-end tests
python -m experiments.run_all            # E1–E8 -> ../results/phase0/SUMMARY.md + plots
python -m experiments.export_vectors     # -> vectors/<case>/ for the C tests
```

Run these from this `sim/` folder (or use `pytest sim` from the repo root).

| Module | Contents |
|---|---|
| `lockin/q31.py` | Q31 primitives that mirror ARM instructions (rounded multiply, saturation, shifts) |
| `lockin/signals.py` | Test signals; 12-bit ADC/DAC models with clipping; RC and AC-coupling networks |
| `lockin/nco.py` | 32-bit phase accumulator, harmonics, CORDIC angle mapping, sin/cos |
| `lockin/demod.py` | Mixer, boxcar decimator, IIR cascade (naive and 64-bit state), sync filter |
| `lockin/meas.py` | R/θ, volts-RMS and radian scaling, two-channel ratio, through calibration |
| `lockin/pipeline.py` | `Config` and the complete chain: `run_q31` (firmware model), `run_float` (ideal) |
| `lockin/vectors.py` | Little-endian int32 test-vector export |

Fixed-point conventions: [Q31-RULES.md](Q31-RULES.md).
