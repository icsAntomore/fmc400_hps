#pragma once
#include <stdint.h>
#include "socal/socal.h"
#include "socal/hps.h"            // ALT_SYSMGR_ADDR, ALT_RSTMGR_ADDR
#include "socal/alt_rstmgr.h"     // ALT_RSTMGR_MPUMODRST_* macros

/* ----------------------------------------------------------------
   Arria-10: registro "CPU1 start address"
   La tua HWLIB non espone un macro pronto. Usiamo l’offset 0xC4
   dalla base del System Manager (come su Cyclone V e diversi BSP A10).
   Se il tuo TRM/BSP usa un offset diverso, cambia 0xC4 qui sotto.
   ---------------------------------------------------------------- */
#ifndef ALT_SYSMGR_CORE1_START_ADDR
/* Base System Manager: ALT_SYSMGR_ADDR (da socal/hps.h) */
#define ALT_SYSMGR_CORE1_START_ADDR \
    ALT_CAST(void *, (ALT_CAST(char *, ALT_SYSMGR_ADDR) + 0xC4))
#endif

/* Scrive l’entry point di Core1 nel registro del System Manager. */
static inline void core1_set_start(uint32_t entry_addr)
{
    /* barriera */
    __asm__ volatile("dsb sy" ::: "memory");
    alt_write_word(ALT_SYSMGR_CORE1_START_ADDR, entry_addr);
    __asm__ volatile("dsb sy; isb" ::: "memory");
}

/* Rilascia CPU1 dal reset (CPU1 parte dall’indirizzo impostato sopra). */
static inline void core1_release_from_reset(void)
{
    /* Metti a 0 il bit CPU1 nel MPUMODRST (clear bit = esci dal reset). */
    alt_clrbits_word(ALT_RSTMGR_MPUMODRST_ADDR, ALT_RSTMGR_MPUMODRST_CPU1_SET_MSK);
    __asm__ volatile("dsb sy; isb" ::: "memory");
}

/* Sequenza completa: set start + release. */
static inline void start_core1(uint32_t entry_addr)
{
    core1_set_start(entry_addr);
    core1_release_from_reset();
}
