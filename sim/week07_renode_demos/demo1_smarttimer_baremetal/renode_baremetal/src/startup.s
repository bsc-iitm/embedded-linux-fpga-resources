/* Minimal startup code for ARM Cortex-A9 bare-metal
 * Sets up stack and jumps to main()
 */

.section .text.boot
.global _start

_start:
    /* Set up stack pointer */
    ldr sp, =__stack_top

    /* Clear BSS section */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
bss_loop:
    cmp r0, r1
    bge bss_done
    str r2, [r0], #4
    b bss_loop
bss_done:

    /* Call main() */
    bl main

    /* Halt if main returns */
halt:
    wfi
    b halt
