/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_BITS_PER_LONG
#define __ASM_GENERIC_BITS_PER_LONG

#include <limits.h>

#if ULONG_MAX == UINT64_MAX
#define BITS_PER_LONG 64
#elif ULONG_MAX == UINT32_MAX
#define BITS_PER_LONG 32
#else
#error "Unsupported ULONG_MAX"
#endif

#define BITS_PER_LONG_LONG 64

#endif /* __ASM_GENERIC_BITS_PER_LONG */
