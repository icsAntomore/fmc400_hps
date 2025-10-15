#include <stdint.h>
#include <stddef.h>
#include "socal/hps.h"
#include "socal/socal.h"        // alt_read_word(), alt_write_word()
#include "socal/alt_rstmgr.h"   // ALT_RSTMGR_MPUMODRST_ADDR
#include "alt_cache.h"          // alt_cache_system_clean()
#include "alt_qspi.h"           // alt_qspi_init(), alt_qspi_read(), ...


/* ---- Parametri che puoi adattare ---- */
#define CORE1_QSPI_SRC          (0x00B00000u)       /* offset in QSPI */
#define CORE1_IMAGE_SIZE        (0x00020000u)       /* 128 KiB        */
#define CORE1_DDR_BASE          (0x01000000u)       /* dove caricare in DDR */

/* ---- SYSMGR: CPU1 start address (Arria10) ----
 * Non esiste macro HWLib “pronta”.
 * Registro = ALT_SYSMGR_ADDR + 0xC4
 */
#ifndef A10_SYSMGR_CORE1_START_ADDR_ADDR
#define A10_SYSMGR_CORE1_START_ADDR_ADDR \
    ALT_CAST(void*, (ALT_CAST(char*, ALT_SYSMGR_ADDR) + 0x000000C4))
#endif

/* Funzioni */
int  core1_load_from_qspi_to_ddr(void);
void core1_set_start_addr(uint32_t addr);
void core1_release_from_reset(void);
int  core1_boot_from_ddr(void);
ALT_STATUS_CODE qspi_read(uint8_t *dst1024, size_t len, uint32_t addr);

