// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

/*
 * Fri Jul 13 2001 Crutcher Dunnavant <crutcher+kernel@datastacks.com>
 * - changed to provide snprintf and vsnprintf functions
 * So Feb  1 16:51:32 CET 2004 Juergen Quade <quade@hsnr.de>
 * - scnprintf and vscnprintf
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "ctype.h"

#include "extra.h"
#include "types.h"
#include "byteorder_generic.h"
#include "build_bug.h"
#include "hex.h"
#include "div64.h"

static inline int skip_atoi(const char **s)
{
	int i = 0;

	do {
		i = i*10 + *((*s)++) - '0';
	} while (isdigit(**s));

	return i;
}

/*
 * Decimal conversion is by far the most typical, and is used for
 * /proc and /sys data. This directly impacts e.g. top performance
 * with many processes running. We optimize it for speed by emitting
 * two characters at a time, using a 200 byte lookup table. This
 * roughly halves the number of multiplications compared to computing
 * the digits one at a time. Implementation strongly inspired by the
 * previous version, which in turn used ideas described at
 * <http://www.cs.uiowa.edu/~jones/bcd/divide.html> (with permission
 * from the author, Douglas W. Jones).
 *
 * It turns out there is precisely one 26 bit fixed-point
 * approximation a of 64/100 for which x/100 == (x * (u64)a) >> 32
 * holds for all x in [0, 10^8-1], namely a = 0x28f5c29. The actual
 * range happens to be somewhat larger (x <= 1073741898), but that's
 * irrelevant for our purpose.
 *
 * For dividing a number in the range [10^4, 10^6-1] by 100, we still
 * need a 32x32->64 bit multiply, so we simply use the same constant.
 *
 * For dividing a number in the range [100, 10^4-1] by 100, there are
 * several options. The simplest is (x * 0x147b) >> 19, which is valid
 * for all x <= 43698.
 */

static const u16 decpair[100] = {
#define _(x) (__force u16) cpu_to_le16(((x % 10) | ((x / 10) << 8)) + 0x3030)
	_( 0), _( 1), _( 2), _( 3), _( 4), _( 5), _( 6), _( 7), _( 8), _( 9),
	_(10), _(11), _(12), _(13), _(14), _(15), _(16), _(17), _(18), _(19),
	_(20), _(21), _(22), _(23), _(24), _(25), _(26), _(27), _(28), _(29),
	_(30), _(31), _(32), _(33), _(34), _(35), _(36), _(37), _(38), _(39),
	_(40), _(41), _(42), _(43), _(44), _(45), _(46), _(47), _(48), _(49),
	_(50), _(51), _(52), _(53), _(54), _(55), _(56), _(57), _(58), _(59),
	_(60), _(61), _(62), _(63), _(64), _(65), _(66), _(67), _(68), _(69),
	_(70), _(71), _(72), _(73), _(74), _(75), _(76), _(77), _(78), _(79),
	_(80), _(81), _(82), _(83), _(84), _(85), _(86), _(87), _(88), _(89),
	_(90), _(91), _(92), _(93), _(94), _(95), _(96), _(97), _(98), _(99),
#undef _
};

/*
 * This will print a single '0' even if r == 0, since we would
 * immediately jump to out_r where two 0s would be written but only
 * one of them accounted for in buf. This is needed by ip4_string
 * below. All other callers pass a non-zero value of r.
*/
static
char *put_dec_trunc8(char *buf, unsigned r)
{
	unsigned q;

	/* 1 <= r < 10^8 */
	if (r < 100)
		goto out_r;

	/* 100 <= r < 10^8 */
	q = (r * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;

	/* 1 <= q < 10^6 */
	if (q < 100)
		goto out_q;

	/*  100 <= q < 10^6 */
	r = (q * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[q - 100*r];
	buf += 2;

	/* 1 <= r < 10^4 */
	if (r < 100)
		goto out_r;

	/* 100 <= r < 10^4 */
	q = (r * 0x147b) >> 19;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;
out_q:
	/* 1 <= q < 100 */
	r = q;
out_r:
	/* 1 <= r < 100 */
	*((u16 *)buf) = decpair[r];
	buf += r < 10 ? 1 : 2;
	return buf;
}

#if BITS_PER_LONG == 64 && BITS_PER_LONG_LONG == 64
static
char *put_dec_full8(char *buf, unsigned r)
{
	unsigned q;

	/* 0 <= r < 10^8 */
	q = (r * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;

	/* 0 <= q < 10^6 */
	r = (q * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[q - 100*r];
	buf += 2;

	/* 0 <= r < 10^4 */
	q = (r * 0x147b) >> 19;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;

	/* 0 <= q < 100 */
	*((u16 *)buf) = decpair[q];
	buf += 2;
	return buf;
}

static
char *put_dec(char *buf, unsigned long long n)
{
	if (n >= 100*1000*1000)
		buf = put_dec_full8(buf, do_div(n, 100*1000*1000));
	/* 1 <= n <= 1.6e11 */
	if (n >= 100*1000*1000)
		buf = put_dec_full8(buf, do_div(n, 100*1000*1000));
	/* 1 <= n < 1e8 */
	return put_dec_trunc8(buf, n);
}

#elif BITS_PER_LONG == 32 && BITS_PER_LONG_LONG == 64

static void
put_dec_full4(char *buf, unsigned r)
{
	unsigned q;

	/* 0 <= r < 10^4 */
	q = (r * 0x147b) >> 19;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;
	/* 0 <= q < 100 */
	*((u16 *)buf) = decpair[q];
}

/*
 * Call put_dec_full4 on x % 10000, return x / 10000.
 * The approximation x/10000 == (x * 0x346DC5D7) >> 43
 * holds for all x < 1,128,869,999.  The largest value this
 * helper will ever be asked to convert is 1,125,520,955.
 * (second call in the put_dec code, assuming n is all-ones).
 */
static
unsigned put_dec_helper4(char *buf, unsigned x)
{
        uint32_t q = (x * (uint64_t)0x346DC5D7) >> 43;

        put_dec_full4(buf, x - q * 10000);
        return q;
}

/* Based on code by Douglas W. Jones found at
 * <http://www.cs.uiowa.edu/~jones/bcd/decimal.html#sixtyfour>
 * (with permission from the author).
 * Performs no 64-bit division and hence should be fast on 32-bit machines.
 */
static
char *put_dec(char *buf, unsigned long long n)
{
	uint32_t d3, d2, d1, q, h;

	if (n < 100*1000*1000)
		return put_dec_trunc8(buf, n);

	d1  = ((uint32_t)n >> 16); /* implicit "& 0xffff" */
	h   = (n >> 32);
	d2  = (h      ) & 0xffff;
	d3  = (h >> 16); /* implicit "& 0xffff" */

	/* n = 2^48 d3 + 2^32 d2 + 2^16 d1 + d0
	     = 281_4749_7671_0656 d3 + 42_9496_7296 d2 + 6_5536 d1 + d0 */
	q   = 656 * d3 + 7296 * d2 + 5536 * d1 + ((uint32_t)n & 0xffff);
	q = put_dec_helper4(buf, q);

	q += 7671 * d3 + 9496 * d2 + 6 * d1;
	q = put_dec_helper4(buf+4, q);

	q += 4749 * d3 + 42 * d2;
	q = put_dec_helper4(buf+8, q);

	q += 281 * d3;
	buf += 12;
	if (q)
		buf = put_dec_trunc8(buf, q);
	else while (buf[-1] == '0')
		--buf;

	return buf;
}

#endif

#define SIGN	1		/* unsigned/signed */
#define LEFT	2		/* left justified */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define ZEROPAD	16		/* pad with zero, must be 16 == '0' - ' ' */
#define SMALL	32		/* use lowercase in hex (must be 32 == 0x20) */
#define SPECIAL	64		/* prefix hex with "0x", octal with "0" */

static_assert(ZEROPAD == ('0' - ' '));
static_assert(SMALL == ('a' ^ 'A'));

enum format_state {
	FORMAT_STATE_NONE, /* Just a string part */
	FORMAT_STATE_NUM,
	FORMAT_STATE_WIDTH,
	FORMAT_STATE_PRECISION,
	FORMAT_STATE_CHAR,
	FORMAT_STATE_STR,
	FORMAT_STATE_PTR,
	FORMAT_STATE_PERCENT_CHAR,
	FORMAT_STATE_INVALID,
};

struct printf_spec {
	unsigned char	flags;		/* flags to number() */
	unsigned char	base;		/* number base, 8, 10 or 16 only */
	short		precision;	/* # of digits/chars */
	int		field_width;	/* width of output field */
} __packed;
static_assert(sizeof(struct printf_spec) == 8);

#define FIELD_WIDTH_MAX ((1 << 23) - 1)
#define PRECISION_MAX ((1 << 15) - 1)

static
char *number(char *buf, char *end, unsigned long long num,
	     struct printf_spec spec)
{
	/* put_dec requires 2-byte alignment of the buffer. */
	char tmp[3 * sizeof(num)] __aligned(2);
	char sign;
	char locase;
	int need_pfx = ((spec.flags & SPECIAL) && spec.base != 10);
	int i;
	bool is_zero = num == 0LL;
	int field_width = spec.field_width;
	int precision = spec.precision;

	/* locase = 0 or 0x20. ORing digits or letters with 'locase'
	 * produces same digits or (maybe lowercased) letters */
	locase = (spec.flags & SMALL);
	if (spec.flags & LEFT)
		spec.flags &= ~ZEROPAD;
	sign = 0;
	if (spec.flags & SIGN) {
		if ((signed long long)num < 0) {
			sign = '-';
			num = -(signed long long)num;
			field_width--;
		} else if (spec.flags & PLUS) {
			sign = '+';
			field_width--;
		} else if (spec.flags & SPACE) {
			sign = ' ';
			field_width--;
		}
	}
	if (need_pfx) {
		if (spec.base == 16)
			field_width -= 2;
		else if (!is_zero)
			field_width--;
	}

	/* generate full string in tmp[], in reverse order */
	i = 0;
	if (num < spec.base)
		tmp[i++] = hex_asc_upper[num] | locase;
	else if (spec.base != 10) { /* 8 or 16 */
		int mask = spec.base - 1;
		int shift = 3;

		if (spec.base == 16)
			shift = 4;
		do {
			tmp[i++] = (hex_asc_upper[((unsigned char)num) & mask] | locase);
			num >>= shift;
		} while (num);
	} else { /* base 10 */
		i = put_dec(tmp, num) - tmp;
	}

	/* printing 100 using %2d gives "100", not "00" */
	if (i > precision)
		precision = i;
	/* leading space padding */
	field_width -= precision;
	if (!(spec.flags & (ZEROPAD | LEFT))) {
		while (--field_width >= 0) {
			if (buf < end)
				*buf = ' ';
			++buf;
		}
	}
	/* sign */
	if (sign) {
		if (buf < end)
			*buf = sign;
		++buf;
	}
	/* "0x" / "0" prefix */
	if (need_pfx) {
		if (spec.base == 16 || !is_zero) {
			if (buf < end)
				*buf = '0';
			++buf;
		}
		if (spec.base == 16) {
			if (buf < end)
				*buf = ('X' | locase);
			++buf;
		}
	}
	/* zero or space padding */
	if (!(spec.flags & LEFT)) {
		char c = ' ' + (spec.flags & ZEROPAD);

		while (--field_width >= 0) {
			if (buf < end)
				*buf = c;
			++buf;
		}
	}
	/* hmm even more zero padding? */
	while (i <= --precision) {
		if (buf < end)
			*buf = '0';
		++buf;
	}
	/* actual digits of result */
	while (--i >= 0) {
		if (buf < end)
			*buf = tmp[i];
		++buf;
	}
	/* trailing space padding */
	while (--field_width >= 0) {
		if (buf < end)
			*buf = ' ';
		++buf;
	}

	return buf;
}

static void move_right(char *buf, char *end, unsigned len, unsigned spaces)
{
	size_t size;
	if (buf >= end)	/* nowhere to put anything */
		return;
	size = end - buf;
	if (size <= spaces) {
		memset(buf, ' ', size);
		return;
	}
	if (len) {
		if (len > size - spaces)
			len = size - spaces;
		memmove(buf + spaces, buf, len);
	}
	memset(buf, ' ', spaces);
}

/*
 * Handle field width padding for a string.
 * @buf: current buffer position
 * @n: length of string
 * @end: end of output buffer
 * @spec: for field width and flags
 * Returns: new buffer position after padding.
 */
static
char *widen_string(char *buf, int n, char *end, struct printf_spec spec)
{
	unsigned spaces;

	if (likely(n >= spec.field_width))
		return buf;
	/* we want to pad the sucker */
	spaces = spec.field_width - n;
	if (!(spec.flags & LEFT)) {
		move_right(buf - n, end, n, spaces);
		return buf + spaces;
	}
	while (spaces--) {
		if (buf < end)
			*buf = ' ';
		++buf;
	}
	return buf;
}

/* Handle string from a well known address. */
static char *string_nocheck(char *buf, char *end, const char *s,
			    struct printf_spec spec)
{
	int len = 0;
	int lim = spec.precision;

	while (lim--) {
		char c = *s++;
		if (!c)
			break;
		if (buf < end)
			*buf = c;
		++buf;
		++len;
	}
	return widen_string(buf, len, end, spec);
}

/* Be careful: error messages must fit into the given buffer. */
static char *error_string(char *buf, char *end, const char *s,
			  struct printf_spec spec)
{
	/*
	 * Hard limit to avoid a completely insane messages. It actually
	 * works pretty well because most error messages are in
	 * the many pointer format modifiers.
	 */
	if (spec.precision == -1)
		spec.precision = 2 * sizeof(void *);

	return string_nocheck(buf, end, s, spec);
}

/*
 * Do not call any complex external code here. Nested printk()/vsprintf()
 * might cause infinite loops. Failures might break printk() and would
 * be hard to debug.
 */
static const char *check_pointer_msg(const void *ptr)
{
	if (!ptr)
		return "(null)";

	return NULL;
}

static int check_pointer(char **buf, char *end, const void *ptr,
			 struct printf_spec spec)
{
	const char *err_msg;

	err_msg = check_pointer_msg(ptr);
	if (err_msg) {
		*buf = error_string(*buf, end, err_msg, spec);
		return -EFAULT;
	}

	return 0;
}

static
char *string(char *buf, char *end, const char *s,
	     struct printf_spec spec)
{
	if (check_pointer(&buf, end, s, spec))
		return buf;

	return string_nocheck(buf, end, s, spec);
}

static char *pointer_string(char *buf, char *end,
			    const void *ptr,
			    struct printf_spec spec)
{
	spec.base = 16;
	spec.flags |= SMALL;
	if (spec.field_width == -1) {
		spec.field_width = 2 * sizeof(ptr);
		spec.flags |= ZEROPAD;
	}

	return number(buf, end, (unsigned long int)ptr, spec);
}

static char *default_pointer(char *buf, char *end, const void *ptr,
	struct printf_spec spec)
{

	return pointer_string(buf, end, ptr, spec);
}

static
char *hex_string(char *buf, char *end, u8 *addr, struct printf_spec spec,
		 const char *fmt)
{
	int i, len = 1;		/* if we pass '%ph[CDN]', field width remains
				   negative value, fallback to the default */
	char separator;

	if (spec.field_width == 0)
		/* nothing to print */
		return buf;

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	switch (fmt[1]) {
	case 'C':
		separator = ':';
		break;
	case 'D':
		separator = '-';
		break;
	case 'N':
		separator = 0;
		break;
	default:
		separator = ' ';
		break;
	}

	if (spec.field_width > 0)
		len = min_t(int, spec.field_width, 64);

	for (i = 0; i < len; ++i) {
		if (buf < end)
			*buf = hex_asc_hi(addr[i]);
		++buf;
		if (buf < end)
			*buf = hex_asc_lo(addr[i]);
		++buf;

		if (separator && i != len - 1) {
			if (buf < end)
				*buf = separator;
			++buf;
		}
	}

	return buf;
}

/*
 * Show a '%p' thing.  A kernel extension is that the '%p' is followed
 * by an extra set of alphanumeric characters that are extended format
 * specifiers.
 *
 * Please update scripts/checkpatch.pl when adding/removing conversion
 * characters.  (Search for "check for vsprintf extension").
 *
 * Right now we handle:
 *
 * - 'S' For symbolic direct pointers (or function descriptors) with offset
 * - 's' For symbolic direct pointers (or function descriptors) without offset
 * - '[Ss]R' as above with __builtin_extract_return_addr() translation
 * - 'S[R]b' as above with module build ID (for use in backtraces)
 * - '[Ff]' %pf and %pF were obsoleted and later removed in favor of
 *	    %ps and %pS. Be careful when re-using these specifiers.
 * - 'B' For backtraced symbolic direct pointers with offset
 * - 'Bb' as above with module build ID (for use in backtraces)
 * - 'R' For decoded struct resource, e.g., [mem 0x0-0x1f 64bit pref]
 * - 'r' For raw struct resource, e.g., [mem 0x0-0x1f flags 0x201]
 * - 'ra' For struct ranges, e.g., [range 0x0000000000000000 - 0x00000000000000ff]
 * - 'b[l]' For a bitmap, the number of bits is determined by the field
 *       width which must be explicitly specified either as part of the
 *       format string '%32b[l]' or through '%*b[l]', [l] selects
 *       range-list format instead of hex format
 * - 'M' For a 6-byte MAC address, it prints the address in the
 *       usual colon-separated hex notation
 * - 'm' For a 6-byte MAC address, it prints the hex address without colons
 * - 'MF' For a 6-byte MAC FDDI address, it prints the address
 *       with a dash-separated hex notation
 * - '[mM]R' For a 6-byte MAC address, Reverse order (Bluetooth)
 * - 'I' [46] for IPv4/IPv6 addresses printed in the usual way
 *       IPv4 uses dot-separated decimal without leading 0's (1.2.3.4)
 *       IPv6 uses colon separated network-order 16 bit hex with leading 0's
 *       [S][pfs]
 *       Generic IPv4/IPv6 address (struct sockaddr *) that falls back to
 *       [4] or [6] and is able to print port [p], flowinfo [f], scope [s]
 * - 'i' [46] for 'raw' IPv4/IPv6 addresses
 *       IPv6 omits the colons (01020304...0f)
 *       IPv4 uses dot-separated decimal with leading 0's (010.123.045.006)
 *       [S][pfs]
 *       Generic IPv4/IPv6 address (struct sockaddr *) that falls back to
 *       [4] or [6] and is able to print port [p], flowinfo [f], scope [s]
 * - '[Ii][4S][hnbl]' IPv4 addresses in host, network, big or little endian order
 * - 'I[6S]c' for IPv6 addresses printed as specified by
 *       https://tools.ietf.org/html/rfc5952
 * - 'E[achnops]' For an escaped buffer, where rules are defined by combination
 *                of the following flags (see string_escape_mem() for the
 *                details):
 *                  a - ESCAPE_ANY
 *                  c - ESCAPE_SPECIAL
 *                  h - ESCAPE_HEX
 *                  n - ESCAPE_NULL
 *                  o - ESCAPE_OCTAL
 *                  p - ESCAPE_NP
 *                  s - ESCAPE_SPACE
 *                By default ESCAPE_ANY_NP is used.
 * - 'U' For a 16 byte UUID/GUID, it prints the UUID/GUID in the form
 *       "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 *       Options for %pU are:
 *         b big endian lower case hex (default)
 *         B big endian UPPER case hex
 *         l little endian lower case hex
 *         L little endian UPPER case hex
 *           big endian output byte order is:
 *             [0][1][2][3]-[4][5]-[6][7]-[8][9]-[10][11][12][13][14][15]
 *           little endian output byte order is:
 *             [3][2][1][0]-[5][4]-[7][6]-[8][9]-[10][11][12][13][14][15]
 * - 'V' For a struct va_format which contains a format string * and va_list *,
 *       call vsnprintf(->format, *->va_list).
 *       Implements a "recursive vsnprintf".
 *       Do not use this feature without some mechanism to verify the
 *       correctness of the format string and va_list arguments.
 * - 'K' For a kernel pointer that should be hidden from unprivileged users.
 *       Use only for procfs, sysfs and similar files, not printk(); please
 *       read the documentation (path below) first.
 * - 'NF' For a netdev_features_t
 * - '4cc' V4L2 or DRM FourCC code, with endianness and raw numerical value.
 * - 'h[CDN]' For a variable-length buffer, it prints it as a hex string with
 *            a certain separator (' ' by default):
 *              C colon
 *              D dash
 *              N no separator
 *            The maximum supported length is 64 bytes of the input. Consider
 *            to use print_hex_dump() for the larger input.
 * - 'a[pd]' For address types [p] phys_addr_t, [d] dma_addr_t and derivatives
 *           (default assumed to be phys_addr_t, passed by reference)
 * - 'd[234]' For a dentry name (optionally 2-4 last components)
 * - 'D[234]' Same as 'd' but for a struct file
 * - 'g' For block_device name (gendisk + partition number)
 * - 't[RT][dt][r][s]' For time and date as represented by:
 *      R    struct rtc_time
 *      T    time64_t
 * - 'C' For a clock, it prints the name (Common Clock Framework) or address
 *       (legacy clock framework) of the clock
 * - 'Cn' For a clock, it prints the name (Common Clock Framework) or address
 *        (legacy clock framework) of the clock
 * - 'G' For flags to be printed as a collection of symbolic strings that would
 *       construct the specific value. Supported flags given by option:
 *       p page flags (see struct page) given as pointer to unsigned long
 *       g gfp flags (GFP_* and __GFP_*) given as pointer to gfp_t
 *       v vma flags (VM_*) given as pointer to unsigned long
 * - 'OF[fnpPcCF]'  For a device tree object
 *                  Without any optional arguments prints the full_name
 *                  f device node full_name
 *                  n device node name
 *                  p device node phandle
 *                  P device node path spec (name + @unit)
 *                  F device node flags
 *                  c major compatible string
 *                  C full compatible string
 * - 'fw[fP]'	For a firmware node (struct fwnode_handle) pointer
 *		Without an option prints the full name of the node
 *		f full name
 *		P node name, including a possible unit address
 * - 'x' For printing the address unmodified. Equivalent to "%lx".
 *       Please read the documentation (path below) before using!
 * - '[ku]s' For a BPF/tracing related format specifier, e.g. used out of
 *           bpf_trace_printk() where [ku] prefix specifies either kernel (k)
 *           or user (u) memory to probe, and:
 *              s a string, equivalent to "%s" on direct vsnprintf() use
 *
 * ** When making changes please also update:
 *	Documentation/core-api/printk-formats.rst
 *
 * Note: The default behaviour (unadorned %p) is to hash the address,
 * rendering it useful as a unique identifier.
 *
 * There is also a '%pA' format specifier, but it is only intended to be used
 * from Rust code to format core::fmt::Arguments. Do *not* use it from C.
 * See rust/kernel/print.rs for details.
 */
static
char *pointer(const char *fmt, char *buf, char *end, void *ptr,
	      struct printf_spec spec)
{
	switch (*fmt) {
	case 'h':
		return hex_string(buf, end, ptr, spec, fmt);
	case 'x':
		return pointer_string(buf, end, ptr, spec);
	default:
		return default_pointer(buf, end, ptr, spec);
	}
}

struct fmt {
	const char *str;
	unsigned char state;	// enum format_state
	unsigned char size;	// size of numbers
};

#define SPEC_CHAR(x, flag) [(x)-32] = flag
static unsigned char spec_flag(unsigned char c)
{
	static const unsigned char spec_flag_array[] = {
		SPEC_CHAR(' ', SPACE),
		SPEC_CHAR('#', SPECIAL),
		SPEC_CHAR('+', PLUS),
		SPEC_CHAR('-', LEFT),
		SPEC_CHAR('0', ZEROPAD),
	};
	c -= 32;
	return (c < sizeof(spec_flag_array)) ? spec_flag_array[c] : 0;
}

/*
 * Helper function to decode printf style format.
 * Each call decode a token from the format and return the
 * number of characters read (or likely the delta where it wants
 * to go on the next call).
 * The decoded token is returned through the parameters
 *
 * 'h', 'l', or 'L' for integer fields
 * 'z' support added 23/7/1999 S.H.
 * 'z' changed to 'Z' --davidm 1/25/99
 * 'Z' changed to 'z' --adobriyan 2017-01-25
 * 't' added for ptrdiff_t
 *
 * @fmt: the format string
 * @type of the token returned
 * @flags: various flags such as +, -, # tokens..
 * @field_width: overwritten width
 * @base: base of the number (octal, hex, ...)
 * @precision: precision of a number
 * @qualifier: qualifier of a number (long, size_t, ...)
 */
static
struct fmt format_decode(struct fmt fmt, struct printf_spec *spec)
{
	const char *start = fmt.str;
	char flag;

	/* we finished early by reading the field width */
	if (unlikely(fmt.state == FORMAT_STATE_WIDTH)) {
		if (spec->field_width < 0) {
			spec->field_width = -spec->field_width;
			spec->flags |= LEFT;
		}
		fmt.state = FORMAT_STATE_NONE;
		goto precision;
	}

	/* we finished early by reading the precision */
	if (unlikely(fmt.state == FORMAT_STATE_PRECISION)) {
		if (spec->precision < 0)
			spec->precision = 0;

		fmt.state = FORMAT_STATE_NONE;
		goto qualifier;
	}

	/* By default */
	fmt.state = FORMAT_STATE_NONE;

	for (; *fmt.str ; fmt.str++) {
		if (*fmt.str == '%')
			break;
	}

	/* Return the current non-format string */
	if (fmt.str != start || !*fmt.str)
		return fmt;

	/* Process flags. This also skips the first '%' */
	spec->flags = 0;
	do {
		/* this also skips first '%' */
		flag = spec_flag(*++fmt.str);
		spec->flags |= flag;
	} while (flag);

	/* get field width */
	spec->field_width = -1;

	if (isdigit(*fmt.str))
		spec->field_width = skip_atoi(&fmt.str);
	else if (unlikely(*fmt.str == '*')) {
		/* it's the next argument */
		fmt.state = FORMAT_STATE_WIDTH;
		fmt.str++;
		return fmt;
	}

precision:
	/* get the precision */
	spec->precision = -1;
	if (unlikely(*fmt.str == '.')) {
		fmt.str++;
		if (isdigit(*fmt.str)) {
			spec->precision = skip_atoi(&fmt.str);
			if (spec->precision < 0)
				spec->precision = 0;
		} else if (*fmt.str == '*') {
			/* it's the next argument */
			fmt.state = FORMAT_STATE_PRECISION;
			fmt.str++;
			return fmt;
		}
	}

qualifier:
	/* Set up default numeric format */
	spec->base = 10;
	fmt.state = FORMAT_STATE_NUM;
	fmt.size = sizeof(int);
	static const struct format_state {
		unsigned char state;
		unsigned char size;
		unsigned char flags_or_double_size;
		unsigned char base;
	} lookup_state[256] = {
		// Length
		['l'] = { 0, sizeof(long), sizeof(long long) },
		['L'] = { 0, sizeof(long long) },
		['h'] = { 0, sizeof(short), sizeof(char) },
		['H'] = { 0, sizeof(char) },	// Questionable historical
		['z'] = { 0, sizeof(size_t) },
		['t'] = { 0, sizeof(ptrdiff_t) },

		// Non-numeric formats
		['c'] = { FORMAT_STATE_CHAR },
		['s'] = { FORMAT_STATE_STR },
		['p'] = { FORMAT_STATE_PTR },
		['%'] = { FORMAT_STATE_PERCENT_CHAR },

		// Numerics
		['o'] = { FORMAT_STATE_NUM, 0, 0, 8 },
		['x'] = { FORMAT_STATE_NUM, 0, SMALL, 16 },
		['X'] = { FORMAT_STATE_NUM, 0, 0, 16 },
		['d'] = { FORMAT_STATE_NUM, 0, SIGN, 10 },
		['i'] = { FORMAT_STATE_NUM, 0, SIGN, 10 },
		['u'] = { FORMAT_STATE_NUM, 0, 0, 10, },

		/*
		 * Since %n poses a greater security risk than
		 * utility, treat it as any other invalid or
		 * unsupported format specifier.
		 */
	};

	const struct format_state *p = lookup_state + (u8)*fmt.str;
	if (p->size) {
		fmt.size = p->size;
		if (p->flags_or_double_size && fmt.str[0] == fmt.str[1]) {
			fmt.size = p->flags_or_double_size;
			fmt.str++;
		}
		fmt.str++;
		p = lookup_state + *fmt.str;
	}
	if (p->state) {
		if (p->base)
			spec->base = p->base;
		spec->flags |= p->flags_or_double_size;
		fmt.state = p->state;
		fmt.str++;
		return fmt;
	}

	WARN_ONCE(1, "Please remove unsupported %%%c in format string\n", *fmt.str);
	fmt.state = FORMAT_STATE_INVALID;
	return fmt;
}

static void
set_field_width(struct printf_spec *spec, int width)
{
	spec->field_width = width;
	if (WARN_ONCE(spec->field_width != width, "field width %d too large", width)) {
		spec->field_width = clamp(width, -FIELD_WIDTH_MAX, FIELD_WIDTH_MAX);
	}
}

static void
set_precision(struct printf_spec *spec, int prec)
{
	spec->precision = prec;
	if (WARN_ONCE(spec->precision != prec, "precision %d too large", prec)) {
		spec->precision = clamp(prec, 0, PRECISION_MAX);
	}
}

/*
 * Turn a 1/2/4-byte value into a 64-bit one for printing: truncate
 * as necessary and deal with signedness.
 *
 * 'size' is the size of the value in bytes.
 */
static unsigned long long convert_num_spec(unsigned int val, int size, struct printf_spec spec)
{
	unsigned int shift = 32 - size*8;

	val <<= shift;
	if (!(spec.flags & SIGN))
		return val >> shift;
	return (int)val >> shift;
}

/**
 * vsnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt_str: The format string to use
 * @args: Arguments for the format string
 *
 * This function generally follows C99 vsnprintf, but has some
 * extensions and a few limitations:
 *
 *  - ``%n`` is unsupported
 *  - ``%p*`` is handled by pointer()
 *
 * See pointer() or Documentation/core-api/printk-formats.rst for more
 * extensive description.
 *
 * **Please update the documentation in both places when making changes**
 *
 * The return value is the number of characters which would
 * be generated for the given input, excluding the trailing
 * '\0', as per ISO C99. If you want to have the exact
 * number of characters written into @buf as return value
 * (not including the trailing '\0'), use vscnprintf(). If the
 * return is greater than or equal to @size, the resulting
 * string is truncated.
 *
 * If you're not already dealing with a va_list consider using snprintf().
 */
int vsnprintf(char *buf, size_t size, const char *fmt_str, va_list args)
{
	char *str, *end;
	struct printf_spec spec = {0};
	struct fmt fmt = {
		.str = fmt_str,
		.state = FORMAT_STATE_NONE,
	};

	/* Reject out-of-range values early.  Large positive sizes are
	   used for unknown buffer sizes. */
	if (WARN_ON_ONCE(size > INT_MAX))
		return 0;

	str = buf;
	end = buf + size;

	/* Make sure end is always >= buf */
	if (end < buf) {
		end = ((void *)-1);
		size = end - buf;
	}

	while (*fmt.str) {
		const char *old_fmt = fmt.str;

		fmt = format_decode(fmt, &spec);

		switch (fmt.state) {
		case FORMAT_STATE_NONE: {
			int read = fmt.str - old_fmt;
			if (str < end) {
				int copy = read;
				if (copy > end - str)
					copy = end - str;
				memcpy(str, old_fmt, copy);
			}
			str += read;
			continue;
		}

		case FORMAT_STATE_NUM: {
			unsigned long long num;
			if (fmt.size <= sizeof(int))
				num = convert_num_spec(va_arg(args, int), fmt.size, spec);
			else
				num = va_arg(args, long long);
			str = number(str, end, num, spec);
			continue;
		}

		case FORMAT_STATE_WIDTH:
			set_field_width(&spec, va_arg(args, int));
			continue;

		case FORMAT_STATE_PRECISION:
			set_precision(&spec, va_arg(args, int));
			continue;

		case FORMAT_STATE_CHAR: {
			char c;

			if (!(spec.flags & LEFT)) {
				while (--spec.field_width > 0) {
					if (str < end)
						*str = ' ';
					++str;

				}
			}
			c = (unsigned char) va_arg(args, int);
			if (str < end)
				*str = c;
			++str;
			while (--spec.field_width > 0) {
				if (str < end)
					*str = ' ';
				++str;
			}
			continue;
		}

		case FORMAT_STATE_STR:
			str = string(str, end, va_arg(args, char *), spec);
			continue;

		case FORMAT_STATE_PTR:
			str = pointer(fmt.str, str, end, va_arg(args, void *),
				      spec);
			while (isalnum(*fmt.str))
				fmt.str++;
			continue;

		case FORMAT_STATE_PERCENT_CHAR:
			if (str < end)
				*str = '%';
			++str;
			continue;

		default:
			/*
			 * Presumably the arguments passed gcc's type
			 * checking, but there is no safe or sane way
			 * for us to continue parsing the format and
			 * fetching from the va_list; the remaining
			 * specifiers and arguments would be out of
			 * sync.
			 */
			goto out;
		}
	}

out:
	if (size > 0) {
		if (str < end)
			*str = '\0';
		else
			end[-1] = '\0';
	}

	/* the trailing null byte doesn't count towards the total */
	return str-buf;

}

/**
 * vscnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The return value is the number of characters which have been written into
 * the @buf not including the trailing '\0'. If @size is == 0 the function
 * returns 0.
 *
 * If you're not already dealing with a va_list consider using scnprintf().
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int i;

	if (unlikely(!size))
		return 0;

	i = vsnprintf(buf, size, fmt, args);

	if (likely(i < size))
		return i;

	return size - 1;
}

/**
 * snprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters which would be
 * generated for the given input, excluding the trailing null,
 * as per ISO C99.  If the return is greater than or equal to
 * @size, the resulting string is truncated.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

/**
 * scnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is == 0 the function returns 0.
 */

int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

/**
 * vsprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf. Use vsnprintf() or vscnprintf() in order to avoid
 * buffer overflows.
 *
 * If you're not already dealing with a va_list consider using sprintf().
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
	return vsnprintf(buf, INT_MAX, fmt, args);
}

/**
 * sprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf. Use snprintf() or scnprintf() in order to avoid
 * buffer overflows.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, INT_MAX, fmt, args);
	va_end(args);

	return i;
}
