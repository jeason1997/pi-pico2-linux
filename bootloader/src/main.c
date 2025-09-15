#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "rp2350-map.h"
#include "helpers.h"
#include "gpio.h"
#include "pads.h"
#include "resets.h"
#include "uart.h"
#include "clocks.h"
#include "qmi.h"

#include "linux/vsprintf.h"
#include "byteswap.h"
#include "ctype.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

#define LED_PIN			25

//#if defined(SPARKFUN_PROMICRO_RP2350)

// For the pro micro rp2350
#define SFE_RP2350_XIP_CSI_PIN 19
//#endif

#define PSRAM_LOCATION (0x11000000U)

/* Linker variable for the end of binary image */
extern size_t *__data_load_start;

static size_t _psram_size = 0;

static inline void set_uart0_pinmux(void);
static int get_rom(void **data_addr, size_t* data_size);
static int wait_for_input(const char *msg);
static void hexdump(const void *data, size_t size);
static int test_executability(void* addr);
static int wait_for_input(const char *msg);
static size_t get_devicetree_size(const void *data);

int main(void) {
	int ret;
	uint32_t jump_ret, data;
	size_t data_size, kernel_offset;
	void *data_addr, *ram_addr = (void *)PSRAM_LOCATION;

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
		uart_puts(buffer);
		delay(500000);
		SIO_BASE[0x28/4] = BIT(LED_PIN); // OUT_XOR
	}

	//ret = psram_setup_and_test();
	if (ret) {
		goto exit;
	}

	ret = get_rom(&data_addr, &data_size);
	if (ret) {
		goto exit;
	}

	if (data_size > _psram_size) {
		printf("Data size 0x%04x is larger than PSRAM size 0x%04x\n",
			data_size, _psram_size);
		goto exit;
	}

	if(data_size < 8) {
		printf("Data size is 0\n");
		goto exit;
	}

	printf("\nRom dump:\n");
	hexdump(data_addr, min(data_size, 0x20));

	kernel_offset = get_devicetree_size(data_addr);
	if (kernel_offset) {
		printf("Kernel offset: 0x%08x\n", kernel_offset);
	} else {
		printf("No kernel + device tree found\n");
	}

	memcpy(ram_addr, data_addr + kernel_offset, data_size - kernel_offset);
	printf("\nRam dump:\n");
	hexdump(ram_addr, 0x20);

	if (kernel_offset) {
		typedef void (*image_entry_arg_t)(unsigned long hart, void *dtb);
		image_entry_arg_t image_entry = (image_entry_arg_t)ram_addr;

		printf("\nJumping to kernel at 0x%08x and DT at 0x%08x\n", ram_addr, data_addr);
		printf("If you are using USB serial, please connect over the hardware serial port.\n");
		image_entry(0, data_addr);
	}

	ret = wait_for_input("Press y to jump to PSRAM...\r");
	if (ret == 'y' || ret == 'Y') {
		data = (uint32_t)printf;
		jump_ret = ((uint32_t (*)(void *))ram_addr)((void *)data);
		printf("Jump to PSRAM returned 0x%08x\n", jump_ret);
	}

exit:
	wait_for_input("Press any key to reset.\r");
	puts("Resetting...");
	return 0;
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

static size_t get_devicetree_size(const void *data) {
	const uint32_t *data_ptr = (const uint32_t *)data;

	if (be32toh(data_ptr[0]) == 0xd00dfeed) {
		return be32toh(data_ptr[1]);
	}

	return 0;
}

static int test_executability(void* addr) {
	const uint16_t ret_inst = 0x8082;
	size_t addr_aligned = ((size_t)addr) & ~1;
	volatile uint16_t* addr_ptr = (volatile uint16_t*)addr_aligned;

	//__mem_fence_acquire();
	*addr_ptr = ret_inst;
	//__mem_fence_release();

	printf("Jumping to 0x%08x, aligned from 0x%08x\n", addr_aligned, (size_t)addr);
	printf("Function pointers: 0x%02x 0x%02x\n", ((uint8_t*)addr_ptr)[0], ((uint8_t*)addr_ptr)[1]);

	if (*addr_ptr != ret_inst) {
		printf("ERROR: Expected 0x%04x, got 0x%04x\n", ret_inst, *addr_ptr);
		return -1;
	}

	((void (*)(void))addr_aligned)();
	return 0;
}

static int psram_setup_and_test(void) {
	_psram_size = setup_psram(SFE_RP2350_XIP_CSI_PIN);

	if (!_psram_size) {
		printf("PSRAM setup failed\n");
		return -1;
	}

	printf("PSRAM setup complete. PSRAM size 0x%lX (%d)\n", _psram_size, _psram_size);

	return test_executability((size_t *)(PSRAM_LOCATION + _psram_size - 4));
}

static int get_rom(void **data_addr, size_t* data_size) {
	size_t *appended_data = __data_load_start;
	*data_addr = (void *)appended_data;
	*data_size = 1;

	return 0;
}

static void hexdump(const void *data, size_t size) {
	const uint8_t *data_ptr = (const uint8_t *)data;
	size_t i, b;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0) {
			printf("%08x  ", (uint32_t)data_ptr + i);
		}
		if (i % 8 == 0) {
			printf(" ");
		}
		printf("%02x ", data_ptr[i]);
		if (i % 16 == 15) {
			printf(" |");
			for (b = 0; b < 16; b++){
				if (isprint(data_ptr[i + b - 15])) {
					printf("%c", data_ptr[i + b - 15]);
				} else {
					printf(".");
				}
			}
			printf("|\n");
		}
	}
	printf("%08x\n", 16 + size - (size%16));
}
