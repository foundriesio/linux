/*
 * PMAP driver for Telechips SoCs
 *
 * Copyright (C) 2010-2019 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifdef CONFIG_PMAP_TO_CMA
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

struct device *pmap_device;
#endif

#define PMAP_DATA_NA		((unsigned int) ~(0))

#define MAX_PMAPS		64
#define MAX_PMAP_GROUPS		4

#ifdef CONFIG_PROC_FS
#define PROC_PMAP_CMD_GET	0x501
#define PROC_PMAP_CMD_RELEASE	0x502

typedef struct {
	char name[TCC_PMAP_NAME_LEN];
	unsigned int base;
	unsigned int size;
} user_pmap_t;
#endif

static bool pmap_list_sorted = false;
static unsigned int num_pmaps = 0;
static unsigned int pmap_total_size = 0;

struct pmap_entry {
	pmap_t info;
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
	int ret = 0;

	memblock_set_bottom_up(true);

	base = memblock_alloc_range(size, align, start, end, MEMBLOCK_NONE);
	if (!base) {
		ret = -ENOMEM;
		goto error;
	}

	if (base < start) {
		memblock_free(base, size);
		ret = -ENOMEM;
		goto error;
	}

	*res_base = base;
	if (nomap)
		ret = memblock_remove(base, size);

error:
	memblock_set_bottom_up(false);
	return ret;
}

static inline struct pmap_entry *pmap_entry_of(pmap_t *info)
{
	return container_of(info, struct pmap_entry, info);
}

static pmap_t *pmap_find_info_by_name(const char *name)
{
	struct pmap_entry *entry = NULL;

	list_for_each_entry(entry, &pmap_list_head, list) {
		if (!strcmp(name, entry->info.name)) {
			return &entry->info;
		}
	}

	return NULL;
}

static inline __u32 pmap_get_base(pmap_t *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);
	struct pmap_entry *iter = NULL;
	__u32 base = 0;

	for (iter = entry; iter; iter = iter->parent) {
		if (iter->info.base == PMAP_DATA_NA)
			return 0;

		base += iter->info.base;
	}

	return base;
}

#ifdef CONFIG_PMAP_TO_CMA
#define PMAP_DMA_ATTR DMA_ATTR_WRITE_COMBINE | DMA_ATTR_FORCE_CONTIGUOUS | \
	DMA_ATTR_NO_KERNEL_MAPPING

static __u32 pmap_cma_alloc(pmap_t *info)
{
	dma_addr_t phys;
	void *ret;

	pr_debug("pmap: cma: request to alloc %s for size 0x%08x\n",
			info->name, info->size);

	ret = dma_alloc_attrs(pmap_device, info->size, &phys, GFP_KERNEL,
			PMAP_DMA_ATTR);

	if (!ret) {
		pr_err("\x1b[31m" "pmap: cma: failed to alloc %s" "\x1b[0m\n",
				info->name);
		return 0;
	}

	info->base = (__u32) phys;

	pr_debug("pmap: cma: %s is allocated at 0x%08x\n",
			info->name, info->base);

	return 1;
}

static void pmap_cma_release(pmap_t *info)
{
	dma_addr_t phys = (dma_addr_t) info->base;
	void *page = (void *) phys_to_page(phys);

	dma_free_attrs(pmap_device, info->size, page, phys, PMAP_DMA_ATTR);

	pr_debug("pmap: cma: %s is released from 0x%08x\n",
			info->name, info->base);

	info->base = PMAP_DATA_NA;
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

	pr_debug("pmap: cma: request to map 0x%08x for size 0x%08x\n",
			base, size);

	size = PAGE_ALIGN(size);

	virt = dma_common_contiguous_remap(page, size,
			VM_ARM_DMA_CONSISTENT | VM_USERMAP,
			pgprot_writecombine(PAGE_KERNEL),
			__builtin_return_address(0));

	if (!virt)
		pr_err("\x1b[31m" "pmap: cma: failed to map 0x%08x" "\x1b[0m\n",
				base);
	else
		pr_debug("pmap: cma: 0x%08x is virtually mapped to 0x%08x\n",
				base, (__u32) virt);

	return virt;
}
EXPORT_SYMBOL(pmap_cma_remap);

void pmap_cma_unmap(void *virt, __u32 size)
{
	size = PAGE_ALIGN(size);

	dma_common_free_remap(virt, size, VM_ARM_DMA_CONSISTENT | VM_USERMAP);

	pr_debug("pmap: cma: virtual address 0x%08x is unmapped\n",
			(__u32) virt);
}
EXPORT_SYMBOL(pmap_cma_unmap);
#endif

static int __pmap_get_info(pmap_t *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if (entry->parent) {
		if (!__pmap_get_info(&entry->parent->info))
			return 0;
#ifdef CONFIG_PMAP_TO_CMA
	} else if (!info->rc && pmap_is_cma_alloc(info)) {
		if (!pmap_cma_alloc(info))
			return 0;

		pmap_total_size += info->size;
		pmap_list_sorted = false;
#endif
	}

	++info->rc;

	return 1;
}

static void __pmap_release_info(pmap_t *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if (info->rc && entry->parent) {
		__pmap_release_info(&entry->parent->info);
#ifdef CONFIG_PMAP_TO_CMA
	} else if (info->rc == 1 && pmap_is_cma_alloc(info)) {
		pmap_cma_release(info);
		pmap_total_size -= info->size;
#endif
	}

	if (info->rc > 0)
		--info->rc;
}

int pmap_get_info(const char *name, pmap_t *mem)
{
	pmap_t *info = pmap_find_info_by_name(name);

	if (mem) {
		mem->base = 0;
		mem->size = 0;
	}

	if (!mem || !info)
		return 0;

	if (!__pmap_get_info(info))
		return -1;

	memcpy(mem, info, sizeof(pmap_t));
	mem->base = pmap_get_base(info);
	return 1;
}
EXPORT_SYMBOL(pmap_get_info);

int pmap_release_info(const char *name)
{
	pmap_t *info = pmap_find_info_by_name(name);

	if (!info)
		return 0;

	__pmap_release_info(info);

	return 1;
}
EXPORT_SYMBOL(pmap_release_info);

int pmap_check_region(__u32 base, __u32 size)
{
	struct pmap_entry *entry = NULL;
	__u32 start, end;

	list_for_each_entry(entry, &pmap_list_head, list) {
		start = pmap_get_base(&entry->info);
		end = start + entry->info.size;

		if (!start)
			continue;

		if (start <= base && end >= base + size)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL(pmap_check_region);

#ifdef CONFIG_PROC_FS
#define pmap_get_order(info) \
	( (info)->groups == PMAP_DATA_NA ? 2 : (pmap_is_shared(info) ? 1 : 0) )

static int pmap_compare(void *p, struct list_head *a, struct list_head *b)
{
	pmap_t *pa = &list_entry(a, struct pmap_entry, list)->info;
	pmap_t *pb = &list_entry(b, struct pmap_entry, list)->info;

	int order_a = pmap_get_order(pa);
	int order_b = pmap_get_order(pb);

	if (order_a != order_b)
		return order_a < order_b ? -1 : 1;

	return pmap_get_base(pa) <= pmap_get_base(pb) ? -1 : 1;
}

#undef pmap_get_order

static int proc_pmap_show(struct seq_file *m, void *v)
{
	struct pmap_entry *entry = NULL;
	int shared_info = 0, secured_info = 0;

	if (!pmap_list_sorted) {
		list_sort(NULL, &pmap_list_head, pmap_compare);
		pmap_list_sorted = true;
	}

	/* format:     %-10s      %-10s      %3s %3s %s */
	seq_printf(m, "base_addr  size       cma ref name\n");

	list_for_each_entry(entry, &pmap_list_head, list) {
		pmap_t *info = &entry->info;
		char is_cma = pmap_is_cma_alloc(info) ? '*' : ' ';

		if (!pmap_get_base(info))
			continue;

		if (!shared_info && pmap_is_shared(info)) {
			seq_printf(m, " ======= Shared Area Info. ======= \n");
			shared_info = 1;
		}

		if (!secured_info && info->groups == PMAP_DATA_NA) {
			seq_printf(m, " ======= Secured Area Info. ======= \n");
			secured_info = 1;
		}

		seq_printf(m, "0x%8.8x 0x%8.8x  %c  %-3u %s\n",
				pmap_get_base(info), info->size, is_cma,
				info->rc, info->name);
	}

	seq_printf(m, " ======= Total Area Info. ======= \n");
	seq_printf(m, "0x00000000 0x%8.8x         total\n", pmap_total_size);

	return 0;
}

static int proc_pmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_pmap_show, PDE_DATA(inode));
}

static long proc_pmap_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	user_pmap_t __user *uinfo = (user_pmap_t __user *) arg;
	user_pmap_t ret_info;
	pmap_t info;
	int ret;

	if (copy_from_user(&ret_info, uinfo, sizeof(user_pmap_t)))
		return -1;

	switch (cmd) {
	case PROC_PMAP_CMD_GET:
		ret = pmap_get_info(ret_info.name, &info);

		ret_info.base = info.base;
		ret_info.size = info.size;
		break;
	case PROC_PMAP_CMD_RELEASE:
		ret = pmap_release_info(ret_info.name);
		break;
	default:
		ret = -1;
	}

	if (copy_to_user(uinfo, &ret_info, sizeof(user_pmap_t)))
		return -1;

	return ret;
}

static const struct file_operations proc_pmap_fops = {
	.open		= proc_pmap_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.unlocked_ioctl	= proc_pmap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= proc_pmap_ioctl,
#endif
};
#endif

static void pmap_update_info(struct device_node *np, pmap_t *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if (pmap_is_secured(info)) {
		if (pmap_is_cma_alloc(info))
			info->base = entry->parent->info.size;
		else
			info->base -= entry->parent->info.base;

		entry->parent->info.size += info->size;

		if (pmap_is_cma_alloc(&entry->parent->info))
			info->flags |= PMAP_FLAG_CMA_ALLOC;
	} else if (pmap_is_shared(info)) {
		struct device_node *node;
		const char *name;
		pmap_t *shared;
		u32 offset = 0, size = 0;

		node = of_parse_phandle(np, "telechips,pmap-shared", 0);
		if (of_property_read_string(node, "telechips,pmap-name", &name))
			return;

		if (!(shared = pmap_find_info_by_name(name)))
			return;

		of_property_read_u32(np, "telechips,pmap-offset", &offset);
		of_property_read_u32(np, "telechips,pmap-shared-size", &size);

		entry->parent = pmap_entry_of(shared);
		info->base = offset;
		info->size = size;

		if (pmap_is_cma_alloc(&entry->parent->info))
			info->flags |= PMAP_FLAG_CMA_ALLOC;
	}

	/* Remove pmap with size 0 from entry list */
	if (!entry->info.size)
		list_del(&entry->list);
}

static int __init tcc_pmap_init(void)
{
#ifdef CONFIG_PMAP_TO_CMA
	struct class *class;
#endif
	struct pmap_entry *entry;
	struct device_node *np;
	int i;

	/*
	 * Create entry under procfs for user interface and create a
	 * device to use DMA API on cma alloc/release.
	 */
#ifdef CONFIG_PMAP_TO_CMA
	class = class_create(THIS_MODULE, "pmap");

	pmap_device = device_create(class, NULL, 0, NULL, "pmap");
	if (IS_ERR(pmap_device)) {
		class_destroy(class);
		return PTR_ERR(pmap_device);
	}

	if (dma_set_coherent_mask(pmap_device, DMA_BIT_MASK(32)) < 0)
		return -EIO;
#endif

#ifdef CONFIG_PROC_FS
	if (!proc_create("pmap", S_IRUGO, NULL, &proc_pmap_fops))
		return -ENOMEM;
#endif

	/*
	 * Initialize secure area table as 0 and set initial value for
	 * each secure area.
	 */
	memset(secure_area_table, 0, sizeof(secure_area_table));

	for (i = 0; i < MAX_PMAP_GROUPS; i++) {
		entry = &secure_area_table[i];

		sprintf(entry->info.name, "secure_area%d", i+1);
		entry->info.base = PMAP_DATA_NA;
		entry->info.groups = PMAP_DATA_NA;
	}

	/*
	 * Register secure area as a parent for each secured pmap and
	 * calculate base address of each secure area.
	 */
	for (i = 0; i < MAX_PMAPS; i++) {
		entry = &pmap_table[i];

		if (pmap_is_secured(&entry->info)) {
			entry->parent = &secure_area_table[entry->info.groups-1];
			entry->parent->info.flags |= entry->info.flags;

			if (entry->info.size && entry->info.base < entry->parent->info.base) {
				entry->parent->info.base = entry->info.base;
			}
		}

		/* Calculate pmap total size */
		if (!pmap_is_cma_alloc(&entry->info))
			pmap_total_size += entry->info.size;
	}

	/*
	 * Add reg property for each device tree node if there are only
	 * size property with dynamic allocation.
	 */
	for_each_compatible_node(np, NULL, "telechips,pmap") {
		const char *name;
		pmap_t *info;
		int naddr, nsize;
		struct property *prop;
		__be32 *val, *pval;

		if (of_property_read_string(np, "telechips,pmap-name", &name))
			continue;

		/* Skip if there are reg property */
		if (of_get_property(np, "reg", NULL))
			continue;

		if (!(info = pmap_find_info_by_name(name)))
			continue;

		naddr = of_n_addr_cells(np);
		nsize = of_n_size_cells(np);

		prop = kzalloc(sizeof(*prop), GFP_KERNEL);
		val = kzalloc((naddr + nsize) * sizeof(__be32), GFP_KERNEL);
		pval = val;

		if (naddr > 1)
			*pval++ = 0;
		*pval++ = cpu_to_be32(info->base);

		if (nsize > 1)
			*pval++ = 0;
		*pval++ = cpu_to_be32(info->size);

		prop->name = kstrdup("reg", GFP_KERNEL);
		prop->length = (naddr + nsize) * sizeof(__be32);
		prop->value = val;

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

		if (entry->info.size)
			list_add_tail(&entry->list, &pmap_list_head);
	}

	pr_info("pmap: total reserved %u bytes (%u KiB, %u MiB)\n",
			pmap_total_size, pmap_total_size >> 10,
			pmap_total_size >> 20);

	return 0;
}
postcore_initcall(tcc_pmap_init);

static int __init rmem_pmap_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;
	phys_addr_t end = rmem->size ? rmem->base + rmem->size - 1 : rmem->base;
	const char *name;
	const __be32 *prop;
	unsigned int len, flags = 0;
	u64 groups = 0, alloc_size = 0;
	struct pmap_entry *entry;

	pr_debug("pmap: reserved %ld MiB\t[%pa-%pa]\n",
			(unsigned long) rmem->size >> 20, &rmem->base, &end);

	name = of_get_flat_dt_prop(node, "telechips,pmap-name", NULL);
	if (!name)
		return -EINVAL;

#ifdef CONFIG_PMAP_SECURED
	/*
	 * When CONFIG_PMAP_SECURED is not set, simply ignore "telechips,
	 * pmap-secured" property.
	 */
	prop = of_get_flat_dt_prop(node, "telechips,pmap-secured", &len);

	if (prop) {
		groups = of_read_number(prop, len/4);

		if (1 <= groups && groups <= MAX_PMAP_GROUPS)
			flags |= PMAP_FLAG_SECURED;
	}
#endif

	prop = of_get_flat_dt_prop(node, "telechips,pmap-shared", NULL);

	if (prop) {
		flags |= PMAP_FLAG_SHARED;
	}

#ifdef CONFIG_PMAP_TO_CMA
	/*
	 * When CONFIG_PMAP_TO_CMA is not set, simply ignore "telechips,
	 * cma-alloc-size" property.
	 */
	prop = of_get_flat_dt_prop(node, "telechips,cma-alloc-size", &len);

	if (prop) {
		alloc_size = of_read_number(prop, len/4);

		if (alloc_size)
			flags |= PMAP_FLAG_CMA_ALLOC;
	}
#endif

	/* Initialize pmap and register it into pmap list */
	if (num_pmaps >= MAX_PMAPS)
		return -ENOMEM;

	entry = &pmap_table[num_pmaps];
	num_pmaps++;

	strncpy(entry->info.name, name, TCC_PMAP_NAME_LEN);
	entry->info.base = alloc_size ? PMAP_DATA_NA : rmem->base;
	entry->info.size = alloc_size ? alloc_size : rmem->size;
	entry->info.groups = groups;
	entry->info.rc = 0;
	entry->info.flags = flags;

	list_add_tail(&entry->list, &pmap_list_head);

	return 0;
}
RESERVEDMEM_OF_DECLARE(pmap, "telechips,pmap", rmem_pmap_setup);

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("Telechips PMAP driver");
MODULE_LICENSE("GPL");
