# Design Decisions

Why the instrument is built the way it is. Each entry records the decision, the
alternatives that were considered, and what the choice costs. When a decision
changes, add a new entry that supersedes the old one, and leave the old one in place.

**Status:** *Implemented* = done and verified (where it was checked is given);
*Planned* = decided, not built yet.

| # | Decision | Status |
|---|---|---|
| [D1](#d1-stm32g474-on-a-nucleo-board) | STM32G474 on a NUCLEO-G474RE | Planned (Phase 1) |
| [D2](#d2-nco-reference-instead-of-coherent-sampling) | NCO reference instead of coherent sampling | Implemented (model) |
| [D3](#d3-two-channels-sampled-simultaneously-ratio-measurements) | Two channels, sampled simultaneously; ratio measurements | Implemented (model) |
| [D4](#d4-sample-rate-100-kss-per-channel) | Sample rate 100 kS/s per channel | Implemented (model) |
| [D5](#d5-q31-fixed-point-for-the-whole-dsp-chain) | Q31 fixed point for the whole DSP chain | Implemented (model) |
| [D6](#d6-round-half-up-multiply) | Round-half-up multiply | Implemented (model) |
| [D7](#d7-half-scale-headroom-from-the-adc-onwards) | Half-scale headroom from the ADC onwards | Implemented (model) |
| [D8](#d8-power-of-two-boxcar-decimation-before-the-iir) | Power-of-two boxcar decimation before the IIR | Implemented (model) |
| [D9](#d9-cascaded-single-pole-iir-with-64-bit-state) | Cascaded single-pole IIR with 64-bit state | Implemented (model) |
| [D10](#d10-filter-coefficient-computed-on-the-host) | Filter coefficient computed on the host | Implemented (model) |
| [D11](#d11-optional-sync-filter) | Optional sync filter | Implemented (model) |
| [D12](#d12-cordic-is-checked-with-a-tolerance-everything-else-bit-exactly) | CORDIC is checked with a tolerance, everything else bit-exactly | Implemented (model) |
| [D13](#d13-bit-exact-reference-model-as-the-verification-strategy) | Bit-exact reference model as the verification strategy | Implemented (model + CI) |
| [D14](#d14-physical-units-only-at-the-output) | Physical units only at the output | Implemented (model) |
| [D15](#d15-differential-ac-coupled-instrumentation-amplifier-front-end) | Differential, AC-coupled instrumentation-amplifier front end | Planned (Phase 3) |
| [D16](#d16-firmware-structure-dsp-in-the-dma-interrupt-machine-on-tether-main-loop) | DSP in the DMA interrupt, Machine-on-Tether main loop | Planned (Phase 1–2) |
| [D17](#d17-cmsis--ll-not-hal-cmake--arm-none-eabi-gcc) | CMSIS + LL (not HAL); CMake + arm-none-eabi-gcc | Planned (Phase 1) |
| [D18](#d18-host-link-lpuart1-at-921600-baud-through-the-st-link) | Host link: LPUART1 at 921600 baud through the ST-LINK | Planned (Phase 1) |
| [D19](#d19-system-clock-hsi-for-bring-up-then-hse-170-mhz-via-pll-in-boost-mode) | System clock: HSI for bring-up, then HSE; 170 MHz via PLL in boost mode | Planned (Phase 1) |
| [D20](#d20-adc-clock-hclk4--425-mhz-synchronous-with-a-sampling-time-cap) | ADC clock: HCLK/4 = 42.5 MHz, synchronous, with a sampling-time cap | Planned (Phase 1) |
| [D21](#d21-block-size-b--64) | Block size B = 64 | Planned (Phase 1) |
| [D22](#d22-uart-interrupt-above-the-dsp-and-fifo-enabled) | UART interrupt above the DSP; FIFO enabled | Planned (Phase 1) |
| [D23](#d23-missed-deadline-detection-three-independent-checks) | Missed-deadline detection: three independent checks | Planned (Phase 1) |
| [D24](#d24-raw-data-by-burst-capture-in-checksummed-binary-frames-text-commands) | Raw data by burst capture in checksummed binary frames; text commands | Planned (Phase 1) |
| [D25](#d25-nco-and-dac-refill-in-the-adc-dma-isr) | NCO and DAC refill in the ADC DMA ISR | Planned (Phase 1) |
| [D26](#d26-pin-assignment-adc-inputs-on-pa0-and-pa1-dac-on-pa4) | Pin assignment: ADC inputs on PA0 and PA1, DAC on PA4 | Planned (Phase 1) |
| [D27](#d27-tim2-is-the-sample-clock-with-the-trigger-visible-on-pb10) | TIM2 is the sample clock, with the trigger visible on PB10 | Planned (Phase 1) |
| [D28](#d28-debug-pins-pa9-for-isr-busy-pa10-spare) | Debug pins: PA9 for ISR busy, PA10 spare | Planned (Phase 1) |
| [D29](#d29-adc-sampling-time-925-cycles) | ADC sampling time 92.5 cycles | Planned (Phase 1) |
| [D30](#d30-dac-buffered-hfsel-set-cordic-sine-defined-code-mapping) | DAC: buffered, HFSEL set, CORDIC sine, defined code mapping | Planned (Phase 1) |
| [D31](#d31-dma-dmamux-and-interrupt-priorities) | DMA, DMAMUX and interrupt priorities | Planned (Phase 1) |
| [D32](#d32-vendored-cmsis-and-ll-our-own-linker-script) | Vendored CMSIS and LL; our own linker script | Planned (Phase 1) |
| [D33](#d33-machine-on-tether-as-a-small-c-dispatcher) | Machine-on-Tether as a small C dispatcher | Planned (Phase 1) |
| [D34](#d34-human-console-with-a-host-mode) | Human console with a host mode | Planned (Phase 1) |
| [D35](#d35-binary-frame-encoding-little-endian-raw-words-crc-32) | Binary frame encoding: little-endian, raw words, CRC-32 | Planned (Phase 1) |
| [D36](#d36-flashing-with-stm32_programmer_cli-from-cmake) | Flashing with STM32_Programmer_CLI from CMake | Planned (Phase 1) |
| [D37](#d37-repo-layout-host-side-c-tests-and-firmware-ci) | Repo layout, host-side C tests and firmware CI | Planned (Phase 1) |
| [D38](#d38-timing-measured-with-the-dwt-counter-and-a-logic-analyzer) | Timing measured with the DWT counter and a logic analyzer | Planned (Phase 1) |
| [D39](#d39-loopback-drives-both-channels) | Loopback drives both channels | Planned (Phase 1) |

---

## D1. STM32G474 on a Nucleo board

**Decision:** NUCLEO-G474RE (Cortex-M4F at 170 MHz).

**Why:** One inexpensive chip has everything a two-channel lock-in needs:
- two ADCs that can sample *simultaneously* (dual mode)
- a DAC for the excitation
- a CORDIC accelerator that computes sin/cos and modulus/phase in q1.31
- an FMAC filter accelerator
- internal op-amps that can buffer the DAC or V_mid

The Nucleo's ST-LINK provides programming, debugging and a USB serial port with no
extra hardware.

**Alternatives:**
- An external ADC: better resolution, but more parts, cost and SPI timing work.
- An FPGA: kept as a stretch goal for porting the demodulator.

**Cost:**
- The ADCs are 12-bit SAR converters (about 10–11 effective bits).
- VREF+ is the 3.3 V supply, which limits absolute accuracy. Ratio measurements cancel most of this (D3).

## D2. NCO reference instead of coherent sampling

**Decision:** A 32-bit phase accumulator, `phase += Δ` with Δ = round(f_ref/f_s · 2³²).
The same phase drives the DAC excitation and the demodulation reference.

**Why:**
- Any reference frequency is possible, with a resolution of f_s/2³² ≈ 23 µHz.
- Reinterpreting the uint32 phase as int32 gives exactly the CORDIC's q1.31 angle format (±1 ↔ ±π), so no conversion is needed.
- Because one phase drives both the excitation and the reference, they can never drift apart.
- Harmonic k is just `k·phase` (an integer multiply that wraps automatically).

**Alternative:** Coherent sampling (f_s = N·f_ref). An integer number of samples per period
cancels the 2f mixer product exactly. However, f_ref is then limited to values the
timer can divide down to.

**Cost:** Without coherent sampling, the 2f term must be removed by the low-pass filter
(D9) or the sync filter (D11).

## D3. Two channels sampled simultaneously; ratio measurements

**Decision:**
- ADC1 and ADC2 run in dual simultaneous mode, both triggered by the same timer.
- Both channels are demodulated against the same reference.
- The headline output is the ratio H = Z₂/Z₁.

**Why:**
- In a ratio, everything common to both channels cancels: excitation amplitude and phase, DAC error, VREF drift, LED drift.
- That is what lets a low-cost build make credible Bode and absorbance measurements.
- What remains are differences *between* the channels (gain, AC-coupling phase, amplifier bandwidth). A through calibration removes those: both channels measure the same node, the result H_cal(f) is stored, and later measurements are reported as H/H_cal.
- Verified in the model (E8): gain error 0.316 dB → 0.0004 dB, phase error 19° → 0.001°.

**Alternative:** Sampling the two channels one after the other on one ADC.

**Cost of that alternative:** A sampling skew Δt appears as a phase error 2π·f·Δt. For example, a 1 µs skew is 3.6° at 10 kHz.

## D4. Sample rate: 100 kS/s per channel

**Decision:** f_s = 170 MHz / 1700 = 100 kS/s per channel.

**Why:**
- It is 10× the highest reference frequency (10 kHz), so the anti-alias filter can be a simple RC.
- It is an integer division of the core clock.
- It leaves 1,700 CPU cycles per sample pair for the DSP, far more than the tens of cycles it needs.

**Alternative:** A much higher rate (the ADC can do several MS/s). That spends CPU and
memory bandwidth for no benefit in a 10 kHz instrument.

## D5. Q31 fixed point for the whole DSP chain

**Decision:** Every signal in the interrupt is an int32 read as value/2³¹. Float is only
used in the main loop, for output formatting and ratios (D14).

**Why:**
- The CORDIC works in q1.31 natively.
- Integer arithmetic is deterministic, so the firmware can be tested *bit-exactly* against a Python model (D13).
- float32's 24-bit mantissa can't resolve the tiny filter steps needed at long time constants. Q31 with a 64-bit accumulator can (D9).

**Alternative:** float32 on the M4F's FPU. It is simpler to write, but rounding depends on
operation order and compiler flags, so firmware and model would only agree approximately.

**Cost:** Every stage needs a documented range, rounding mode and overflow argument.
Those are in [`sim/Q31-RULES.md`](../sim/Q31-RULES.md).

## D6. Round-half-up multiply

**Decision:** `mul(a,b) = SSAT32(((int64)a*b + 2³⁰) >> 31)`.

**Why:**
- Truncating instead would bias every product by −0.5 LSB.
- After the mixer, that bias is a DC offset on X and Y. A DC offset is exactly what the low-pass passes, and it can't be told apart from a real signal.
- Rounding costs one add.
- Verified unbiased in `test_smoke.py::TestMul::test_rounding_error_is_unbiased`.

## D7. Half-scale headroom from the ADC onwards

**Decision:** ADC codes map to Q31 as `(code − 2048) << 19`, which gives values in [−0.5, 0.5).

**Why:** With inputs limited to half scale:
- mixer products stay within ±0.5
- the IIR error term `x − y` always fits in an int32
- R = √(X² + Y²) ≤ 0.71 fits in Q31

As a result, no stage after the ADC can overflow, and saturation is only a safety net
(Q31-RULES §2–3).

## D8. Power-of-two boxcar decimation before the IIR

**Decision:** Average blocks of M = 2^k mixer samples, with k = 0…16. The average is the
block sum shifted right by k, with rounding.

**Why:**
- The IIR then runs at f_s/M instead of f_s, which cuts CPU load.
- It also makes α M times larger, which shrinks the fixed-point dead band by the same factor (E5).
- A power of two makes normalising an exact shift instead of a division.

**Alternatives:**
- A CIC filter.
- An FIR on the FMAC. The FMAC is only q1.15, so it suits decimation but not the long-τ IIR. Kept as a Phase 6 option.

## D9. Cascaded single-pole IIR with 64-bit state

**Decision:**
- The low-pass is 1–4 identical single-pole stages (6/12/18/24 dB/oct), `y += α(x − y)`.
- Each stage's state is an int64 in Q62, updated with the full α·e product (one SMLAL instruction).

**Why:**
- Identical poles give the standard lock-in time-constant and roll-off behaviour, with known settling times (99% settled at 4.61/6.64/8.41/10.05 τ; verified in E2).
- A naive 32-bit state stalls up to 2³⁰/α LSB short of the input: the update rounds to zero once the error is small enough (E5).
- Accumulating at 62 bits never discards an increment, so the output converges to within 0.5 LSB.

## D10. Filter coefficient computed on the host

**Decision:** α = 1 − exp(−T/τ) is computed in double precision on the PC, using `expm1`, then
rounded to Q31. The firmware receives `alpha_q31`, never τ.

**Why:**
- No `exp()` is needed on the MCU.
- The float model uses the *same* quantised α, so float-vs-Q31 comparisons measure only arithmetic error (E6).

## D11. Optional sync filter

**Decision:** As an alternative to the boxcar, average the mixer output over each complete
reference period, using the NCO phase wrap to mark period boundaries. Only complete
periods are output, and there is one 64-bit division per period.

**Why:**
- A whole-period average has nulls at f_ref and every multiple of it.
- That removes the 2f ripple, which matters most at 10–100 Hz, where the IIR alone would need a very long τ.
- Measured improvement: at least 64 dB (E4). It isn't infinite because f_s/f_ref is not an integer, so each period contains a whole number of samples that doesn't exactly match one cycle.

## D12. CORDIC is checked with a tolerance, everything else bit-exactly

**Decision:**
- The model computes sin/cos and R/θ as ideal values rounded to the nearest LSB.
- The test vectors include the `sin`/`cos` arrays. The C tests feed those to the mixer, so every later stage is checked bit-exactly.
- On hardware, the real CORDIC is compared with the ideal values using a tolerance of a few LSB, and the deviation is reported.

**Why:** ST does not document the CORDIC's internal rounding, so it can't be emulated
bit for bit. Without this split, CORDIC rounding would mask real bugs further down the chain.

## D13. Bit-exact reference model as the verification strategy

**Decision:**
- `sim/` holds a float model (ground truth) and a Q31 model that mirrors the firmware instruction for instruction.
- `sim/vectors/` stores every stage's output for fixed-seed inputs.
- CI checks that the vectors still reproduce exactly.

**Why:**
- A mismatch in the firmware can be traced to one stage.
- Fixed-point error was predicted analytically and confirmed (≈ 1/√12 LSB RMS per stage, E6) before any hardware existed.

## D14. Physical units only at the output

**Decision:** Volts RMS, radians, complex ratios and calibration are applied only in the
output stage (`meas`), in the main loop.

**Why:**
- The interrupt stays integer-only and bit-exact.
- The unit scale chain is written down in one place: 1 Q31 unit = VREF/2³¹ V, and V_rms = √2 · R · VREF/2³¹.

## D15. Differential, AC-coupled instrumentation-amplifier front end

**Decision:** One external instrumentation amplifier (INA) per channel:
- Each input leg has a series capacitor and a bias resistor to mid-rail.
- The INA's REF pin is at mid-rail (≈1.65 V), which centres the output in the ADC range.
- An RC anti-alias filter sits between the INA and the ADC pin.

**Why:**
- Differential inputs can measure across a component that isn't grounded (a sense resistor, a DUT returned to V_mid). Impedance mode needs this.
- AC coupling removes DC offsets that would otherwise limit gain.

**Alternative:** Single-ended inputs biased to mid-rail. This is simpler, but can't measure floating
nodes and rejects common-mode noise poorly.

**Cost:**
- No measurement below about 1 Hz.
- Any mismatch between the two legs' RC corners lowers CMRR at low frequency (to be measured, T9).

## D16. Firmware structure: DSP in the DMA interrupt, Machine-on-Tether main loop

**Decision:**
- **Hard real-time path:** one timer triggers the ADC pair and the DAC. Circular DMA moves the data, and the DSP runs in the DMA half/full-transfer interrupt.
- **Everything else** runs in a cooperative main loop of small state machines: commands, configuration, measurement, sweep, telemetry and status.
- The two sides communicate only through single-producer/single-consumer rings and a few atomic flags.

**Why:**
- The sample timing comes from hardware, so it has no jitter.
- The interrupt's worst case is fixed and measurable.
- The main loop can't block the DSP.

**Alternative:** FreeRTOS. Not needed for a PC-tethered instrument; kept as a stretch goal
for standalone operation.

## D17. CMSIS + LL (not HAL); CMake + arm-none-eabi-gcc

**Decision:**
- Peripherals are configured through CMSIS register definitions and ST's LL (low-layer) library where it reads better.
- The build uses CMake and the Arm GNU toolchain.

**Why:**
- Every register write is visible and reviewable, with no hidden timing or callbacks in the interrupt path.
- CMake builds the same hardware-free modules for the PC (unit tests in CI) and for the target.

**Alternative:** CubeMX/HAL code generation. Faster to start, but it hides the configuration
the project is meant to demonstrate.

## D18. Host link: LPUART1 at 921600 baud through the ST-LINK

**Decision:**
- LPUART1 (PA2/PA3) connects to the ST-LINK virtual COM port.
- The interrupt-driven ring-buffer driver from an earlier project is reused.
- Commands are SCPI-style text.

**Why:** No extra hardware is needed, and the driver is already tested.

**Cost:**
- The link carries about 92 kB/s. Raw two-channel samples need 400 kB/s, so raw data can't be streamed continuously.
- Finished results (10–100 samples/s) fit easily.
- How raw data is captured during bring-up is decided in Phase 1.

---

## D19. System clock: HSI for bring-up, then HSE; 170 MHz via PLL in boost mode

**Decision:**
- Bring-up runs from HSI16 (±1%). Once the chain works, switch to HSE.
- SYSCLK is 170 MHz from the PLL. The PWR regulator is in Range 1 boost before the switch, and flash latency is set first.
- Going above 150 MHz follows RM0440 §6.1.5: set the AHB prescaler to ÷2, select boost (clear `PWR_CR5.R1MODE`), switch to the PLL, wait at least 1 µs, then set the AHB prescaler back to ÷1.

**Why:**
- HSI removes one unknown while the clock tree, PLL and flash settings are first brought up.
- HSI is accurate enough for ratio measurements (D3). It matters only for absolute frequency, which is why HSE comes later.
- The regulator and flash settings are needed for 170 MHz whatever feeds the PLL.

**Alternative:** Start on HSE immediately. This is more accurate but adds a crystal or MCO dependency during bring-up.

**Cost:**
- Until HSE is used, f_s and f_ref are only known to ±1%.
- HSE is the external 24 MHz crystal X3 by default (UM2505 Table 12, p. 26; SB25/SB26 ON, SB27 OFF). The ST-LINK MCO is not used for HSE.
- PLL values for 170 MHz from 24 MHz are M = 6, N = 85, R = 2. HSI16 gives 170 MHz with M = 4, N = 85, R = 2. Both give a 4 MHz PLL input (allowed 2.66–16 MHz) and a 340 MHz VCO (allowed 96–344 MHz in Range 1), per the PLL characteristics in DS12288.
- The AHB ÷2 step above briefly runs the core at 85 MHz. It only happens once, at boot.

## D20. ADC clock: HCLK/4 = 42.5 MHz, synchronous, with a sampling-time cap

**Decision:**
- The ADCs use the synchronous clock (CKMODE = HCLK/4), giving 42.5 MHz.
- The longest sampling time allowed is 247.5 ADC cycles.

**Why:**
- Table 66 (DS12288 p. 138) limits the ADC clock for Range 1, all ADCs, single-ended mode to 52 MHz when VDDA ≥ 2.7 V, and to 42 MHz when VDDA ≥ 1.62 V. The Nucleo runs VDDA at 3.3 V (VDDA = VDD via SB8 OFF, UM2505 p. 25), so the 52 MHz limit applies. HCLK/1 and HCLK/2 exceed it, so /4 is the first setting that fits.
- Synchronous clocking gives a fixed trigger-to-sample latency. Asynchronous clocking adds up to one ADC clock of jitter, which would undo the point of hardware triggering (section 3 of the guide).
- The sampling-time cap comes from the sample period: (SMP + 12.5) / 42.5 MHz ≤ 10 µs.

**Cost:**
- At 640.5 cycles, a conversion takes about 15 µs and overruns the 10 µs period. This sampling time is excluded.
- The 42.5 MHz choice sits within the 52 MHz limit at VDDA = 3.3 V. It would exceed the 42 MHz limit only at VDDA < 2.7 V, which the Nucleo does not use. Differential mode allows 56 MHz, but the design uses single-ended inputs.
- Range 1 boost (D19) is required for these limits.

## D21. Block size B = 64

**Decision:** Each DMA half-buffer holds B = 64 sample pairs.

**Why:**
- The DMA deadline is 640 µs (108,800 CPU cycles), long enough for the DSP.
- The ISR is short enough that the UART (D22) is not blocked for long.
- B is a multiple of M for all M ≤ 64. The boxcar (D8) must carry its partial sum across blocks for M > 64, so the decimator state must persist between ISR calls.

**Alternative:** B = 32 or 128. Smaller increases interrupt rate; larger increases latency and blocking.

## D22. UART interrupt above the DSP, and FIFO enabled

**Decision:**
- LPUART1 interrupt has higher NVIC priority than the DMA ISR.
- The 8-byte LPUART FIFOs are enabled. Receive interrupts on **RX FIFO not empty** (`RXFNEIE`), not on a threshold, and the ISR reads until `RXFNE` clears. Transmit refills the TX FIFO while it is not full.

**Why:**
- The LPUART ISR is a few dozen cycles. Running it first costs the DSP almost nothing against a deadline of hundreds of µs.
- With the FIFO on, the CPU may respond up to about 8 byte-times (about 87 µs at 921600 baud) late without losing data.
- Interrupting on "not empty" means a short command can never sit in the FIFO below a threshold. A threshold interrupt would need a receiver timeout to flush it, and **LPUART1 has no receiver timeout** (RM0440 Table 370, p. 1691: USART/UART only).

**Alternative:** An RX threshold interrupt plus the idle-line interrupt (`IDLEIE`) to flush the remainder. Fewer interrupts, but the interrupt count doesn't matter at command rates.

**Cost:** One interrupt per received burst rather than per FIFO-full. The reused driver, which read one byte per interrupt, gets a drain loop.

*Revised 2026-11-24, before implementation: originally "FIFO with an RX threshold", with the receiver timeout suggested as the flush. That feature doesn't exist on LPUART1.*

## D23. Missed-deadline detection: three independent checks

**Decision:** Each DMA ISR runs these checks and keeps counters that are reported through `status`:
1. **Position check:** at ISR exit, read the DMA CNDTR. If the DMA has already moved into the half just processed, count a late finish.
2. **Alternation check:** HT and TC events must strictly alternate. A repeated flag, or both set on entry, counts a missed interrupt.
3. **ADC overrun:** check the ADC OVR flag. This catches the DMA itself falling behind.

**Why:** Each check catches a different failure:
- Check 1: the ISR started on time but finished too late.
- Check 2: an entire interrupt was missed.
- Check 3: the data path was already losing samples before the CPU saw it.

All three counters have one writer (the ISR) and one reader (the main loop), so no locking is needed. The DAC side has the same deadline, so check 1 is also applied to the DAC DMA stream.

**Cost:** A few reads per interrupt. Negligible.

## D24. Raw data by burst capture in checksummed binary frames; text commands

**Decision:**
- Raw data is captured in a contiguous RAM buffer, then sent as one frame.
- A frame is: magic number, length, f_s, sequence number, payload, checksum.
- Commands are text. Each text reply ends with a newline. A binary frame is preceded by a text header line, so the host can tell the two apart.

**Why:**
- Continuous streaming is impossible at 92 kB/s against 400 kB/s raw (D18).
- Burst capture gives a contiguous run for timing, waveform and FFT checks.
- Binary avoids the three-fold size and parsing ambiguity of text.

**Cost:** Capture length is limited by RAM. 8192 pairs = 32 KB = 82 ms at 100 kS/s.

## D25. NCO and DAC refill in the ADC DMA ISR

**Decision:** The DAC buffer is refilled in the same ISR as the ADC processing. Only the ADC DMA interrupts are enabled. The DAC DMA interrupts stay off.

**Why:** The DAC and ADC share one trigger, so their half-buffer events coincide. One ISR handles both with one deadline (section 4 of the guide). The NCO advances exactly once per sample, in the same order as the DAC buffer is filled.

**Cost:** The ISR does a little more work, but it is already bounded by the deadline in D23.

---

## D26. Pin assignment: ADC inputs on PA0 and PA1, DAC on PA4

**Decision:**
- **CH1 = PA0** (ADC1_IN1, Arduino A0, morpho CN7 pin 28).
- **CH2 = PA1** (ADC2_IN2, Arduino A1, morpho CN7 pin 30).
- **DAC excitation = PA4** (DAC1_OUT1, Arduino A2). For loopback, a jumper connects A2 to A0 so CH1 sees the DAC output.
- PA4 is an ADC2-only input (ADC2_IN17), so it is not used as CH1.

**Why:**
- PA0 and PA1 are both ADC12 channels, so either ADC can take either input in dual simultaneous mode (D3).
- By default the USER button is on PC13, not PA0 (UM2505 Table 12, SB16 ON / SB21 OFF, p. 26). PA0 is therefore free.
- PA0 and PA1 are not used by the ST-LINK VCP (PA2/PA3), LD2 (PA5), SWD (PA13/PA14) or HSE (PF0/PF1).

**Reserved pins (do not reuse):**
- PA2/PA3: LPUART1 to ST-LINK VCP (SB17, SB23 ON).
- PA5: LD2 (SB6 ON).
- PA13/PA14: SWD (SB40, SB41 ON).
- PC13: USER button (SB16 ON).
- PF0/PF1: HSE crystal (SB25/SB26 ON).

**Sources:** Datasheet Table 12 (DS12288, pp. 59–60) for the ADC and DAC alternate functions. UM2505 Table 12 (pp. 26–27) and Table 17 for the board routing.

**Cost:** The loopback needs one jumper (A2 to A0). Without it, CH1 measures nothing from the DAC.

## D27. TIM2 is the sample clock, with the trigger visible on PB10

**Decision:**
- TIM2 generates f_s: `PSC = 0`, `ARR = SYSCLK_HZ / FS_HZ − 1 = 1699`, TRGO = update event (`MMS = 010`).
- TRGO triggers the ADC pair (`EXTSEL = 01011`, rising edge) and DAC1 channel 1 (`TSEL1 = 0100`, dac_ch1_trg4).
- TIM2_CH3 runs PWM mode 1 at 50% duty on **PB10** (AF1, Arduino D6), so its rising edge marks every sample instant.

**Why:**
- Timers that reach both the ADC1/2 regular trigger (RM0440 Table 166) and DAC1 (Table 186) are TIM2, TIM3, TIM4, TIM6, TIM7, TIM8 and TIM15.
- TIM6/TIM7 are basic timers with no output pin. With them, a logic analyzer can only see the interrupt, with its latency included, never the sampling instant.
- With the PWM output, a frequency measurement on PB10 checks f_s and the whole clock tree at once.

**Alternative:** TIM6 plus a GPIO toggle in the ISR. Simpler, but it shows the wrong edge.

**Cost:** TIM2 is a 32-bit timer that would suit external-reference input capture (stretch goal). TIM5 is also 32-bit and can take that role.

## D28. Debug pins: PA9 for ISR busy, PA10 spare

**Decision:**
- **PA9** (Arduino D8) is high while the DMA ISR runs. Its duty cycle is the CPU load.
- **PA10** (Arduino D2) is a spare marker (for example capture armed, or one main-loop pass).
- Both are push-pull, high speed, and written with `BSRR`, a single atomic store.
- `PWR_CR3.UCPD1_DBDIS = 1` at boot.

**Why:**
- Both pins are free on the Nucleo and sit on the Arduino header next to the D6 sample clock (D27).
- Driving PA9 or PA10 high enables the UCPD dead-battery pull-down (5.1 kΩ) on PB6 or PB4 (DS12288 Table 12, note 6). Those pins are unused now. Disabling the feature means it can't cause surprises later.

## D29. ADC sampling time: 92.5 cycles

**Decision:** Both ADCs sample for 92.5 ADC cycles = 2.18 µs at 42.5 MHz. The conversion takes 105 cycles = 2.47 µs, a quarter of the 10 µs period.

**Why:**
- PA0/PA1 are ADC12_IN1/IN2, which are fast channels (IN1–IN5).
- DS12288 Table 67 allows R_AIN ≤ 4.7 kΩ for 92.5 cycles at 12 bits. The table is specified at a 60 MHz ADC clock. At 42.5 MHz the same cycle count is 1.41× longer, so the margin is larger.
- The buffered DAC output (loopback) and a later INA with an RC filter are far below 4.7 kΩ.

**Alternative:** 47.5 cycles (R_AIN ≤ 2.2 kΩ) or 247.5 cycles (≤ 12 kΩ). Either fits within the D20 cap.

**Cost:** None now. The value is a named constant, to be revisited in Phase 3 once the anti-alias R and C are chosen.

## D30. DAC: buffered, HFSEL set, CORDIC sine, defined code mapping

**Decision:**
- DAC1 channel 1 in normal mode with the output buffer on, driving the external pin.
- **`DAC_MCR.HFSEL = 10`**, because the DAC clock (AHB) is 170 MHz.
- The sine comes from the **CORDIC**, fed with the NCO phase.
- Code mapping: `code = 2048 + (((int64)sin_q31 · DAC_AMP + 2³⁰) >> 31)`, round-half-up as in D6, with `DAC_AMP = 1600` codes peak (≈ 1.29 V peak). A `static_assert` checks that 2048 ± DAC_AMP stays inside 248…3847.
- The same mapping is added to the Python model and exported as a test vector.

**Why:**
- HFSEL must be set above 80 MHz. Above 160 MHz the value is 10 (RM0440 §22.4.5, Table 191). Without it, DAC behaviour is "unpredictable".
- The buffered output swings 0.2 V to VREF+ − 0.2 V (DS12288). 1600 codes leaves about 200 codes of margin at each end.
- Phase 2 needs the CORDIC driver anyway. Bringing it up against a visible output is the easiest way to check it.
- The CORDIC isn't bit-exact with the model (D12). After scaling to 12 bits, the codes differ only at rounding boundaries, so the DAC check is "within 1 code".

**Alternative:** A sine lookup table. Bit-exact, but it is throwaway work once the CORDIC is needed in Phase 2.

## D31. DMA, DMAMUX and interrupt priorities

**Decision:**

| | ADC pair | DAC |
|---|---|---|
| Channel | DMA1 Ch1, DMAMUX1 Ch0 | DMA1 Ch2, DMAMUX1 Ch1 |
| Request | `LL_DMAMUX_REQ_ADC1` (5) | `LL_DMAMUX_REQ_DAC1_CH1` (6) |
| Transfer | `ADC12_COMMON->CDR` → `adc_buf[2B]` | `dac_buf[2B]` → `DAC1->DHR12R1` |
| Width | 32 / 32 | 32 / 32 |
| Mode | circular, memory increment | circular, memory increment |
| DMA priority | very high | high |
| Interrupts | HT + TC (D25) | none (D25) |

- ADC common settings: regular simultaneous mode (`DUAL = 00110`), `MDMA = 10` (both 12-bit results packed in one word), `DMACFG = 1` (circular).
- NVIC (lower number = more urgent): LPUART1 = 1, DMA1_Ch1 = 2, SysTick = 15. The worst-case latency argument sits next to these numbers in the code.

**Why:**
- In dual mode only the master (ADC1) requests DMA, so one transfer moves one sample pair (D3).
- The ADC stream gets the higher DMA priority: losing an ADC sample corrupts data, and the DAC's DHR keeps its last value if a transfer is a little late.
- Named LL constants are used instead of numbers, because DMAMUX IDs are easy to misread from the reference manual's tables.

**Note:** On the G4 the DMA can reach SRAM1, SRAM2 and CCM SRAM (RM0440 §2.1), so buffers can go anywhere in RAM.

## D32. Vendored CMSIS and LL; our own linker script

**Decision:**
- `firmware/third_party/` holds a pinned minimal subset of:
  - ST `cmsis-device-g4` (Apache-2.0): device header, `system_stm32g4xx.c`, GCC startup file
  - Arm CMSIS core headers (Apache-2.0)
  - ST `stm32g4xx-hal-driver` (BSD-3-Clause), LL headers and sources only
- Each upstream licence file is kept, and a README records the upstream tags.
- The linker script is our own (about 60 lines): 512 KB flash, 128 KB RAM at 0x20000000.
- `SystemInit` from ST is kept (it enables the FPU and sets VTOR). Clock setup is ours (D19).

**Why:**
- BasicBlink's startup and linker files are for the STM32G491, with a different vector table and less RAM.
- Vendoring a small subset is simpler than submodules, for CI and for anyone cloning the repo.
- A short handwritten linker script can be reviewed line by line.

**Alternative:** Git submodules of the full ST repositories. These are easier to update, but each clone and CI run must fetch them.

## D33. Machine-on-Tether as a small C dispatcher

**Decision:**
- The main loop is a table of `{ name, init(), poll() }` devices. `main` calls each `poll()` in turn, forever.
- A SysTick interrupt gives a 1 ms tick (`millis()`) for timeouts and periodic work. It replaces `HAL_GetTick` in the reused telemetry code.
- The design follows the Machine-on-Tether idea from ENEE440. The course's prebuilt `libMoTsystem.a` is not used.

**Why:**
- The course library is precompiled assembly. It can't be reviewed, probably can't be published under MIT, and uses its own hex command protocol, which conflicts with D18's text commands.
- A cooperative dispatcher is about 50 lines, and every device's behaviour stays visible.

## D34. Human console with a host mode

**Decision:**
- The reused telemetry console stays for interactive use (PuTTY), at 921600 baud.
- A `mode host` command turns off echo, line redrawing, asynchronous events and the status stream. The host script sends it first.
- Binary frames never go through `tlm_printf`. The capture device writes them with `serial_write()` in chunks no bigger than `serial_tx_free()`, so a 32 KB burst passes through the 2 KB TX ring without blocking the main loop.

**Why:**
- In console mode the code echoes input, redraws the line around events, streams status by default and turns `\n` into `\r\n`. Any of these would corrupt or interleave with binary frames (D24).
- `serial_flush()` blocks, so it is used only before a reset.
- Baud check: BRR = 256 · 16 MHz / 921600 = 4444, within the valid range, with an error of 0.01%.

## D35. Binary frame encoding: little-endian, raw words, CRC-32

**Decision** (refines D24):
- Everything is little-endian.
- The payload is the raw packed 32-bit ADC words (ADC1 in the low half, ADC2 in the high half). The host unpacks them.
- A fixed-size, versioned header: magic, format version, payload length, f_s, NCO Δ, the NCO phase of the DAC sample aligned with ADC sample 0, DAC amplitude, sequence number, and a snapshot of the D23 error counters.
- The checksum is CRC-32 with the zlib polynomial. The G4 hardware CRC unit computes it with polynomial 0x04C11DB7, init 0xFFFFFFFF and input/output bit reversal. The unit has no final XOR, so the firmware XORs with 0xFFFFFFFF.
- A host-side C test checks `crc32("123456789") = 0xCBF43926`.

**Why:**
- Both ends are little-endian, so nothing is byte-swapped.
- Sending raw words keeps the ISR and capture path free of repacking.
- With the start phase, the host can rebuild the exact DAC sequence and measure the loopback delay (guide §3).
- The host verifies with one call to `zlib.crc32`.

## D36. Flashing with STM32_Programmer_CLI from CMake

**Decision:** A CMake `flash` target runs `STM32_Programmer_CLI -c port=SWD -w <elf> -v -rst`. The tool's path is a CMake cache variable, defaulting to the copy inside the STM32CubeIDE installation. Debugging uses VS Code + Cortex-Debug or CubeIDE on the same ELF.

**Why:** The programmer CLI is already installed (inside CubeIDE) and verifies after writing. The build doesn't depend on any debugger.

**Alternative:** OpenOCD, which is also bundled with CubeIDE. Either works.

## D37. Repo layout, host-side C tests and firmware CI

**Decision:**
- `firmware/`: `CMakeLists.txt`, `cmake/arm-gcc.cmake`, and `CMakePresets.json` with two presets. **target** builds for the MCU; **host** builds the hardware-free modules for the PC. Sources live in `src/bsp/`, `src/app/` and `src/lib/` (`ringbuf`, `nco`, `frame`, `crc`), with `third_party/` (D32).
- `host/`: the Python capture tool (`pyserial`), kept separate from `sim/`.
- Host C tests use CTest and a small assert macro, with no framework. Phase 1 tests: `nco` against `sim/vectors/*/phase.bin`, frame encoding and CRC, the ring buffer.
- CI gets a `firmware` job: build the target preset with Arm GNU Toolchain **14.2** (pinned to match the local install), then build and run the host preset's tests.

**Why:**
- One CMake project builds the same modules for both target and PC (D17).
- The Phase 1 tests are small, and a framework can come later if they grow.

## D38. Timing measured with the DWT counter and a logic analyzer

**Decision:**
- The DWT cycle counter measures every DMA ISR (min/max/mean), reported by `status`. It is always on.
- An 8-channel 24 MHz USB logic analyzer with PulseView records the timing diagram: PB10 sample clock (D27), PA9 ISR busy (D28), and the UART TX line.

**Why:**
- The DWT gives exact cycle counts and works without extra hardware, so bring-up doesn't wait for the analyzer.
- The analyzer shows what the DWT can't: ISR entry jitter against the true sample instant, and how the UART and DSP interrupts interleave. At 24 MS/s the resolution is 42 ns, ample for a 10 µs sample period.

**Cost:** About $10–15 from the parts budget. The analyzer arrives after bring-up, so the timing diagram is recorded at the end of Phase 1, not during the timer step.

## D39. Loopback drives both channels

**Decision:** For Phase 1 loopback, the DAC output (A2/PA4) is jumpered to **both** A0 and A1. This extends the single jumper in D26.

**Why:**
- With the same signal on both channels, the measured phase difference should be zero. That is a direct hardware check of the simultaneous sampling that D3 depends on.
- The two channels' gain and offset can also be compared, a preview of the Phase 4 through-calibration.

**Cost:** One extra jumper wire.
