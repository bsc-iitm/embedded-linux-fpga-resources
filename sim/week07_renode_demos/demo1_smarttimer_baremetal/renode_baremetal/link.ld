/* Linker script for bare-metal ARM Cortex-A9 on Zynq (Renode)
 * Memory layout matches ZedBoard/Zynq platform in Renode
 */

ENTRY(_start)

MEMORY
{
    /* DDR RAM starts at 0x00000000 in Zynq, 512 MB available */
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 1M
}

SECTIONS
{
    .text : {
        *(.text.boot)    /* Boot code first */
        *(.text*)        /* Code */
        *(.rodata*)      /* Read-only data */
    } > RAM

    .data : {
        *(.data*)        /* Initialized data */
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss*)         /* Uninitialized data */
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Stack grows downward from end of RAM */
    . = ALIGN(8);
    . = . + 0x10000;     /* 64 KB stack */
    __stack_top = .;
}
