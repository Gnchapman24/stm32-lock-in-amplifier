/*
 * Linker script for the STM32G474RE: 512 KB flash, 128 KB SRAM (decision D32).
 *
 * RAM is treated as one contiguous 128 KB block at 0x20000000:
 *   SRAM1  80 KB  0x20000000
 *   SRAM2  16 KB  0x20014000
 *   CCM    32 KB  0x20018000  (alias of 0x10000000, reachable by the CPU and DMA1/2
 *                              through the S-bus; DS12288 section 3.6, RM0440 section 2.1)
 * so the stack can sit at the very top and DMA buffers can go anywhere.
 *
 * Symbols required by ST's startup_stm32g474xx.s:
 *   _sidata  load address of .data in flash
 *   _sdata, _edata, _sbss, _ebss
 *   _estack  initial stack pointer (top of RAM)
 * Symbol required by newlib's _sbrk (nosys.specs): end
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Minimum sizes. The link fails if they don't fit after .data/.bss/.noinit.
 * vsnprintf in the console needs well over 1 KB of stack. */
_Min_Heap_Size  = 0x400;
_Min_Stack_Size = 0x1000;

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    /* Vector table first: the core fetches the initial SP and reset vector from 0x08000000. */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } >FLASH

    /* Constructor tables walked by __libc_init_array (called from Reset_Handler). */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH
    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH
    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* Initialised data: runs from RAM, initial values stored in flash and copied by Reset_Handler. */
    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    /* Zero-initialised data: cleared by Reset_Handler. */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* Not touched by Reset_Handler, so it survives a software reset.
     * Used by fault.c to report the last HardFault after the reboot. */
    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        *(.noinit)
        *(.noinit*)
        . = ALIGN(4);
    } >RAM

    /* Heap starts at `end` and grows up; the stack starts at _estack and grows down.
     * This section only reserves the minimum sizes so the link fails if RAM is too full. */
    ._user_heap_stack (NOLOAD) :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
