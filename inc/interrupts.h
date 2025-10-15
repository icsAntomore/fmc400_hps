#include "alt_fpga_manager.h"
#include "alt_interrupt.h"

/* Initializes and enables the interrupt controller.*/
void gic_eoi(uint32_t irq_id);

ALT_STATUS_CODE socfpga_GIC_init(void);

ALT_STATUS_CODE socfpga_int_start(ALT_INT_INTERRUPT_t int_id,
                                       alt_int_callback_t callback,
                                       void *context,
                                       ALT_INT_TRIGGER_t trigger);

void socfpga_int_stop(ALT_INT_INTERRUPT_t int_id);
