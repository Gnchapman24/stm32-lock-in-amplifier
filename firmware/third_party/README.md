# Vendored third-party code

A pinned, minimal subset of upstream sources (decision D32). Only the files the
firmware uses are copied. **Do not edit them.** To update, copy the same files
from a newer tag and update this table.

| Directory | Upstream | Tag | Commit | Licence |
|---|---|---|---|---|
| `cmsis_core/` | [STMicroelectronics/cmsis_core](https://github.com/STMicroelectronics/cmsis_core) (ST's mirror of Arm CMSIS 5 Core) | `v5.9.0_20250520` | `b7487de1303e2eb77c402432e368691e9dcfb5f0` | Apache-2.0 (`cmsis_core/LICENSE.md`) |
| `cmsis_device_g4/` | [STMicroelectronics/cmsis-device-g4](https://github.com/STMicroelectronics/cmsis-device-g4) | `v1.2.6` | `25664ddc3a7624ae9627ae8c4c672073dc5b2539` | Apache-2.0 (`cmsis_device_g4/LICENSE.md`) |
| `stm32g4xx_ll/` | [STMicroelectronics/stm32g4xx-hal-driver](https://github.com/STMicroelectronics/stm32g4xx-hal-driver), LL headers only | `v1.2.7` | `f5929f431f9effe45fbe18f5337e4753ced9ac92` | BSD-3-Clause (`stm32g4xx_ll/LICENSE.md`) |

## What was copied

- `cmsis_core/Include/`: `cmsis_compiler.h`, `cmsis_gcc.h`, `cmsis_version.h`,
  `core_cm4.h`, `mpu_armv7.h` (from `Include/`).
- `cmsis_device_g4/Include/`: `stm32g4xx.h`, `stm32g474xx.h`, `system_stm32g4xx.h`.
- `cmsis_device_g4/Source/`: `system_stm32g4xx.c` (from `Source/Templates/`) and
  `startup_stm32g474xx.s` (from `Source/Templates/gcc/`).
- `stm32g4xx_ll/Inc/`: `stm32g4xx_ll_{adc,bus,cordic,cortex,crc,dac,dma,dmamux,gpio,lpuart,pwr,rcc,system,tim,utils}.h`.
  The LL functions used are `static inline` in these headers, so no LL `.c` files are
  needed.

## How they are used

- `startup_stm32g474xx.s` holds the vector table and `Reset_Handler`. It expects
  the linker symbols `_sidata`, `_sdata`, `_edata`, `_sbss`, `_ebss` and `_estack`,
  which `../linker/stm32g474re_flash.ld` provides.
- `SystemInit()` from `system_stm32g4xx.c` runs before `main`. It enables the FPU
  and, because the build defines `USER_VECT_TAB_ADDRESS`, sets `VTOR` to the start
  of flash. The clock tree is configured by our own `src/bsp/clock.c`, not by ST code.
