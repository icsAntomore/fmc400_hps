#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "arm_pio.h"
#include "interrupts.h"
#include "alt_interrupt.h"


ALT_STATUS_CODE socfpga_timer_start(ALT_GPT_TIMER_t timer, uint32_t period_in_ms);
void socfpga_timer_stop(ALT_GPT_TIMER_t timer);
void socfpga_timer_int_callback(uint32_t icciar, void * context);
