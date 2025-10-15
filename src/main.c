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
#include "shared_ipc.h"
#include "socal/socal.h"
#include "socal/alt_rstmgr.h"
#include "core1_start_a10.h"
#include "qspi.h"

#define CORE1_ENTRY_ADDR   0x01000000u

extern volatile uint32_t *g_arm_pio_data;
extern volatile uint32_t *g_arm_msgdma0_csr;
extern volatile uint32_t *g_arm_msgdma1_csr;
extern volatile uint32_t *g_arm_msgdma2_csr;
extern volatile uint32_t *g_arm_msgdma3_csr;
extern volatile uint32_t *g_arm_msgdma4_csr;
extern volatile uint32_t *g_arm_f2h_irq0_en;

extern const unsigned char _binary_app_core1_bin_start[];
extern const unsigned char _binary_app_core1_bin_end[];
extern const unsigned char _binary_app_core1_bin_size[];

#define CORE1_ENTRY_PHYS   0x01000000u   /* ORIGIN(DDR_PRIV) del linker di core1 */

static void load_core1_image(void)
{
    const unsigned char *src = _binary_app_core1_bin_start;
    const unsigned char *end = _binary_app_core1_bin_end;
    volatile unsigned char *dst = (volatile unsigned char *)(uintptr_t)CORE1_ENTRY_PHYS;

    while (src < end) {
        *dst++ = *src++;
    }
    __asm__ volatile("dsb sy; isb" ::: "memory");
}


int main(int argc, char** argv)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    /* Disable watchdogs */
    alt_wdog_stop(ALT_WDOG0);
    alt_wdog_stop(ALT_WDOG1);


    // subito all’inizio del main
    arm_cache_set_enabled(false);   // spegne I/D cache + branch predictor
    arm_icache_invalidate_all();
    arm_dcache_clean_invalidate_all();


    if (status == ALT_E_SUCCESS) status = arm_mmu_setup_core0();

    if (status == ALT_E_SUCCESS) status = arm_mm_open();
    if (status == ALT_E_SUCCESS) status = arm_pio_write(g_arm_pio_data,0x00000000);        // chiude canale output
    if (status == ALT_E_SUCCESS) status = arm_pio_write(g_arm_pio_data,0x80000000);        // apre canale output

    /* Inizializza GIC una sola volta */
    if (status == ALT_E_SUCCESS) status = socfpga_GIC_init();

    /* ---- QUI aggiungi le priorità ---- */
    if (status == ALT_E_SUCCESS) {
        // consenti tutte le priorità e nesting
        alt_int_cpu_priority_mask_set(0xFF);   // PMR: non filtra nulla
        alt_int_cpu_binary_point_set(0);       // permette preemption annidata

        // mSGDMA più urgente del trigger
        alt_int_dist_priority_set(IRQ_ID_F2H0_5, 0x20); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_4, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_3, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_2, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_1, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_0, 0x60); // trigger
        alt_int_dist_priority_set(ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE, 0xA0);

        // per sicurezza: indirizza tutti su CPU0
        alt_int_dist_target_set(IRQ_ID_F2H0_5, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_4, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_3, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_2, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_1, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_0, 0x1);
    }

    if (status == ALT_E_SUCCESS) {
        status = socfpga_int_start(	ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE,
        							socfpga_timer_int_callback,
									NULL,
									ALT_INT_TRIGGER_EDGE); /* Start the interrupt system */
    }

    if (status == ALT_E_SUCCESS) {
    	socfpga_int_start(	IRQ_ID_F2H0_0,
    						fpga_f2h0_isr,
							NULL,
							ALT_INT_TRIGGER_EDGE);
    }

    /* Start the timer system */
    if (status == ALT_E_SUCCESS) status = socfpga_timer_start(ALT_GPT_CPU_PRIVATE_TMR, 1);

    if (status == ALT_E_SUCCESS) status = uart_stdio_init_uart1(115200);


    printf("\nFMC400 Start!");
    if (status == ALT_E_SUCCESS)
    	printf("\nFMC400 Init is OK");
    else
    	printf("\nFMC400 Init is FAIL");

    // (opz.) stampa stato per verifica
    arm_cache_dump_status();        // deve mostrare I=0 D=0 BP=0
    change_pulse();

    /* 1 riceve dati da CPU100 PULSE e REF da mettere in memoria
     * 2 finito questo passaggio abilita il trasferimento DMA dei coefficienti
     * 3 ci deve essere una funzione che gira di continuo che sente la variazione dei PULSE e di conseguenza
     *   dei REF sia quando li riceve ex novo, sia quando l'operatore vuole cambiare la configurazione da trasmettere
     */
    start_mSGDMA(g_arm_msgdma0_csr,0);
    start_mSGDMA(g_arm_msgdma1_csr,1);
    start_mSGDMA(g_arm_msgdma2_csr,2);
    start_mSGDMA(g_arm_msgdma3_csr,3);
    start_mSGDMA(g_arm_msgdma4_csr,4);

    arm_pio_write(g_arm_f2h_irq0_en,1); //enable interrupt del trigger!!! bisogna farlo dopo aver inizializzato MSGDMA

    if (status == ALT_E_SUCCESS) {
    	socfpga_int_start(	IRQ_ID_F2H0_1,
    						sgdma0_int_callback,
							NULL,
							ALT_INT_TRIGGER_LEVEL);
    }

    if (status == ALT_E_SUCCESS) {
    	socfpga_int_start(	IRQ_ID_F2H0_2,
        					sgdma1_int_callback,
    						NULL,
    						ALT_INT_TRIGGER_LEVEL);
	}

    if (status == ALT_E_SUCCESS) {
        socfpga_int_start(	IRQ_ID_F2H0_3,
        					sgdma2_int_callback,
    						NULL,
    						ALT_INT_TRIGGER_LEVEL);
    }

    if (status == ALT_E_SUCCESS) {
        socfpga_int_start(	IRQ_ID_F2H0_4,
        					sgdma3_int_callback,
    						NULL,
    						ALT_INT_TRIGGER_LEVEL);
    }

    if (status == ALT_E_SUCCESS) {
    	socfpga_int_start(	IRQ_ID_F2H0_5,
    						sgdma4_int_callback,
							NULL,
							ALT_INT_TRIGGER_LEVEL);
    }


    sched_insert(SCHED_PERIODIC,ledsys,300);
    //sched_insert(SCHED_PERIODIC,change_pulse,5000);

    const size_t   len  = 1024u;
    uint8_t buf[len];
    ALT_STATUS_CODE s = qspi_read(buf,len,CORE1_QSPI_SRC);

    if (s == ALT_E_SUCCESS) {
    	alt_printf("\r\nQSPI read OK. First 16 bytes => ");
    	for (int i = 0; i < 16; ++i)
    		alt_printf("%02x ", buf[i]);
    	alt_printf("\n");
    }
    else {
    	alt_printf("\nQSPI read Fail");

    }


//#ifdef CORE1

    // 1) Inizializza la shared memory
	//    (MMU off + cache off su Core0 = già NC, quindi scrivi direttamente)
	SHM_CTRL->magic = SHM_MAGIC_BOOT;
	SHM_CTRL->core0_ready = 0u;
	SHM_CTRL->core1_ready = 0u;
	SHM_CTRL->trig_count  = 0u;
	SHM_CTRL->log_head = SHM_CTRL->log_tail = 0u;

	load_core1_image();
	//start_core1(CORE1_ENTRY_ADDR);

	SHM_CTRL->core0_ready = 1u;

	 if (core1_boot_from_ddr() != 0) {
	        alt_printf("Core1 boot failed\n");
	    }

	//if (SHM_CTRL->core1_ready == 1) //core 1 ready
//#endif


    if (status == ALT_E_SUCCESS) {
        while (1) {
        	sched_manager();
        } /* Wait for the timer to be called X times. */
    }

    socfpga_timer_stop(ALT_GPT_CPU_PRIVATE_TMR); /* Stop the timer system */
    socfpga_int_stop(ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE); /* Stop the interrupt system */

    if (status == ALT_E_SUCCESS) {
        //printf("RESULT: Example completed successfully.\n");
    	status = arm_pio_write(g_arm_pio_data,0x00000000);        // chiude canale output
        return 0;
    }
    else {
        //printf("RESULT: Some failures detected.\n");
        return 1;
    }
}
