/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __PPC_PMEM_H__
#define __PPC_PMEM_H__

#include <linux/string.h>

#ifdef CONFIG_ARCH_HAS_PMEM_API
static inline void arch_memcpy_to_pmem(void *dst, const void *src, size_t n)
{
	memcpy_flushcache(dst, src, n);
}
#endif /* CONFIG_ARCH_HAS_PMEM_API */
#endif
