#ifndef ADDRESS_MAP_H
#define ADDRESS_MAP_H

#include <stdint.h>

/* Based on RP2040 Datasheet found at
 * https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
 */

#define CLOCKS_BASE			((volatile uint32_t *) 0x40010000)
#define RESETS_BASE			((volatile uint32_t *) 0x40020000)
#define IO_BANK0_BASE			((volatile uint32_t *) 0x40028000)
#define PADS_BANK0_BASE			((volatile uint32_t *) 0x40038000)

#define SIO_BASE			((volatile uint32_t *) 0xD0000000)
#define XOSC_BASE			((volatile uint32_t *) 0x40048000)
#define PLL_SYS_BASE			((volatile uint32_t *) 0x40050000)
#define PLL_USB_BASE			((volatile uint32_t *) 0x40058000)
#define UART0_BASE			((volatile uint32_t *) 0x40070000)
#define UART1_BASE			((volatile uint32_t *) 0x40078000)

#endif // ADDRESS_MAP_H
