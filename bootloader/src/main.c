#include <stdint.h>

#define SIO_BASE			((volatile uint32_t *) 0xD0000000)
#define IO_BANK0_BASE			((volatile uint32_t *) 0x40028000)
#define PADS_BANK0_BASE			((volatile uint32_t *) 0x40038000)
#define GPIO_FUNC_SIO			5

#define LED_PIN				25

static inline void set_pinfunc(uint8_t pin, uint8_t func) {
	IO_BANK0_BASE[1 + (pin * 2)] = func;
}


static inline void set_config(uint8_t pin, uint8_t conf) {
	PADS_BANK0_BASE[1 + pin] = conf;
}

int main(void) {
	set_config(LED_PIN, 0x0);
	set_pinfunc(LED_PIN, GPIO_FUNC_SIO);

	SIO_BASE[0x38/4] = 1 << LED_PIN; // OE_SET
	SIO_BASE[0x28/4] = 1 << LED_PIN; // OUT_XOR

	while (1) {
		continue;
	}
}
