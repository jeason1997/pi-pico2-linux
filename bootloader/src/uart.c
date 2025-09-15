#include <stdint.h>
#include <stddef.h>

#include "uart.h"
#include "resets.h"
#include "rp2350-map.h"

#define UART_UARTCR		12
#define UART_UARTCR_UARTEN_BITS	BIT(0)
#define UART_UARTCR_TXE_BITS	BIT(8)
#define UART_UARTCR_RXE_BITS	BIT(9)

#define UART_UARTFR		6
#define UART_UARTFR_TXFF_SHIFT	5
#define UART_UARTFR_RXFE_SHIFT	4

#define UART_UARTIBRD		9
#define UART_UARTFBRD		10

#define UART_LCR_H		11
#define UART_WLEN_8		0x3
#define UART_LCR_H_WLEN_OFFSET	5

#define BAUD			9600
#define F_PERIPH		12000000ULL

#define UARTBAUDRATE_DIV \
	((8u * (F_PERIPH) / (BAUD)) + 1)

#define UARTIBRD_PRE_VALUE ( (UARTBAUDRATE_DIV) >> (7) )

#define UARTIBRD_VALUE ( ( (UARTIBRD_PRE_VALUE) == 0) ? 1 : \
			( (UARTIBRD_PRE_VALUE) >= 65535 ? 65535 : \
			(UARTIBRD_PRE_VALUE) ) )

#define UARTFBRD_VALUE ( ( ((UARTIBRD_PRE_VALUE) == 0 \
			|| (UARTIBRD_PRE_VALUE) >= 65535) \
			? 0 : (((UARTBAUDRATE_DIV) & 0x7f) >> 1) ) )

void uart_init(void) {
	UART0_BASE[UART_UARTIBRD] = UARTIBRD_VALUE;
	UART0_BASE[UART_UARTFBRD] = UARTFBRD_VALUE;

	UART0_BASE[UART_LCR_H] = (UART_WLEN_8 << UART_LCR_H_WLEN_OFFSET);
	UART0_BASE[UART_UARTCR] = UART_UARTCR_UARTEN_BITS | UART_UARTCR_TXE_BITS | UART_UARTCR_RXE_BITS;
}

static inline void uart_send(char c) {
	loop_until_bit_is_clear(UART0_BASE[UART_UARTFR], UART_UARTFR_TXFF_SHIFT);
	*((uint8_t *)UART0_BASE) = c;
}

int uart_putc(int c) {
	uart_send(c);

	return 0;
}

int uart_puts(const char *str) {
	while(*str)
		uart_send(*str++);

	return 0;
}

int uart_getc(void) {
	if (bit_is_set(UART0_BASE[UART_UARTFR], UART_UARTFR_RXFE_SHIFT))
		return -1;

	return *((uint8_t *)UART0_BASE);
}
