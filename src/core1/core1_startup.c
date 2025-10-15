#include <stdint.h>

extern unsigned long __bss_start__, __bss_end__, __stack_top__;
extern unsigned long __data_start__, __data_end__, __data_load__;
extern void core1_main(void);

/* Entry linkata nel linker: ENTRY(_start_core1) */
static void __attribute__((used)) core1_startup_body(void)
{
	 /* copia le sezioni .data dall'area di load (flash/DDR alta) all'area di run */
	for (unsigned long *src = &__data_load__, *dst = &__data_start__; dst < &__data_end__; ++src, ++dst) {
		*dst = *src;
	}

    /* azzera BSS */
    for (unsigned long *p = &__bss_start__; p < &__bss_end__; ++p) {
        *p = 0UL;
    }

    /* qui puoi settare VBAR se usi vettori in DDR
       // uint32_t vectors = (uint32_t)&__vectors_start__;
       // __asm__ volatile ("mcr p15,0,%0,c12,c0,0" :: "r"(vectors) : "memory");
       // __asm__ volatile("isb");
    */

    (void)core1_main();

    for (;;) { /* stop */ }
}

/* Entry linkata nel linker: ENTRY(_start_core1) */
void __attribute__((naked)) _start_core1(void)
{
    /* set SP in cima alla regione DDR_PRIV */
    __asm__ volatile (
        "ldr sp, =__stack_top__\n\t"
        "b core1_startup_body\n\t"
        ::: "memory");
}


/*
void _socfpga_main(void)
{
    // NOP: evita di far crashare il linker. Non viene usata.
}*/
