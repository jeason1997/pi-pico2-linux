#include <stdint.h>

#include "rp2350-map.h"
#include "helpers.h"
#include "gpio.h"
#include "pads.h"

#define LED_PIN			25

int main(void) {
	set_config(LED_PIN, 0x0);
	set_pinfunc(LED_PIN, GPIO_FUNC_SIO);

	SIO_BASE[0x38/4] = BIT(LED_PIN); // OE_SET

	while (1) {
		delay(500000);
		SIO_BASE[0x28/4] = BIT(LED_PIN); // OUT_XOR
	}
}
