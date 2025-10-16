#include <stdint.h>

extern unsigned long __bss_start__, __bss_end__, __stack_top__;
extern unsigned long __data_start__, __data_end__, __data_load__;
extern void core1_main(void);

/* Entry linkata nel linker: ENTRY(_start_core1) */
//static void __attribute__((used)) core1_startup_body(void)
static void __attribute__((used, section(".text.startup"))) core1_startup_body(void)
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
//void __attribute__((naked)) _start_core1(void)
/* Entry linkata nel linker: ENTRY(_start_core1)
 *
 * Assicurati che il reset handler sia il primissimo simbolo nella
 * sezione .text, così l'indirizzo di entry corrisponde anche alla base
 * dell'immagine caricata in DDR (0x0100_0000).  Core0 programma il
 * registro “CPU1 start address” con l'indirizzo base della regione
 * copiata da QSPI e si aspetta quindi che il vettore di reset si trovi
 * proprio a quell'indirizzo.  Se il linker posiziona altre funzioni
 * prima di `_start_core1` (ad esempio lo stub usato per generare
 * l'exidx), Core1 parte da codice errato e rimane bloccato.  Forziamo il
 * posizionamento del simbolo nella sezione `.text.startup` che è la
 * prima ad essere emessa dal linker script. */
/*void __attribute__((naked, section(".text.startup._start_core1"))) _start_core1(void)
{
    // set SP in cima alla regione DDR_PRIV
    __asm__ volatile (
        "ldr sp, =__stack_top__\n\t"
        "b core1_startup_body\n\t"
        ::: "memory");
}*/

#define SHM_BASE   ((volatile uint32_t*)0x3F000000u) // come nel tuo linker (SHM_BASE)
#define SHM_PROBE  (*(volatile uint32_t*)(0x3F000000u))  // offset 0

extern unsigned long __stack_top__;
void _start_core1(void) __attribute__((naked, section(".text.startup._start_core1")));
void _start_core1(void)
{
    __asm__ volatile (
        "ldr sp, =__stack_top__      \n\t"
        :
        :
        : "memory"
    );
    SHM_PROBE = 0xC0DE0001u;   // <<< segnale "sono partito"
    __asm__ volatile ("dmb sy" ::: "memory");
    for(;;) __asm__ volatile ("wfi");   // fermo qui: niente MMU/VBAR/UART
}
