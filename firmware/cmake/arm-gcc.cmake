# Toolchain file: Arm GNU Toolchain (arm-none-eabi-gcc) for the STM32G474 (Cortex-M4F).
#
# Used by the "target" preset. The compiler must be on PATH, or set
# ARM_TOOLCHAIN_DIR to its bin/ directory.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(DEFINED ENV{ARM_TOOLCHAIN_DIR})
    set(_tc "$ENV{ARM_TOOLCHAIN_DIR}/")
else()
    set(_tc "")
endif()

set(CMAKE_C_COMPILER   "${_tc}arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "${_tc}arm-none-eabi-gcc")
set(CMAKE_OBJCOPY      "${_tc}arm-none-eabi-objcopy" CACHE FILEPATH "objcopy")
set(CMAKE_SIZE         "${_tc}arm-none-eabi-size"    CACHE FILEPATH "size")

# Compiler checks can't link a bare-metal executable without a linker script.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M4 with the single-precision FPU, hard-float ABI.
# __FPU_USED (and so SystemInit's CPACR write that turns the FPU on) depends on
# these flags. Without them the FPU stays off and the first float instruction faults.
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${MCU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${MCU_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,--print-memory-usage")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
