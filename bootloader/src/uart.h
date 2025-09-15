#ifndef _UART_H
#define _UART_H

#include <stdint.h>
#include "interrupt.h"

void uart_init(void);

int uart_getc(void);
int uart_putc(int c);
int uart_puts(const char *str);
void uart_rx_int_en(void);
void uart_set_callback(void (*callback)(void));

#endif /* _UART_H */
