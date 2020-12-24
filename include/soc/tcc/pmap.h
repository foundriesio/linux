// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SOC_TCC_PMAP_H
#define SOC_TCC_PMAP_H

#include <linux/types.h>

#define TCC_PMAP_NAME_LEN	(16U)

#define PMAP_FLAG_SECURED	((u32)1U << 1)
#define PMAP_FLAG_SHARED	((u32)1U << 2)
#define PMAP_FLAG_CMA_ALLOC	((u32)1U << 3)

#define pmap_is_secured(pmap)	(((pmap)->flags & PMAP_FLAG_SECURED) != 0U)
#define pmap_is_shared(pmap)	(((pmap)->flags & PMAP_FLAG_SHARED) != 0U)
#define pmap_is_cma_alloc(pmap)	(((pmap)->flags & PMAP_FLAG_CMA_ALLOC) != 0U)

struct pmap {
	char name[TCC_PMAP_NAME_LEN];
	u64 base;
	u64 size;
	u32 groups;
	u32 rc;
	u32 flags;
};


s32 pmap_check_region(u64 base, u64 size);
s32 pmap_get_info(const char *name, struct pmap *mem);
s32 pmap_release_info(const char *name);

#ifdef CONFIG_PMAP_TO_CMA
void *pmap_cma_remap(u64 base, u64 size);
void pmap_cma_unmap(void *virt, u64 size);
#else
#define pmap_cma_remap(base, size) NULL
#define pmap_cma_unmap(base, size)
#endif

#endif  /* SOC_TCC_PMAP_H */
