#include "alt_interrupt.h"
#include "alt_bridge_manager.h"
#include "interrupts.h"

// Basi GIC su A9 HPS (Arria 10/Cyclone V)
#define GIC_CPU_IF_BASE   0xFFFEC100u   // CPU interface base
#define GIC_ICCEOIR       (GIC_CPU_IF_BASE + 0x10u)  // End Of Interrupt

void gic_eoi(uint32_t irq_id)
{
    __asm__ volatile("dsb sy" ::: "memory");
    alt_write_word((void*)(uintptr_t)GIC_ICCEOIR, irq_id);  // EOI
    __asm__ volatile("isb" ::: "memory");
}


ALT_STATUS_CODE socfpga_GIC_init(void)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    if (status == ALT_E_SUCCESS) status = alt_int_global_init();
    if (status == ALT_E_SUCCESS) status = alt_int_cpu_init();
    if (status == ALT_E_SUCCESS) status = alt_int_cpu_enable();
    if (status == ALT_E_SUCCESS) status = alt_int_global_enable();

    return status;
}


/* Initializes and enables the interrupt controller */
ALT_STATUS_CODE socfpga_int_start(ALT_INT_INTERRUPT_t int_id,
                                      alt_int_callback_t callback,
                                      void *context,
                                      ALT_INT_TRIGGER_t trigger)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    /* Disabilita la sola linea mentre la configuri */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_disable(int_id);
    /* Pulisci eventuali pending “sporchi” sulla linea */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_pending_clear(int_id);
    /* Setup the interrupt specific items */
    if (status == ALT_E_SUCCESS) status = alt_int_isr_register(int_id, callback, context);

    /* Edge-triggered */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_trigger_set(int_id, trigger);
    /* set priorità */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_priority_set(int_id, 0x80);


    if ((status == ALT_E_SUCCESS) && (int_id >= 32)) {
        int target = 0x1; /* 1 = CPU0, 2=CPU1 */
        status = alt_int_dist_target_set(int_id, target);
    }

    /* Enable the distributor, CPU, and global interrupt */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_enable(int_id);
    /*if (status == ALT_E_SUCCESS) status = alt_int_cpu_enable();
    if (status == ALT_E_SUCCESS) status = alt_int_global_enable();*/

    return status;
}

void socfpga_int_stop(ALT_INT_INTERRUPT_t int_id)
{
    /* Disable the global, CPU, and distributor interrupt */

    alt_int_global_disable();
    alt_int_cpu_disable();
    alt_int_dist_disable(int_id);

    alt_int_isr_unregister(int_id); /* Unregister the ISR. */

    /* Uninitialize the CPU and global data structures. */

    alt_int_cpu_uninit();
    alt_int_global_uninit();
}



