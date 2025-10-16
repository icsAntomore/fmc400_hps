// qspi_utils.c

#include <stdio.h>
#include <qspi.h>
#include "alt_printf.h"
#include "uart_stdio.h"
#include <stdint.h>


// Se la tua HWLIB ha ECC per Arria10, abilitalo (è dichiarato nel tuo header con #if defined(soc_a10))
static ALT_STATUS_CODE qspi_init_safe(void)
{
    ALT_STATUS_CODE s;

    // 0) Assicurati che il controller sia in stato pulito
    (void)alt_qspi_uninit();   // ok anche se non era init
    (void)alt_qspi_disable();  // ok anche se era già disabilitato

    // 1) (Opzionale ma utile) rallenta la clock prima dell’init: /16 o /32
    // Se fallisce, prosegui lo stesso.
    (void)alt_qspi_baud_rate_div_set(ALT_QSPI_BAUD_DIV_16);

    // 2) Init di base del controller
    s = alt_qspi_init();
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_init() fail: %d", (int)s);
        return s;
    }

#if defined(soc_a10)
    // 3) (Solo Arria10) Avvia ECC RAM del QSPI (API dichiarata nel tuo header)
    s = alt_qspi_ecc_start();
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_ecc_start() fail: %d (continuo comunque)", (int)s);
        // non fermarti, alcune board non lo richiedono
    }
#endif

    // 4) Config di timing base molto conservativa (clock inattiva alta, ecc.)
    ALT_QSPI_TIMING_CONFIG_t tcfg = {
        .clk_phase = ALT_QSPI_CLK_PHASE_INACTIVE,
        .clk_pol   = ALT_QSPI_CLK_POLARITY_HIGH,
        .cs_da     = 2,
        .cs_dads   = 2,
        .cs_eot    = 2,
        .cs_sot    = 2,
        .rd_datacap= 1        // aggiungi 1 ciclo di capture per sicurezza
    };
    (void)alt_qspi_timing_config_set(&tcfg); // Se non supportato, ignora il ritorno

    // 5) Config dimensioni device (valori GENERICI, modificali poi per il tuo chip)
    ALT_QSPI_DEV_SIZE_CONFIG_t dsz = {
        .block_size        = 16,   // 2^16 = 64KB block (classico dei NOR)
        .page_size         = 256,  // 256B
        .addr_size         = 2,    // 3-byte addressing (0=1B,1=2B,2=3B,3=4B)
        .lower_wrprot_block= 0,
        .upper_wrprot_block= 0,
        .wrprot_enable     = false
    };
    (void)alt_qspi_device_size_config_set(&dsz); // alcune lib ignorano, ok

    // 6) Config istruzione di READ “safe”: 0x0B (Fast Read), single I/O, 8 dummy
    ALT_QSPI_DEV_INST_CONFIG_t rcfg = {
        .op_code       = 0x0B,                 // Fast Read
        .inst_type     = ALT_QSPI_MODE_SINGLE,
        .addr_xfer_type= ALT_QSPI_MODE_SINGLE,
        .data_xfer_type= ALT_QSPI_MODE_SINGLE,
        .dummy_cycles  = 8
    };
    (void)alt_qspi_device_read_config_set(&rcfg);

    // 7) Chip select singolo, nessun decode (ss_n[0] attivo)
    (void)alt_qspi_chip_select_config_set(0xE, ALT_QSPI_CS_MODE_SINGLE_SELECT);
    // mappa: cs=xxx0 => nSS[3:0]=1110 (seleziona CS0)

    // 8) Abilita il controller
    s = alt_qspi_enable();
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_enable() fail: %d", (int)s);
        return s;
    }

    // 9) Verifica idle
    if (!alt_qspi_is_idle()) {
        printf("\r\nQSPI: controller non idle dopo enable.");
        // Non è per forza un errore fatale, ma segnalo
    }

    // 10) Info utile per capire se il bus risponde
    const char* name = alt_qspi_get_friendly_name();
    if (name) printf("\r\nQSPI: device='%s'", name);
    printf("\r\nQSPI: size=%u, page=%u, multi-die=%d, die_sz=%u",
               (unsigned)alt_qspi_get_device_size(),
               (unsigned)alt_qspi_get_page_size(),
               (int)alt_qspi_is_multidie(),
               (unsigned)alt_qspi_get_die_size());

    return ALT_E_SUCCESS;
}

ALT_STATUS_CODE qspi_read(uint8_t *dst1024, size_t len, uint32_t addr)
{
    if (!dst1024) return ALT_E_BAD_ARG;

    ALT_STATUS_CODE s = qspi_init_safe();
    if (s != ALT_E_SUCCESS) return s;

    s = alt_qspi_read(dst1024, addr, len);
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_read() fail: %d", (int)s);
        return s;
    }

    (void)alt_qspi_uninit();
    return ALT_E_SUCCESS;
}


/********************************************************************************
 ********************* Funzioni per avvio CORE 1 da DDR *************************
 ********************************************************************************/
/* Copia bloccante QSPI -> DDR */
static ALT_STATUS_CODE qspi_copy_to_ddr(uint32_t qspi_ofs, void *ddr_dst, size_t len)
{
    ALT_STATUS_CODE s = qspi_init_safe();
    if (s != ALT_E_SUCCESS) return s;

    s = alt_qspi_read(ddr_dst, qspi_ofs, len);
    (void)alt_qspi_uninit();

    return s;
}

/* 1) Legge da QSPI -> DDR e fa flush delle cache sulla regione */
int core1_load_from_qspi_to_ddr(void)
{
    /* Tieni CPU1 in reset mentre copi (facoltativo ma consigliato) */
    uint32_t r = rd32(RSTMGR_MPUMODRST_ADDR);
    wr32(RSTMGR_MPUMODRST_ADDR, r | (1u<<1));
    barrier();

    ALT_STATUS_CODE s = qspi_copy_to_ddr(CORE1_QSPI_SRC, (void*)CORE1_DDR_BASE, CORE1_IMAGE_SIZE);
    if (s != ALT_E_SUCCESS) {
        alt_printf("\r\nQSPI read fail: %d", (int)s);
        return -1;
    }

    /* Flush cache L1/L2 sulla regione image */
    uintptr_t start = align_down_cache((uintptr_t)CORE1_DDR_BASE);
    uintptr_t end   = align_up_cache  ((uintptr_t)CORE1_DDR_BASE + CORE1_IMAGE_SIZE);
    alt_cache_system_clean((void *)start, (size_t)(end - start));
    alt_cache_l1_instruction_invalidate();

    barrier();

    return 0;
}

/* 2) Programma l'indirizzo di start di CPU1 nel SYSMGR (Arria 10) */
void core1_set_start_addr(uint32_t phys_entry)
{
    wr32(A10_ROM_CPU1START_ADDR, phys_entry);
    barrier();
}

/* 3) Togli CPU1 dal reset (RSTMGR.MPUMODRST bit1 = 0) */
void core1_release_from_reset(void)
{
    uint32_t v = rd32(RSTMGR_MPUMODRST_ADDR);
    v &= ~(1u << 1);
    wr32(RSTMGR_MPUMODRST_ADDR, v);
    barrier();
}

/* 4) Sequenza completa: QSPI->DDR, trampoline, set start, release */
int core1_boot_from_ddr(void)
{
    if (core1_load_from_qspi_to_ddr() != 0) {
        return -1;
    }
    /* Scrivi entry fisico nel registro ROMCODE CPU1STARTADDR (A10) */
    core1_set_start_addr((uint32_t)CORE1_DDR_BASE);
    scu_enable();
    /* Rilascia CPU1 */
    core1_release_from_reset();

    alt_printf("\r\nCore1 avviato da DDR @0x%08x (size 0x%08x)", CORE1_DDR_BASE, CORE1_IMAGE_SIZE);


    uint32_t tries = 0;
    while (tries++ < 1000000u) {
        if (*(volatile uint32_t*)0x3F000000u == 0xC0DE0001u) {
            printf("\n[OK] Core1 ha eseguito l'entry minimal.\n");
            break;
        }
    }
    if (tries >= 1000000u) {
        printf("\n[KO] Core1 non ha mai scritto in SHM.\n");
    }


    return 0;
}








#define L2_AF_START   0xFFFFFC00u  // L2C-310 Address Filtering Start (A10)
#define L2_AF_END     0xFFFFFC04u  // L2C-310 Address Filtering End
#define OCRAM_BASE   0xFFE00000u
#define L3_REMAP     0xFF800000u

static inline void dsb_isb(void){ __asm__ volatile("dsb sy\nisb"); }

static inline void map_ocram_to_zero(void) {
    *(volatile uint32_t*)L3_REMAP = 1u;  // mpuzero=1
    __asm__ volatile("dsb sy\nisb");
}

static inline void write_trampoline(uint32_t entry)
{
    volatile uint32_t *z = (uint32_t*)OCRAM_BASE;
    z[0] = 0xE51FF004;  // LDR PC,[PC,#-4]
    z[1] = entry;
    alt_cache_system_clean((void*)z, 32);
    __asm__ volatile("dsb sy\nisb");
}


/* Mappa SDRAM sull'area 0x00000000..0xBFFFFFFF */
static inline void map_sdram_to_zero(void)
{
    /* Start: [31:20]=upper12bits(start); [0]=enable
       Qui Start=0x00000001 ⇒ start=0x00000000 + enable */
    ALT_CAST(volatile uint32_t*, L2_AF_START)[0] = 0x00000001u;
    /* End: [31:20]=upper12bits(end) ⇒ 0xC00 => 0xC0000000 */
    ALT_CAST(volatile uint32_t*, L2_AF_END)[0] = 0xC0000000u;
    dsb_isb();
}

/* Scrive un micro-trampolino in DDR @ 0x00000000 */
static inline void write_trampoline_in_ddr0(uint32_t entry_phys)
{
    volatile uint32_t *z = (uint32_t*)0x00000000u;
    z[0] = 0xE51FF004;        // LDR PC, [PC, #-4]
    z[1] = entry_phys;        // es. CORE1_DDR_BASE
    alt_cache_system_clean((void*)z, 32);
#if defined(ALT_CACHE_SUPPORTS_L2)
    alt_cache_l2_writeback_all();
#endif
    dsb_isb();
}


/* Disabilita/abilita IRQ+FIQ brevemente mentre tocchiamo 0x0 */
static inline uint32_t save_and_disable_irqs(void){
    uint32_t cpsr;
    __asm__ volatile ("mrs %0, cpsr" : "=r"(cpsr));
    /* set I (IRQ) e F (FIQ) */
    __asm__ volatile ("cpsid if" ::: "memory");
    dsb_isb();
    return cpsr;
}
static inline void restore_irqs(uint32_t cpsr){
    /* ripristina I/F come erano */
    if (!(cpsr & (1u<<7))) __asm__ volatile("cpsie i");
    if (!(cpsr & (1u<<6))) __asm__ volatile("cpsie f");
    dsb_isb();
}

void debug_dump_core1_regs(void)
{
    // Arria 10: System Manager (ROM block) e Reset Manager
    const uint32_t SYSMGR_ROM_BASEv   = 0xFFD06200u;
    const uint32_t ROM_CTRL          = SYSMGR_ROM_BASEv + 0x00;  // romcode_ctrl
    const uint32_t CPU1START         = SYSMGR_ROM_BASEv + 0x08;  // romcode_cpu1startaddr  (== A10_SYSMGR_CORE1_START_ADDR_ADDR)
    const uint32_t INITSWSTATE       = SYSMGR_ROM_BASEv + 0x0C;  // romcode_initswstate
    const uint32_t RSTMGR_MPUMODRST  = 0xFFD05000u + 0x20u;     // bit1=CPU1
    const uint32_t SCU_CTRL          = 0xFFFFC000u;             // bit0=EN

    alt_printf("\nROM_CTRL     @0x%08x = 0x%08x", ROM_CTRL, rd32(ROM_CTRL));
    alt_printf("\nCPU1START    @0x%08x = 0x%08x", CPU1START, rd32(CPU1START));
    alt_printf("\nINITSWSTATE  @0x%08x = 0x%08x", INITSWSTATE, rd32(INITSWSTATE));
    alt_printf("\nMPUMODRST    @0x%08x = 0x%08x", RSTMGR_MPUMODRST, rd32(RSTMGR_MPUMODRST));
    alt_printf("\nSCU_CTRL     @0x%08x = 0x%08x", SCU_CTRL, rd32(SCU_CTRL));
}



/* 2.2: test bring-up mantenendo i tuoi nomi/flow */
int core1_boot_minimal_probe(void)
{
    /* 0) Tieni CPU1 in reset */
    uint32_t v = alt_read_word(ALT_RSTMGR_MPUMODRST_ADDR);
    alt_write_word(ALT_RSTMGR_MPUMODRST_ADDR, v | (1u << 1));
    dsb_isb();


    map_sdram_to_zero();
    write_trampoline_in_ddr0((uint32_t)CORE1_DDR_BASE);

    //3) Debug: verifica che 0x0 ALIASI l’OCRAM e contenga il trampolino
       uint32_t o0 = *(volatile uint32_t*)OCRAM_BASE;
       uint32_t o1 = *(volatile uint32_t*)(OCRAM_BASE + 4);
       uint32_t z0 = *(volatile uint32_t*)0x00000000u;
       uint32_t z1 = *(volatile uint32_t*)0x00000004u;
       alt_printf("\n[DBG] OCRAM[0:1]=%08x %08x  ZERO[0:1]=%08x %08x",
                  o0, o1, z0, z1);
       //Atteso: OCRAM[0]=E51FF004, OCRAM[1]=01000000, ZERO[0..1] identici

       // 4) programma l’entry fisico nel registro A10
       core1_set_start_addr((uint32_t)CORE1_DDR_BASE);   // usa la tua macro A10 @ 0xFFD06208

       // 5) SCU on (se cache su entrambi i core)
       *(volatile uint32_t*)0xFFFFC000u |= 1u;  // SCU_CTRL bit0
       dsb_isb();


       v = alt_read_word(ALT_RSTMGR_MPUMODRST_ADDR);
          alt_write_word(ALT_RSTMGR_MPUMODRST_ADDR, v & ~(1u << 1));
          dsb_isb();
          __asm__ volatile("sev");

          //7) sonda: aspetta che Core1 scriva in SHM
          volatile uint32_t *shm = (volatile uint32_t*)SHM_BASE;
          for (uint32_t i=0; i<1000000u; ++i){
              if (shm[0] == 0xC0DE0001u){ alt_printf("\n[OK] probe."); return 0; }
          }
          alt_printf("\n[KO] probe.");
    return -1;
}

