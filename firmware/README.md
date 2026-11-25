# Firmware

Bare-metal C for the NUCLEO-G474RE: CMSIS + LL headers, no HAL, CMake +
`arm-none-eabi-gcc` (decisions D17, D32, D36, D37 in [`../docs/decisions.md`](../docs/decisions.md)).

## Build

Needs the Arm GNU Toolchain 14.2 (`arm-none-eabi-gcc` on PATH), CMake ≥ 3.25 and Ninja.

```sh
cmake --preset target && cmake --build --preset target     # build/target/lockin.elf
cmake --build --preset target --target flash                # program over SWD
cmake --preset host && cmake --build --preset host && ctest --preset host   # PC unit tests
```

- `flash` uses `STM32_Programmer_CLI`. CMake looks for it on PATH and inside an
  STM32CubeIDE install. If it isn't found, pass `-DSTM32_PROGRAMMER_CLI=<path>`.
- `-DLOCKIN_USE_HSE=ON` clocks the PLL from the 24 MHz crystal instead of HSI16 (D19).
- `-DLOCKIN_NO_PLL=ON` never starts the PLL and runs everything at 16 MHz. This is for
  the first bring-up steps.
- After a HardFault the next boot also stays at 16 MHz (safe mode), so the console can
  report the fault (`fault`). A further `reset` tries 170 MHz again.

## Layout

| Path | Contents |
|---|---|
| `src/lib/` | Hardware-free modules. Built for the MCU **and** the PC, and unit-tested on the PC. |
| `src/bsp/` | Board support: clock, tick, GPIO, serial port, fault handler. Talks to registers. |
| `src/app/` | `main` (boot + device table), the Machine-on-Tether dispatcher, the console. |
| `tests/` | Host-side tests for `src/lib`, run by CTest. |
| `linker/` | Our linker script (512 KB flash, 128 KB contiguous RAM). |
| `third_party/` | Pinned subset of ST/Arm CMSIS and LL. See its README. |

## Console

LPUART1 through the ST-LINK virtual COM port at **921600 baud**, 8N1, no flow
control. In PuTTY set local echo and local line editing to "force off".
Type `help` for the commands. A script sends `mode host` first: from then on there
is no echo or unsolicited output, and every reply ends with a line `#END`.
