/*
 * Provide a minimal function with unwind metadata so that the
 * linker always emits a valid .ARM.exidx table for the core1 image.
 *
 * Without at least one entry, the final ELF ends up with an empty
 * .ARM.exidx section whose sh_link field is left unset.  Recent
 * binutils warn about that situation and the build is reported as
 * failed.  The stub below forces the assembler to emit a cantunwind
 * record that references the stub function, which keeps the linker
 * happy without adding any meaningful code to the firmware.
 */

    .syntax unified
    .arm

    .section .text.__core1_unwind_stub, "ax", %progbits
    .global __core1_unwind_stub
    .type   __core1_unwind_stub, %function
    .hidden __core1_unwind_stub
__core1_unwind_stub:
    .fnstart
    bx      lr
    .cantunwind
    .fnend

    .size   __core1_unwind_stub, .-__core1_unwind_stub
