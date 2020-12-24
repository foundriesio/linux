// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_PMAP_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <soc/tcc/pmap.h>

#ifdef CONFIG_PROC_FS
#include <linux/list_sort.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_OPTEE
#include <linux/tee_drv.h>
#include <linux/time.h>
#endif

#ifdef CONFIG_PMAP_TO_CMA
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

struct device *pmap_device;
#endif

#define PMAP_FREED		(~((u64)0U))
#define PMAP_DATA_NA		(~((u32)0U))

#define MAX_PMAPS		(64U)
#define MAX_PMAP_GROUPS		(4U)

#ifdef CONFIG_PROC_FS
#define PROC_PMAP_CMD_GET	(0x501)
#define PROC_PMAP_CMD_RELEASE	(0x502)

struct user_pmap {
	char name[TCC_PMAP_NAME_LEN];
	// FIXME: Update the type to u64 with user-level library.
	u32 base;
	u32 size;
};
#endif

static bool pmap_list_sorted;
static u64 pmap_total_size;

struct pmap_entry {
	struct pmap info;
	struct pmap_entry *parent;
	struct list_head list;
};

static struct pmap_entry pmap_table[MAX_PMAPS];
static struct pmap_entry secure_area_table[MAX_PMAP_GROUPS];

static LIST_HEAD(pmap_list_head);

int __init early_init_dt_alloc_reserved_memory_arch(phys_addr_t size,
	phys_addr_t align, phys_addr_t start, phys_addr_t end, bool nomap,
	phys_addr_t *res_base)
{
	phys_addr_t base;
	s32 ret = 0;

	memblock_set_bottom_up((bool)true);

	base = memblock_alloc_range(size, align, start, end, MEMBLOCK_NONE);
	if (base == (phys_addr_t)0U) {
		ret = -ENOMEM;
		goto error;
	}

	if (base < start) {
		(void)memblock_free(base, size);
		ret = -ENOMEM;
		goto error;
	}

	*res_base = base;
	if (nomap) {
		ret = memblock_remove(base, size);
	}

error:
	memblock_set_bottom_up((bool)false);
	return ret;
}

static inline struct pmap_entry *pmap_entry_of(struct pmap *info)
{
	return container_of(info, struct pmap_entry, info);
}

static struct pmap *pmap_find_info_by_name(const char *name)
{
	struct pmap_entry *entry = NULL;

	list_for_each_entry(entry, &pmap_list_head, list) {
		s32 cmp = strncmp(name, entry->info.name, TCC_PMAP_NAME_LEN);

		if (cmp == 0) {
			return &entry->info;
		}
	}

	return NULL;
}

static inline u64 pmap_get_base(struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);
	struct pmap_entry *iter = NULL;
	u64 base = 0;

	for (iter = entry; iter != NULL; iter = iter->parent) {
		if (iter->info.base == PMAP_FREED) {
			return 0;
		}

		if ((U64_MAX - base) >= iter->info.base) {
			base += iter->info.base;
		} else {
			/* XXX: Should not happen */
			return 0;
		}
	}

	return base;
}

#ifdef CONFIG_PMAP_TO_CMA
#define PMAP_DMA_ATTR (DMA_ATTR_WRITE_COMBINE | DMA_ATTR_FORCE_CONTIGUOUS | \
		       DMA_ATTR_NO_KERNEL_MAPPING)

#ifdef CONFIG_OPTEE
static void pmap_cma_tee_alloc(struct pmap *info)
{
	s32 id, secure_area;
#ifdef CONFIG_PMAP_DEBUG
	struct timespec start, end;
	ulong elapsed;
#endif

	secure_area = strncmp(info->name, "secure_area", 11);

	if ((info->name[12] == '\0') && (secure_area == 0)) {
		id = (s32)info->name[11] - 0x30;

#ifdef CONFIG_PMAP_DEBUG
		getnstimeofday(&start);
#endif
		tee_alloc_dynanic_smp(id, info->base, info->size);

#ifdef CONFIG_PMAP_DEBUG
		getnstimeofday(&end);

		if (end.tv_nsec < start.tv_nsec) {
			elapsed = end.tv_nsec + (1000000000 - start.tv_nsec);
		} else {
			elapsed = end.tv_nsec - start.tv_nsec;
		}

		pr_debug("[DEBUG][PMAP] %s: 0x%08x++0x%08llx, area_%d, elapsed:%09ldns\n",
			 __func__, info->base, info->size, id, elapsed);
#endif
	}
}

static void pmap_cma_tee_free(struct pmap *info)
{
	s32 id, secure_area;
#ifdef CONFIG_PMAP_DEBUG
	struct timespec start, end;
	ulong elapsed;
#endif

	secure_area = strncmp(info->name, "secure_area", 11);

	if ((info->name[12] == '\0') && (secure_area == 0)) {
		id = (s32)info->name[11] - 0x30;

#ifdef CONFIG_PMAP_DEBUG
		getnstimeofday(&start);
#endif

		tee_free_dynanic_smp(id, info->base, info->size);

#ifdef CONFIG_PMAP_DEBUG
		getnstimeofday(&end);

		if (end.tv_nsec < start.tv_nsec) {
			elapsed = end.tv_nsec + (1000000000 - start.tv_nsec);
		} else {
			elapsed = end.tv_nsec - start.tv_nsec;
		}

		pr_debug("[DEBUG][PMAP] %s: 0x%08x++0x%08x, area_%d, elapsed:%09ldns\n",
			 __func__, info->base, info->size, id, elapsed);
#endif
	}
}
#endif

static s32 pmap_cma_alloc(struct pmap *info)
{
	dma_addr_t phys;
	void *ret;

	pr_debug("[DEBUG][PMAP] cma: request to alloc %s for size 0x%08x\n",
			info->name, info->size);

	ret = dma_alloc_attrs(pmap_device, info->size, &phys, GFP_KERNEL,
			PMAP_DMA_ATTR);

	if (!ret) {
		pr_err("[ERROR][PMAP] cma: failed to alloc %s\n", info->name);
		return 0;
	}

	info->base = (u64) phys;
#ifdef CONFIG_OPTEE
	pmap_cma_tee_alloc(info);
#endif

	pr_debug("[DEBUG][PMAP] cma: %s is allocated at 0x%08llx\n",
			info->name, info->base);

	return 1;
}

static void pmap_cma_release(struct pmap *info)
{
	dma_addr_t phys = (dma_addr_t) info->base;
	void *page = (void *) phys_to_page(phys);

	dma_free_attrs(pmap_device, info->size, page, phys, PMAP_DMA_ATTR);
#ifdef CONFIG_OPTEE
	pmap_cma_tee_free(info);
#endif

	pr_debug("[DEBUG][PMAP] cma: %s is released from 0x%08x\n",
			info->name, info->base);

	info->base = PMAP_FREED;
}

#define VM_ARM_DMA_CONSISTENT	0x20000000

/*
 * FIXME
 * pmap_cma_remap/unmap functions have critical dependency on ...
 *
 * 1. ARM architecture
 * 2. CMA area on highmem
 */
void *pmap_cma_remap(__u32 base, __u32 size)
{
	struct page *page = phys_to_page(base);
	void *virt;

	BUG_ON(!pmap_check_region(base, size));

	pr_debug("[DEBUG][PMAP] cma: request to map 0x%08x for size 0x%08x\n",
			base, size);

	size = PAGE_ALIGN(size);

	virt = dma_common_contiguous_remap(page, size,
			VM_ARM_DMA_CONSISTENT | VM_USERMAP,
			pgprot_writecombine(PAGE_KERNEL),
			__builtin_return_address(0));

	if (!virt) {
		pr_err("[ERROR][PMAP] cma: failed to map 0x%08x\n", base);
	} else {
		pr_debug("[DEBUG][PMAP] cma: 0x%08x is virtually mapped to 0x%08x\n",
				base, (__u32) virt);
	}

	return virt;
}
EXPORT_SYMBOL(pmap_cma_remap);

void pmap_cma_unmap(void *virt, __u32 size)
{
	size = PAGE_ALIGN(size);

	dma_common_free_remap(virt, size, VM_ARM_DMA_CONSISTENT | VM_USERMAP);

	pr_debug("[DEBUG][PMAP] cma: virtual address 0x%08x is unmapped\n",
			(__u32) virt);
}
EXPORT_SYMBOL(pmap_cma_unmap);
#endif

static s32 pmap_get_info_internal(struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if (entry->parent != NULL) {
		s32 ret = pmap_get_info_internal(&entry->parent->info);

		if (ret == 0) {
			return 0;
		}
#ifdef CONFIG_PMAP_TO_CMA
	} else if (!info->rc && pmap_is_cma_alloc(info)) {
		s32 ret = pmap_cma_alloc(info);

		if (ret == 0) {
			return 0;
		}

		pmap_total_size += info->size;
		pmap_list_sorted = false;
#endif
	}

	if (info->rc < UINT_MAX) {
		++info->rc;
	}

	return 1;
}

static void pmap_release_info_internal(struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if ((info->rc != 0U) && (entry->parent != NULL)) {
		pmap_release_info_internal(&entry->parent->info);
#ifdef CONFIG_PMAP_TO_CMA
	} else if ((info->rc == 1U) && pmap_is_cma_alloc(info)) {
		pmap_cma_release(info);
		pmap_total_size -= info->size;
#endif
	}

	if (info->rc > 0U) {
		--info->rc;
	}
}

s32 pmap_get_info(const char *name, struct pmap *mem)
{
	struct pmap *info = pmap_find_info_by_name(name);
	s32 ret;

	if (mem != NULL) {
		mem->base = 0;
		mem->size = 0;
	}

	if ((mem == NULL) || (info == NULL)) {
		return 0;
	}

	ret = pmap_get_info_internal(info);

	if (ret == 0) {
		return -1;
	}

	(void)memcpy(mem, info, sizeof(struct pmap));
	mem->base = pmap_get_base(info);
	return 1;
}
EXPORT_SYMBOL(pmap_get_info);

s32 pmap_release_info(const char *name)
{
	struct pmap *info = pmap_find_info_by_name(name);

	if (info == NULL) {
		return 0;
	}

	pmap_release_info_internal(info);

	return 1;
}
EXPORT_SYMBOL(pmap_release_info);

s32 pmap_check_region(u64 base, u64 size)
{
	struct pmap_entry *entry = NULL;
	u64 rbase, rend, end;

	if ((U64_MAX - base) >= size) {
		end = base + size;
	} else {
		end = U64_MAX;
	}

	list_for_each_entry(entry, &pmap_list_head, list) {
		rbase = pmap_get_base(&entry->info);

		if ((U64_MAX - rbase) >= entry->info.size) {
			rend = rbase + entry->info.size;
		} else {
			rend = U64_MAX;
		}

		if ((rbase != (u64)0U) && (rbase <= base) && (rend >= end)) {
			return 1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(pmap_check_region);

#ifdef CONFIG_PROC_FS
#define pmap_get_order(info) \
	(((info)->groups == PMAP_DATA_NA) ? 2 : (pmap_is_shared(info) ? 1 : 0))

static int pmap_compare(void *p, struct list_head *a, struct list_head *b)
{
	struct pmap *pa = &list_entry(a, struct pmap_entry, list)->info;
	struct pmap *pb = &list_entry(b, struct pmap_entry, list)->info;

	s32 order_a = pmap_get_order(pa);
	s32 order_b = pmap_get_order(pb);

	u64 base_a, base_b;

	if (order_a != order_b) {
		return (order_a < order_b) ? -1 : 1;
	}

	base_a = pmap_get_base(pa);
	base_b = pmap_get_base(pb);

	return (base_a <= base_b) ? -1 : 1;
}

#undef pmap_get_order

static int proc_pmap_show(struct seq_file *m, void *v)
{
	struct pmap_entry *entry = NULL;
	s32 shared_info = 0, secured_info = 0;

	if (!pmap_list_sorted) {
		list_sort(NULL, &pmap_list_head, pmap_compare);
		pmap_list_sorted = (bool)true;
	}

	/* format:     %-10s      %-10s      %3s %3s %s */
	seq_puts(m, "base_addr  size       cma ref name\n");

	list_for_each_entry(entry, &pmap_list_head, list) {
		struct pmap *info = &entry->info;
		char *is_cma = pmap_is_cma_alloc(info) ? "*" : " ";
		u64 base = pmap_get_base(info);

		if (base == 0U) {
			continue;
		}

		if ((shared_info == 0) && pmap_is_shared(info)) {
			seq_puts(m, " ======= Shared Area Info. =======\n");
			shared_info = 1;
		}

		if ((secured_info == 0) && (info->groups == PMAP_DATA_NA)) {
			seq_puts(m, " ======= Secured Area Info. =======\n");
			secured_info = 1;
		}

		seq_printf(m, "0x%8.8llx 0x%8.8llx  %s  %-3u %s\n",
			   base, info->size, is_cma, info->rc, info->name);
	}

	seq_puts(m, " ======= Total Area Info. =======\n");
	seq_printf(m, "0x00000000 0x%8.8llx         total\n", pmap_total_size);

	return 0;
}

static int proc_pmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_pmap_show, PDE_DATA(inode));
}

static long proc_pmap_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct user_pmap __user *uinfo = (struct user_pmap __user *) arg;
	struct user_pmap ret_info;
	struct pmap info;
	s32 ret = -1;
	ulong copied;

	copied = copy_from_user(&ret_info, uinfo, sizeof(struct user_pmap));
	if (copied != 0UL) {
		return -1;
	}

	switch (cmd) {
	case PROC_PMAP_CMD_GET:
		ret = pmap_get_info(ret_info.name, &info);

		ret_info.base = (u32)info.base;
		ret_info.size = (u32)info.size;
		break;
	case PROC_PMAP_CMD_RELEASE:
		ret = pmap_release_info(ret_info.name);
		break;
	default:
		/* Nothing to do */
		break;
	}

	copied = copy_to_user(uinfo, &ret_info, sizeof(struct user_pmap));
	if (copied != 0UL) {
		return -1;
	}

	return ret;
}

static const struct file_operations proc_pmap_fops = {
	.open		= proc_pmap_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.unlocked_ioctl	= proc_pmap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= proc_pmap_ioctl,
#endif
};
#endif

static void pmap_update_info(struct device_node *np, struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if (pmap_is_secured(info)) {
		if (pmap_is_cma_alloc(info)) {
			info->base = entry->parent->info.size;
		} else if (info->base >= entry->parent->info.base) {
			info->base -= entry->parent->info.base;
		} else {
			/* XXX: Should not happen */
		}

		if ((U64_MAX - entry->parent->info.size) >= info->size) {
			entry->parent->info.size += info->size;
		} else {
			/* XXX: Should not happen */
		}

		if (pmap_is_cma_alloc(&entry->parent->info)) {
			info->flags |= PMAP_FLAG_CMA_ALLOC;
		}
	} else if (pmap_is_shared(info)) {
		struct device_node *sn;
		struct pmap *shared;
		const char *name;
		const __be32 *prop;
		s32 len, ret;
		u64 offset = 0, size = 0;

		sn = of_parse_phandle(np, "telechips,pmap-shared", 0);
		ret = of_property_read_string(sn, "telechips,pmap-name", &name);
		if (ret != 0) {
			return;
		}

		shared = pmap_find_info_by_name(name);
		if (shared == NULL) {
			return;
		}

		prop = of_get_property(np, "telechips,pmap-offset", &len);
		ret = of_n_addr_cells(np);
		if ((prop != NULL) && (ret == (len/(s32)sizeof(u32)))) {
			offset = of_read_number(prop, of_n_addr_cells(np));
		}

		prop = of_get_property(np, "telechips,pmap-shared-size", &len);
		ret = of_n_size_cells(np);
		if ((prop != NULL) && (ret == (len/(s32)sizeof(u32)))) {
			size = of_read_number(prop, of_n_size_cells(np));
		}

		entry->parent = pmap_entry_of(shared);
		info->base = offset;
		info->size = size;

		if (pmap_is_cma_alloc(&entry->parent->info)) {
			info->flags |= PMAP_FLAG_CMA_ALLOC;
		}
	} else {
		/* Nothing to do */
	}

	/* Remove pmap with size 0 from entry list */
	if (entry->info.size == 0U) {
		list_del(&entry->list);
	}
}

int __init tcc_pmap_init(void)
{
#if defined(CONFIG_PMAP_TO_CMA)
	struct class *class;
#endif
	struct pmap_entry *entry;
	struct device_node *np;
#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *dir;
#endif
	u32 i;

	pmap_list_sorted = (bool)false;
	pmap_total_size = (u64)0U;

	/*
	 * Create entry under procfs for user interface and create a
	 * device to use DMA API on cma alloc/release.
	 */
#if defined(CONFIG_PMAP_TO_CMA)
	class = class_create(THIS_MODULE, "pmap");

	pmap_device = device_create(class, NULL, 0, NULL, "pmap");
	if (IS_ERR(pmap_device)) {
		class_destroy(class);
		return PTR_ERR(pmap_device);
	}

	if (dma_set_coherent_mask(pmap_device, DMA_BIT_MASK(32)) < 0) {
		return -EIO;
	}
#endif

#if defined(CONFIG_PROC_FS)
	dir = proc_create("pmap", 0444, NULL, &proc_pmap_fops);
	if (dir == NULL) {
		return -ENOMEM;
	}
#endif

	/*
	 * Initialize secure area table as 0 and set initial value for
	 * each secure area.
	 */
	(void)memset(secure_area_table, 0, sizeof(secure_area_table));

	for (i = 0; i < MAX_PMAP_GROUPS; i++) {
		entry = &secure_area_table[i];

		(void)sprintf(entry->info.name, "secure_area%d", (s32)i + 1);
		entry->info.base = PMAP_FREED;
		entry->info.groups = PMAP_DATA_NA;
	}

	/*
	 * Register secure area as a parent for each secured pmap and
	 * calculate base address of each secure area.
	 */
	for (i = 0; i < MAX_PMAPS; i++) {
		entry = &pmap_table[i];

		if (pmap_is_secured(&entry->info)) {
			s32 group_idx = (s32)entry->info.groups-1;

			entry->parent = &secure_area_table[group_idx];
			entry->parent->info.flags |= entry->info.flags;

			if ((entry->info.base < entry->parent->info.base) &&
					(entry->info.size != 0U)) {
				entry->parent->info.base = entry->info.base;
			}
		}

		/* Calculate pmap total size */
		if (!pmap_is_cma_alloc(&entry->info)) {
			if ((U64_MAX - pmap_total_size) >= entry->info.size) {
				pmap_total_size += entry->info.size;
			} else {
				/* XXX: Should not happen */
			}
		}
	}

	/*
	 * Add reg property for each device tree node if there are only
	 * size property with dynamic allocation.
	 */
	for_each_compatible_node(np, NULL, "telechips,pmap") {
		const char *name;
		struct pmap *info;
		s32 ncell, naddr, nsize, ret;
		struct property *prop;
		__be32 *pval;
		const void *reg;

		ret = of_property_read_string(np, "telechips,pmap-name", &name);
		if (ret != 0) {
			continue;
		}

		/* Skip if there are reg property */
		reg = of_get_property(np, "reg", NULL);
		if (reg != NULL) {
			continue;
		}

		info = pmap_find_info_by_name(name);
		if (info == NULL) {
			continue;
		}

		naddr = of_n_addr_cells(np);
		nsize = of_n_size_cells(np);
		ncell = naddr + nsize;

		prop = kzalloc(sizeof(*prop), GFP_KERNEL);

		prop->value = kzalloc((size_t)ncell * sizeof(__be32),
				      GFP_KERNEL);
		pval = prop->value;

		if (naddr > 1) {
			*pval = 0;
			pval++;
		}
		*pval = cpu_to_be32((__u32)info->base);
		pval++;

		if (nsize > 1) {
			*pval = 0;
			pval++;
		}
		*pval = cpu_to_be32((__u32)info->size);
		pval++;

		prop->name = kstrdup("reg", GFP_KERNEL);
		prop->length = ncell * (s32)sizeof(__be32);

		of_add_property(np, prop);

		/* Update info with pmap related properties */
		pmap_update_info(np, info);
	}

	/*
	 * Register secure areas with size > 0 into pmap list to print
	 * out later when user requests.
	 */
	for (i = 0; i < MAX_PMAP_GROUPS; i++) {
		entry = &secure_area_table[i];

		if (entry->info.size != 0U) {
			list_add_tail(&entry->list, &pmap_list_head);
		}
	}

	(void)pr_info("[INFO][PMAP] total reserved %llu bytes (%llu KiB, %llu MiB)\n",
		      pmap_total_size, pmap_total_size >> 10,
		      pmap_total_size >> 20);

	return 0;
}
postcore_initcall(tcc_pmap_init);

static int __init rmem_pmap_setup(struct reserved_mem *rmem)
{
	static s32 num_pmaps_left = (s32)MAX_PMAPS;
	ulong node = rmem->fdt_node;
	const char *name;
	const __be32 *prop;
	s32 len;
	u32 flags = 0;
	u64 groups = 0, alloc_size = 0;
	struct pmap_entry *entry;
	phys_addr_t end = rmem->base;

	if ((rmem->size > 0U) && ((ULONG_MAX - rmem->base) >= rmem->size)) {
		end = rmem->base + rmem->size;
	}

	pr_debug("[DEBUG][PMAP] reserved %ld MiB\t[%pa-%pa]\n",
		 (ulong)rmem->size >> 20, &rmem->base, &end);

	name = of_get_flat_dt_prop(node, "telechips,pmap-name", NULL);
	if (name == NULL) {
		return -EINVAL;
	}

#ifdef CONFIG_PMAP_SECURED
	/*
	 * When CONFIG_PMAP_SECURED is not set, simply ignore "telechips,
	 * pmap-secured" property.
	 */
	prop = of_get_flat_dt_prop(node, "telechips,pmap-secured", &len);

	if (prop != NULL) {
		groups = of_read_number(prop, len/4);

		if ((groups >= 1U) && (groups <= MAX_PMAP_GROUPS)) {
			flags |= PMAP_FLAG_SECURED;
		}
	}
#endif

	prop = of_get_flat_dt_prop(node, "telechips,pmap-shared", NULL);

	if (prop != NULL) {
		flags |= PMAP_FLAG_SHARED;
	}

#ifdef CONFIG_PMAP_TO_CMA
	/*
	 * When CONFIG_PMAP_TO_CMA is not set, simply ignore "telechips,
	 * cma-alloc-size" property.
	 */
	prop = of_get_flat_dt_prop(node, "telechips,cma-alloc-size", &len);

	if (prop != NULL) {
		alloc_size = of_read_number(prop, len/4);

		if (alloc_size != 0U) {
			flags |= PMAP_FLAG_CMA_ALLOC;
		}
	}
#endif

	/* Initialize pmap and register it into pmap list */
	if (num_pmaps_left == 0) {
		return -ENOMEM;
	}

	entry = &pmap_table[(s32)MAX_PMAPS - num_pmaps_left];
	--num_pmaps_left;

	(void)strncpy(entry->info.name, name, TCC_PMAP_NAME_LEN);
	entry->info.base = (alloc_size != 0U) ? PMAP_FREED : (u64)rmem->base;
	entry->info.size = (alloc_size != 0U) ? alloc_size : (u64)rmem->size;
	entry->info.groups = (u32)groups;
	entry->info.rc = 0;
	entry->info.flags = flags;

	list_add_tail(&entry->list, &pmap_list_head);

	return 0;
}
RESERVEDMEM_OF_DECLARE(pmap, "telechips,pmap", rmem_pmap_setup);

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("Telechips PMAP driver");
MODULE_LICENSE("GPL");
