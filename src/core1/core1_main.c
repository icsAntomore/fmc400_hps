/*****************************************************************************
*
* ICS Technologies SPA. All Rights Reserved.
* 
*****************************************************************************/
#include <string.h>
#include <inttypes.h>
#include "alt_printf.h"
#include <stdbool.h>
#include "alt_clock_manager.h"
#include "alt_timers.h"
#include "alt_interrupt.h"
#include "alt_watchdog.h"
#include "alt_fpga_manager.h"
#include "interrupts.h"
#include "timers.h"
#include "arm_pio.h"
#include "schedule.h"
#include "f2h_interrupts.h"
#include "uart_stdio.h"
#include "arm_mem_regions.h"
#include "msgdma.h"
#include "dma_layout.h"

#include "arm_mem_regions.h"
#include "shared_ipc.h"


static inline void init_data(void)
{
    extern uint8_t __data_start__;
    extern uint8_t __data_end__;
    extern uint8_t __data_load__;

    const uint8_t *src = &__data_load__;
    for (uint8_t *dst = &__data_start__; dst < &__data_end__; ++dst, ++src) {
        *dst = *src;
    }
}


static inline void zero_bss(void)
{
    extern uint8_t __bss_start__;
    extern uint8_t __bss_end__;
    for (uint8_t *p = &__bss_start__; p < &__bss_end__; ++p) *p = 0;
}

extern char _vectors;  // dichiarazione simbolo dal linker/HWLIB
static inline void set_vbar(void *addr) {
    __asm__ volatile ("mcr p15,0,%0,c12,c0,0" :: "r"(addr) : "memory");
    __asm__ volatile ("isb");
}

void core1_main(void)
{
	set_vbar(&_vectors);
	init_data();
    zero_bss();

    // MMU di Core1 (crea le regioni: DDR WBWA + SHM NON-CACHEABLE + Device)
    (void)arm_mmu_setup_core1();

    // UART (se Core0 l’ha già messa su, puoi solo usare printf;
    // in alternativa, re-inizializza uart1 qui)
    // Il binary di Core1 ha una propria copia di uart_stdio.c, quindi
    // deve inizializzare il relativo handle della UART prima di usare
    // printf. Reinizializzare la periferica è sicuro e consente di
    // condividere la stessa UART con Core0.
    (void)uart_stdio_init_uart1(115200);

    // Attendi che Core0 abbia inizializzato tutto e “aperto” l’handshake
    while (SHM_CTRL->magic != SHM_MAGIC_BOOT) { /* spin */ }
    while (SHM_CTRL->core0_ready != 1u)        { /* spin */ }

    // Saluta e dichiara “ready”
    printf("\n[Core1] hello from DDR ready.");
    printf("\n[Core1] hello from DDR low. SHM @ 0x%08X ready.", SHM_BASE);
    SHM_CTRL->core1_ready = 1u;
    __asm__ volatile("dmb sy" ::: "memory");

    // Esempio d’uso: aggiorna un contatore condiviso
    for (;;) {
        SHM_CTRL->trig_count++;
        __asm__ volatile("dmb sy" ::: "memory");
        // ... il tuo loop
    }


}
