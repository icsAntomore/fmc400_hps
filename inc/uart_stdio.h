#include <stdint.h>
#include <inttypes.h>
#include "alt_16550_uart.h"
#include "alt_clock_manager.h"
#include "alt_bridge_manager.h"



/* Inizializza UART1 e prepara lo standard I/O su UART1 (baud tipico: 115200). */
ALT_STATUS_CODE uart_stdio_init_uart1(uint32_t baud);

/* Facoltativo: CRLF su newline (abilita se ti serve \r\n) */
void uart_stdio_set_crlf(int enable);
