# Experiments

Each script checks one prediction from theory and saves a plot to
`../results/phase0/`. Run them all (and regenerate `SUMMARY.md`) from `sim/`:

```powershell
python -m experiments.run_all
```

or one at a time, e.g. `python -m experiments.e5_dead_band`.

| Script | Checks |
|---|---|
| `e1_enbw` | Output noise ∝ √ENBW for τ = 10–100 ms, orders 1–4 |
| `e2_settling` | 99% settling time = 4.61 / 6.64 / 8.41 / 10.05 τ |
| `e3_processing_gain` | SNR_out = SNR_in + 10·log₁₀((f_s/2)/ENBW); before/after figure at −40 dB |
| `e4_sync_filter` | 2f ripple at 10.3 Hz vs τ, and the sync filter's improvement |
| `e5_dead_band` | Naive 32-bit IIR stalls at 2³⁰/α; 64-bit state doesn't |
| `e6_q31_error` | Q31 vs float error per stage ≈ 1/√12 LSB |
| `e7_clipping` | Interferer rejection until ADC clipping; dynamic-reserve limit |
| `e8_bode` | Two-channel Bode measurement with mismatched channels, through calibration |
| `export_vectors` | Writes `../vectors/` for the firmware's C unit tests |
