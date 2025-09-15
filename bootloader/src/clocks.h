#ifndef CLOCKS_H
#define CLOCKS_H

#include <stdint.h>
#include "rp2350-map.h"
#include "helpers.h"

#define XOSC_CTRL	(0)
#define XOSC_STATUS	(1)

#define XOSC_EN		((0xfabu) << (12))
#define XOSC_DIS	((0xd1eu) << (12))
#define XOSC_1_15MHz	((0xaa0u))
#define XOSC_STABLE	(31)
#define XOSC_ENABLED	(12)

#define CLOCKS_CTRL_ENABLED	28
#define CLOCKS_CTRL_ENABLE	11

#define CLOCKS_PERIF_CTRL	18
#define CLOCKS_CTRL_XOSC_SRC	4
#define CLOCKS_CTRL_SRC_SHIFT	5

#define PLL_CS_REG		0
#define PLL_CS_LOCK_SHIFT	31

#define PLL_PWR_REG		1
#define PLL_PWR_PD_SHIFT	0
#define PLL_PWR_POSTDIVPD_SHIFT	3
#define PLL_PWR_VCOPD_SHIFT	5

#define PLL_FBDIV_INT_REG	2

#define PLL_PRIM_REG		3
#define PLL_PRIM_PDIV1_SHIFT	16
#define PLL_PRIM_PDIV2_SHIFT	12

static inline void init_xosc(void) {
	XOSC_BASE[XOSC_CTRL] = XOSC_EN | XOSC_1_15MHz;
	loop_until_bit_is_set(XOSC_BASE[XOSC_STATUS], XOSC_STABLE);
}

static inline void set_perif_clock_xosc(void) {
	// Set XOSC as the source of the perif clock
	//CLOCKS_BASE[18] &= ~BIT(CLOCKS_CTRL_ENABLE);
	//loop_until_bit_is_clear(CLOCKS_BASE[18], CLOCKS_CTRL_ENABLED);
	//for (volatile int i = 0; i < 1000; i++)
	//	nop();
	CLOCKS_BASE[CLOCKS_PERIF_CTRL] = CLOCKS_CTRL_XOSC_SRC << CLOCKS_CTRL_SRC_SHIFT;
	CLOCKS_BASE[CLOCKS_PERIF_CTRL] |= 1 << CLOCKS_CTRL_ENABLE;
	//loop_until_bit_is_set(CLOCKS_BASE[18], CLOCKS_CTRL_ENABLED);
}

static inline void set_usb_pll(void) {
	PLL_USB_BASE[PLL_CS_REG] = 1; // REFDIV = 1
	PLL_USB_BASE[PLL_FBDIV_INT_REG] = 100; // 12MHz*4 = 1200MHz

	// Power up the PLL
	PLL_USB_BASE[PLL_PWR_REG] &= NBIT(PLL_PWR_PD_SHIFT) & NBIT(PLL_PWR_VCOPD_SHIFT);

	// Wait for PLL to lock
	loop_until_bit_is_set(PLL_USB_BASE[PLL_CS_REG], PLL_CS_LOCK_SHIFT);

	// 1200MHz / (5 * 5) = 48MHz
	PLL_USB_BASE[PLL_PRIM_REG] = (5 << PLL_PRIM_PDIV1_SHIFT) | (5 << PLL_PRIM_PDIV2_SHIFT);

	// Enable PLL output
	PLL_USB_BASE[PLL_PWR_REG] &= NBIT(PLL_PWR_POSTDIVPD_SHIFT);
}

static inline void set_sys_pll(void) {
	PLL_SYS_BASE[PLL_CS_REG] = 1; // REFDIV = 1
	PLL_SYS_BASE[PLL_FBDIV_INT_REG] = 125; // 12MHz*4 = 1200MHz

	// Power up the PLL
	PLL_SYS_BASE[PLL_PWR_REG] &= NBIT(PLL_PWR_PD_SHIFT) & NBIT(PLL_PWR_VCOPD_SHIFT);

	// Wait for PLL to lock
	loop_until_bit_is_set(PLL_SYS_BASE[PLL_CS_REG], PLL_CS_LOCK_SHIFT);

	// 1200MHz / (5 * 5) = 48MHz
	PLL_SYS_BASE[PLL_PRIM_REG] = (5 << PLL_PRIM_PDIV1_SHIFT) | (2 << PLL_PRIM_PDIV2_SHIFT);

	// Enable PLL output
	PLL_SYS_BASE[PLL_PWR_REG] &= NBIT(PLL_PWR_POSTDIVPD_SHIFT);
}

#endif // CLOCKS_H
