/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HEX_H
#define _LINUX_HEX_H

static const char hex_asc[] = "0123456789abcdef";
static const char hex_asc_upper[] = "0123456789ABCDEF";

#define hex_asc_lo(x)	hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)	hex_asc[((x) & 0xf0) >> 4]

#endif /* _LINUX_HEX_H */
