// Purpose: minimal AArch32 crt0 to set SP and jump to main
    .syntax unified
    .arch armv7-a
    .text
    .global _start
_start:
    // Set up a simple stack near top of RAM (0x8000_0000 + 64 MiB)
    ldr r0, =0x84000000
    mov sp, r0
    bl  main
1:  b   1b

