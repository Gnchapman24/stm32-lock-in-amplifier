# Firmware for the STM32G474 lock-in amplifier (decisions D17, D32, D36, D37).
#
# One project, two builds, chosen by the preset:
#   target  cross-compiles the firmware for the MCU (cmake/arm-gcc.cmake)
#   host    builds the hardware-free modules in src/lib for the PC and runs
#           their unit tests with CTest
#
#   cmake --preset target && cmake --build --preset target
#   cmake --build --preset target --target flash        # program the Nucleo
#   cmake --preset host && cmake --build --preset host && ctest --preset host

cmake_minimum_required(VERSION 3.25)

if(CMAKE_TOOLCHAIN_FILE)
    project(lockin_firmware C ASM)
else()
    project(lockin_firmware C)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)                 # gnu11: inline asm, __attribute__
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings for our own code only; the vendored files are compiled as they are.
set(LOCKIN_WARNINGS -Wall -Wextra -Wshadow -Wdouble-promotion -Wundef -Wstrict-prototypes)
option(LOCKIN_WERROR "Treat warnings in our own code as errors" ON)
if(LOCKIN_WERROR)
    list(APPEND LOCKIN_WARNINGS -Werror)
endif()

# Hardware-free modules: built for both the MCU and the PC.
set(LIB_SOURCES
    src/lib/crc32.c
)

if(CMAKE_CROSSCOMPILING)
    # ------------------------------------------------------------------ target
    option(LOCKIN_USE_HSE "Clock the PLL from the 24 MHz HSE crystal instead of HSI16 (D19)" OFF)
    option(LOCKIN_NO_PLL  "Bring-up: stay on HSI16 at 16 MHz, never start the PLL" OFF)

    # Firmware version from git, fixed at configure time (re-run cmake to update).
    execute_process(
        COMMAND git describe --always --dirty --tags
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE FW_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT FW_VERSION)
        set(FW_VERSION "unknown")
    endif()

    set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32g474re_flash.ld)

    add_library(vendor OBJECT
        ${TP}/cmsis_device_g4/Source/startup_stm32g474xx.s
        ${TP}/cmsis_device_g4/Source/system_stm32g4xx.c
    )
    target_include_directories(vendor PUBLIC
        ${TP}/cmsis_core/Include
        ${TP}/cmsis_device_g4/Include
        ${TP}/stm32g4xx_ll/Inc
    )
    target_compile_definitions(vendor PUBLIC
        STM32G474xx
        USER_VECT_TAB_ADDRESS                 # SystemInit sets VTOR = start of flash
    )

    add_executable(lockin
        ${LIB_SOURCES}
        src/bsp/clock.c
        src/bsp/fault.c
        src/bsp/gpio.c
        src/bsp/sample_clock.c
        src/bsp/serial_port.c
        src/bsp/syscalls.c
        src/bsp/tick.c
        src/app/main.c
        src/app/mot.c
        src/app/telemetry.c
    )
    set_target_properties(lockin PROPERTIES SUFFIX ".elf")
    target_include_directories(lockin PRIVATE src/lib src/bsp src/app)
    target_compile_options(lockin PRIVATE ${LOCKIN_WARNINGS})
    target_compile_definitions(lockin PRIVATE
        FW_VERSION="${FW_VERSION}"
        LOCKIN_USE_HSE=$<BOOL:${LOCKIN_USE_HSE}>
        LOCKIN_NO_PLL=$<BOOL:${LOCKIN_NO_PLL}>
    )
    target_link_libraries(lockin PRIVATE vendor)
    target_link_options(lockin PRIVATE
        -T${LINKER_SCRIPT}
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/lockin.map
        -Wl,--no-warn-rwx-segments
    )
    set_target_properties(lockin PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

    add_custom_command(TARGET lockin POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:lockin> lockin.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:lockin> lockin.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:lockin>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "lockin.bin / lockin.hex"
        VERBATIM)

    # Flashing (D36): STM32_Programmer_CLI, from PATH or an STM32CubeIDE install.
    file(GLOB _cubeide_programmers
        "C:/ST/STM32CubeIDE_*/STM32CubeIDE/plugins/*cubeprogrammer*/tools/bin"
        "/opt/st/stm32cubeide_*/plugins/*cubeprogrammer*/tools/bin")
    find_program(STM32_PROGRAMMER_CLI STM32_Programmer_CLI
        HINTS ${_cubeide_programmers}
        DOC "Path to STM32_Programmer_CLI (used by the flash target)")
    if(STM32_PROGRAMMER_CLI)
        add_custom_target(flash
            COMMAND ${STM32_PROGRAMMER_CLI} -c port=SWD -w $<TARGET_FILE:lockin> -v -rst
            DEPENDS lockin
            USES_TERMINAL
            COMMENT "Programming the Nucleo over SWD")
    else()
        message(STATUS "STM32_Programmer_CLI not found: no 'flash' target "
                       "(set -DSTM32_PROGRAMMER_CLI=<path>)")
    endif()

else()
    # -------------------------------------------------------------------- host
    enable_testing()
    add_library(lockin_lib STATIC ${LIB_SOURCES})
    target_include_directories(lockin_lib PUBLIC src/lib)
    target_compile_options(lockin_lib PRIVATE ${LOCKIN_WARNINGS})
    add_subdirectory(tests)
endif()
