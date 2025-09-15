#include <stdint.h>
#include <stddef.h>

#include "rp2350-map.h"
#include "helpers.h"
#include "gpio.h"
#include "pads.h"
#include "resets.h"
#include "uart.h"
#include "clocks.h"

#define LED_PIN			25

/* Linker variable for the end of binary image */
extern size_t *__data_load_start;
static inline void set_uart0_pinmux(void);
static int get_rom(void **data_addr, size_t* data_size);
static int wait_for_input(const char *msg);

int main(void) {
	xosc_init();
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

	void *a;
	size_t b;
	get_rom(&a, &b);

	uart_putc((char)a + '0');
	uart_puts("\n\n");
	delay(500000);
	while (1) {
		char buffer[100];
		sprintf(buffer, "Hello, %s! The answer is %d.\n", "World", 42);
		puts(buffer);
		uart_puts(500000);
		SIO_BASE[0x28/4] = BIT(LED_PIN); // OUT_XOR
	}
}

static inline void set_uart0_pinmux(void) {
	set_config(0, PADS_CLEAR); /* TX, PICO PIN 1 */
	set_config(1, PADS_CLEAR); /* RX, PICO PIN 2 */
	set_pinfunc(0, GPIO_FUNC_UART); /* TX, PICO PIN 1 */
	set_pinfunc(1, GPIO_FUNC_UART); /* RX, PICO PIN 2 */
}

static int wait_for_input(const char *msg) {
	int ret;

	while (1) {
		uart_puts(msg);
		ret = uart_getc();
		if (ret != -1) {
			uart_puts("\n");
			return ret;
		}
		delay(500000);
	}
}

static int get_rom(void **data_addr, size_t* data_size) {
	size_t *appended_data = __data_load_start;
	*data_addr = (void *)appended_data;
	*data_size = 1;

	return 0;
}
