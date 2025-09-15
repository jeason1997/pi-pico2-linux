#include <stdint.h>

#include "rp2350-map.h"
#include "helpers.h"
#include "gpio.h"
#include "pads.h"
#include "resets.h"
#include "uart.h"
#include "clocks.h"

#define LED_PIN			25

static inline void set_uart0_pinmux(void);

int main(void) {
	init_xosc();
	set_perif_clock_xosc();

	set_reset(RESETS_PLL_USB, 0);
	set_reset(RESETS_PLL_SYS, 0);
	set_usb_pll();
	set_sys_pll();

	set_reset(RESETS_UART0, 0);

	set_uart0_pinmux();

	set_config(LED_PIN, PADS_CLEAR);
	set_pinfunc(LED_PIN, GPIO_FUNC_SIO);
	uart_init();

	SIO_BASE[0x38/4] = BIT(LED_PIN); // OE_SET

	while (1) {
		uart_puts("hello!\r\n");
		delay(500000);
		SIO_BASE[0x28/4] = BIT(LED_PIN); // OUT_XOR
	}
}

static inline void set_uart0_pinmux(void) {
	set_config(0, PADS_CLEAR); /* TX, PICO PIN 1 */
	set_config(1, PADS_CLEAR); /* RX, PICO PIN 2 */
	set_pinfunc(0, GPIO_FUNC_UART); /* TX, PICO PIN 1 */
	set_pinfunc(1, GPIO_FUNC_UART); /* RX, PICO PIN 2 */
}
