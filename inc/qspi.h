#include <stdint.h>
#include <stddef.h>
#include "socal/hps.h"
#include "socal/socal.h"        // alt_read_word(), alt_write_word()
#include "socal/alt_rstmgr.h"   // ALT_RSTMGR_MPUMODRST_ADDR
#include "alt_cache.h"          // alt_cache_system_clean()
#include "alt_qspi.h"           // alt_qspi_init(), alt_qspi_read(), ...

/* --- Indirizzi Arria 10 HPS --- */
#define RSTMGR_BASE              0xFFD05000u
#define RSTMGR_MPUMODRST_ADDR    (RSTMGR_BASE + 0x20u)  /* bit1=CPU1, bit0=CPU0 */
#define SYSMGR_ROM_BASE          0xFFD06200u
#define A10_ROM_CPU1START_ADDR   (SYSMGR_ROM_BASE + 0x08u)  /* romcode_cpu1startaddr */
#define L3_REMAP_ADDR            0xFF800000u               /* NIC-301 GPV remap */
#define SCU_CTRL_ADDR            0xFFFFC000u               /* SCU Control Register (bit0=EN) */

#define CORE1_DDR_BASE           0x20000000u  /* tuo indirizzo fisico */
#define CORE1_IMAGE_SIZE         0x00020000u  /* es. 128 KiB */
#define CORE1_QSPI_SRC			 0x00B00000u

#define SHM_BASE         		 0x3F000000u


static inline void wr32(uint32_t a, uint32_t v){ *(volatile uint32_t*)a = v; }
static inline uint32_t rd32(uint32_t a){ return *(volatile uint32_t*)a; }
static inline void barrier(void){ __asm__ volatile("dsb sy\nisb"); }

/* Allineamento a cache-line (32B) per alt_cache_* */
#ifndef ALT_CACHE_LINE_SIZE
#  define ALT_CACHE_LINE_SIZE 32u
#endif
static inline uintptr_t align_down_cache(uintptr_t a){ return a & ~(uintptr_t)(ALT_CACHE_LINE_SIZE - 1); }
static inline uintptr_t align_up_cache(uintptr_t a){ return (a + (ALT_CACHE_LINE_SIZE - 1)) & ~(uintptr_t)(ALT_CACHE_LINE_SIZE - 1); }

/* SCU on (se userai cache su entrambi i core) */
static inline void scu_enable(void){
    *(volatile uint32_t*)SCU_CTRL_ADDR |= 1u; /* bit0=enable */
    barrier();
}



/* Funzioni */
int  core1_boot_from_ddr(void);
int core1_boot_minimal_probe(void);
ALT_STATUS_CODE qspi_read(uint8_t *dst1024, size_t len, uint32_t addr);

