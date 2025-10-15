#include <stdint.h>

/* Stesso indirizzo del mailbox del punto 1 */
#define CORE1_MB_ADDR   ((volatile uint32_t *)0x003FF000u)

static inline uint32_t read_mpidr(void)
{
    uint32_t mpidr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
    return mpidr;
}

/* Constructor con priorità molto alta: viene chiamato PRIMA di main() */
__attribute__((constructor(101)))
static void early_core_mux_ctor(void)
{
    /* Se è CPU1, devi “saltare” all’entry impostato da Core0 nel mailbox. */
    uint32_t mpidr = read_mpidr();
    uint32_t cpu_id = mpidr & 0x3u;   // A9: CPU ID nei bit[1:0]

    if (cpu_id == 1u) {
        /* Leggi l’entry che Core0 ha scritto nel mailbox */
        uint32_t entry = *CORE1_MB_ADDR;
        if (entry != 0u) {
            __asm__ volatile("dsb sy; isb");
            /* Salto diretto: BX al reset handler dell’immagine Core1 */
            void (*entry_fn)(void) = (void (*)(void))(uintptr_t)entry;
            entry_fn();
            while (1) { /* non ritorna */ }
        } else {
            /* Se per qualche motivo l’entry non è ancora stato scritto,
               rimani in attesa “gentile” (o spin con WFE se preferisci) */
            for(;;) {
                uint32_t e = *CORE1_MB_ADDR;
                if (e) {
                    void (*f)(void) = (void (*)(void))(uintptr_t)e;
                    f();
                }
            }
        }
    }
    /* CPU0: continua il boot normale → ritorna al CRT e poi va in main() */
}
