.section .vectors, "ax"
    .align 5
    .global __core1_vectors
__core1_vectors:
    b   _start_core1        /* Reset */
    b   .                   /* Undefined */
    b   .                   /* SWI */
    b   .                   /* Prefetch abort */
    b   .                   /* Data abort */
    b   .                   /* Reserved */
    b   .                   /* IRQ */
    b   .                   /* FIQ */
    .size __core1_vectors, .-__core1_vectors
