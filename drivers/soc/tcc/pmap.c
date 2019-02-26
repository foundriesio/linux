/*
 * PMAP driver for Telechips SoCs
 *
 * Copyright (C) 2010-2019 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

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
#include <soc/tcc/pmap.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_DMA_CMA
#include <linux/cma.h>
#endif

#define MAX_PMAPS 64
#define MAX_PMAP_GROUPS 4

typedef struct {
	__u32 base;
	__u32 size;
} pmap_secured_groups_t;

static int num_pmaps = 0;
static unsigned pmap_total_size = 0;

struct pmap_entry {
	pmap_t info;
	struct list_head list;
};

static struct pmap_entry pmap_table[MAX_PMAPS];

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

#ifdef CONFIG_DMA_CMA
extern struct cma *dma_contiguous_default_area;

static __u32 pmap_cma_alloc(pmap_t *info)
{
	size_t count = info->size >> PAGE_SHIFT;
	unsigned int align = get_order(info->size);
	struct page *page = cma_alloc(dma_contiguous_default_area, count, align, GFP_KERNEL);

	if (!page) {
		pr_err("%s: cma alloc failed for %s\n", __func__, info->name);
		return 0;
	}

	info->base = (__u32) page_to_phys(page);
	info->v_base = (__u32) page_to_virt(page);

	{
		struct pmap_entry *entry = container_of(info, struct pmap_entry, info);
		struct pmap_entry *iter = NULL;

		list_del(&entry->list);

		list_for_each_entry(iter, &pmap_list_head, list) {
			if (pmap_is_shared(&iter->info) ||
					entry->info.base <= iter->info.base) {
				list_add_tail(&entry->list, &iter->list);
				break;
			}
		}
	}

	return 1;
}

static int pmap_cma_release(pmap_t *info)
{
	unsigned int count = info->size >> PAGE_SHIFT;
	struct page *page = phys_to_page(info->base);

	if (!cma_release(dma_contiguous_default_area, page, count)) {
		pr_err("%s: cma release failed for %s\n", __func__, info->name);
		return 0;
	}

	info->base = ~(0);
	info->v_base = 0;

	return 1;
}
#endif

int pmap_get_info(const char *name, pmap_t *mem)
{
	pmap_t *info = pmap_find_info_by_name(name);

	if (!info)
		goto err;

#ifdef CONFIG_DMA_CMA
	if (pmap_is_cma_alloc(info)) {
		if (!info->rc)
			if (!pmap_cma_alloc(info))
				goto err;

		++info->rc;
	}
#endif

	memcpy(mem, info, sizeof(pmap_t));
	return 1;

err:
	mem->base = 0;
	mem->size = 0;

	return 0;
}
EXPORT_SYMBOL(pmap_get_info);

int pmap_release_info(const char *name)
{
	pmap_t *info = pmap_find_info_by_name(name);

	if (!info)
		return 0;

#ifdef CONFIG_DMA_CMA
	if (pmap_is_cma_alloc(info)) {
		if (info->rc == 1)
			if (!pmap_cma_release(info))
				return 0;

		if (info->rc > 0)
			--info->rc;
	}
#endif

	return 1;
}
EXPORT_SYMBOL(pmap_release_info);

int pmap_check_region(__u32 base, __u32 size)
{
	struct pmap_entry *entry = NULL;
	__u32 start, end;

	list_for_each_entry(entry, &pmap_list_head, list) {
		start = entry->info.base;
		end = entry->info.base + entry->info.size;

		if (start <= base && end >= base + size)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL(pmap_check_region);

static struct pmap_entry *get_new_pmap_entry(const char *name, unsigned long base, unsigned long size, unsigned int flags, unsigned int groups)
{
	struct pmap_entry *entry = NULL;

	if (num_pmaps >= MAX_PMAPS)
		return NULL;

	pr_debug("%s :: 0x%8.8lx 0x%8.8lx %s\n", __func__, base, size, name);

	entry = &pmap_table[num_pmaps];
	num_pmaps++;

	strncpy(entry->info.name, name, TCC_PMAP_NAME_LEN);
	entry->info.base = base;
	entry->info.size = size;
	entry->info.flags = flags;
	entry->info.groups = groups;

	entry->info.v_base = 0;
	entry->info.rc = 0;

	return entry;
}

static int pmap_add(const char *name, unsigned long base, unsigned long size, unsigned int flags, unsigned int groups)
{
	struct pmap_entry *entry = NULL;

	entry = get_new_pmap_entry(name, base, size, flags, groups);
	if (!entry)
		return 0;

	if (list_empty(&pmap_list_head)) {
		list_add(&entry->list, &pmap_list_head);
	} else {
		struct pmap_entry *pmap_iter = NULL;
		int added_done = 0;

		list_for_each_entry(pmap_iter, &pmap_list_head, list) {
			if (entry->info.base <= pmap_iter->info.base) {
				list_add_tail(&entry->list, &pmap_iter->list);
				added_done = 1;
				break;
			}
		}

		if (!added_done)
			list_add_tail(&entry->list, &pmap_list_head);
	}

	return 0;
}

static int pmap_add_secure_info(const char *name, unsigned long base, unsigned long size, unsigned int flags, unsigned int groups)
{
	struct pmap_entry *entry = NULL;

	entry = get_new_pmap_entry(name, base, size, flags, groups);
	if (!entry)
		return 0;

	list_add_tail(&entry->list, &pmap_list_head);

	return 0;
}

static int pmap_add_shared_info(const char *name, unsigned long base, unsigned long size, unsigned int flags)
{
	struct pmap_entry *entry = NULL;

	pr_debug("%s :: 0x%8.8lx 0x%8.8lx %s\n", __func__, base, size, name);

	list_for_each_entry(entry, &pmap_list_head, list) {
		if (!strcmp(name, entry->info.name)) {
			//del
			list_del(&entry->list);
			entry->info.base = base;
			entry->info.size = size;
			entry->info.flags = flags;
			//add
			list_add_tail(&entry->list, &pmap_list_head);
			return 0;
		}
	}

	return -ENOENT;
}

#ifdef CONFIG_PROC_FS
static int proc_pmap_show(struct seq_file *m, void *v)
{
	struct pmap_entry *entry = NULL;
	int shared_info = 0, secured_info = 0;

	seq_printf(m, "%-10s %-10s %-3s %s\n", "base_addr", "size", "ref", "name");

	list_for_each_entry(entry, &pmap_list_head, list) {
		if (!shared_info && pmap_is_shared(&entry->info)) {
			seq_printf(m, " ======= Shared Area Info. ======= \n");
			shared_info = 1;
		}

		if (!secured_info && !strncmp("secure_area", entry->info.name, 11)) {
			seq_printf(m, " ======= Secured Area Info. ======= \n");
			secured_info = 1;
		}

		if (entry->info.size && entry->info.base != ~(0))
			seq_printf(m, "0x%8.8x 0x%8.8x %-3u %s\n", entry->info.base,
					entry->info.size, entry->info.rc, entry->info.name);
	}

	seq_printf(m, " ======= Total Area Info. ======= \n");
	seq_printf(m, "0x00000000 0x%8.8x     total\n", pmap_total_size);

	return 0;
}

static int proc_pmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_pmap_show, PDE_DATA(inode));
}

static const struct file_operations proc_pmap_info_fops = {
	.open		= proc_pmap_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#ifdef CONFIG_DMA_CMA
static ssize_t proc_pmap_alloc(struct file *f, const char __user *name, size_t size, loff_t *off)
{
	pmap_t info;
	char pmap_name[size];

	memcpy(pmap_name, name, size);
	pmap_name[size-1] = '\0';

	if (!pmap_get_info(pmap_name, &info))
		return -EINVAL;

	return size;
}

static const struct file_operations proc_pmap_alloc_fops = {
	.write		= proc_pmap_alloc,
};

static ssize_t proc_pmap_release(struct file *f, const char __user *name, size_t size, loff_t *off)
{
	char pmap_name[size];

	memcpy(pmap_name, name, size);
	pmap_name[size-1] = '\0';

	if (!pmap_release_info(pmap_name))
		return -EINVAL;

	return size;
}

static const struct file_operations proc_pmap_release_fops = {
	.write		= proc_pmap_release,
};
#endif

static int pmap_create_proc(void)
{
	struct proc_dir_entry * dir = proc_mkdir("pmap",NULL);

	if (!dir)
		return 0;

	if (!proc_create("info", S_IRUGO, dir, &proc_pmap_info_fops))
		return 0;

#ifdef CONFIG_DMA_CMA
	if (!proc_create("alloc", S_IWUGO, dir, &proc_pmap_alloc_fops))
		return 0;

	if (!proc_create("release", S_IWUGO, dir, &proc_pmap_release_fops))
		return 0;
#endif

	return 1;
}
#endif

static int __init tcc_pmap_init(void)
{
	struct device_node *np;
	unsigned size = 0;
	int i, groups = 0;
	pmap_secured_groups_t secured_groups[MAX_PMAP_GROUPS];
	struct pmap_entry *entry = NULL;

#ifdef CONFIG_PROC_FS
	if (!pmap_create_proc()) {
		return -ENOMEM;
	}
#endif

	/*
	 * Calculate total size for ...
	 * 1. every pmaps on the pmap_list_head
	 * 2. each pmap secured group
	 */
	memset(secured_groups, 0, sizeof(secured_groups));
	list_for_each_entry(entry, &pmap_list_head, list) {
		pmap_t *info = &entry->info;

		if (!pmap_is_cma_alloc(info))
			size += info->size;

		if (pmap_is_secured(info)) {
			groups = info->groups - 1;
			if (-1 < groups && groups < MAX_PMAP_GROUPS) {
				if (!secured_groups[groups].base || info->base < secured_groups[groups].base)
					secured_groups[groups].base = info->base;

				secured_groups[groups].size += info->size;
			}
		}
	}
	pmap_total_size = size;

	/*
	 * Add reg property for each device tree node if there are only
	 * size property with dynamic allocation.
	 */
	for_each_compatible_node(np, NULL, "telechips,pmap") {
		const char *name;
		pmap_t *pmap;
		struct property *prop;
		__be32 *val, *pval;
		int naddr, nsize;
		int ret;

		ret = of_property_read_string(np, "telechips,pmap-name", &name);
		if (ret)
			continue;

		/* Skip if there are reg property */
		if (of_get_property(np, "reg", NULL))
			continue;

		if (!(pmap = pmap_find_info_by_name(name)))
			continue;

		naddr = of_n_addr_cells(np);
		nsize = of_n_size_cells(np);

		if (pmap_is_cma_alloc(pmap)) {
			u32 size;

			ret = of_property_read_u32(np, "telechips,cma-alloc-size", &size);
			if (ret)
				continue;

			pmap->base = ~(0);
			pmap->size = size;
		}

		if (pmap_is_shared(pmap)) {
			const char *shared_name;
			struct device_node *node;
			pmap_t *shared_pmap;
			u32 offset, size;

			node = of_parse_phandle(np, "telechips,pmap-shared", 0);
			ret = of_property_read_string(node, "telechips,pmap-name", &shared_name);
			if (ret)
				continue;

			shared_pmap = pmap_find_info_by_name(shared_name);
			if (!shared_pmap)
				continue;

			ret = of_property_read_u32(np, "telechips,pmap-offset", &offset);
			if (ret)
				offset = 0;

			ret = of_property_read_u32(np, "telechips,pmap-shared-size", &size);
			if (ret)
				size = 0;

			pmap_add_shared_info(name, shared_pmap->base + offset, size,
				 pmap->flags);
		}

		prop = kzalloc(sizeof(*prop), GFP_KERNEL);
		val = kzalloc((naddr + nsize) * sizeof(__be32), GFP_KERNEL);
		pval = val;
		if (naddr > 1)
			*pval++ = 0;
		*pval++ = cpu_to_be32(pmap->base);
		if (nsize > 1)
			*pval++ = 0;
		*pval++ = cpu_to_be32(pmap->size);
		prop->name = kstrdup("reg", GFP_KERNEL);
		prop->length = (naddr + nsize) * sizeof(__be32);
		prop->value = val;
		of_add_property(np, prop);
	}

	for (i = 0; i < MAX_PMAP_GROUPS; i++) {
		char buf[TCC_PMAP_NAME_LEN];
		if (secured_groups[i].size) {
			snprintf(buf, TCC_PMAP_NAME_LEN, "secure_area%d", i+1);
			pmap_add_secure_info(buf, secured_groups[i].base, secured_groups[i].size, PMAP_FLAG_SECURED, 0);
		}
	}

	pr_info("pmap: Total reserved %d bytes (%d KiB, %d MiB)\n",
			size, size >> 10, size >> 20);

	return 0;
}
postcore_initcall(tcc_pmap_init);

static int __init rmem_pmap_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;
	phys_addr_t end = rmem->size ? rmem->base + rmem->size - 1 : rmem->base;
	unsigned int len, flags = 0;
	int shared, cma_alloc;
	const char *name;
	u64 groups = 0;
	const __be32 *secured;

	pr_debug("pmap: Reserved %ld MiB\t[%pa-%pa]\n",
		(unsigned long) rmem->size >> 20, &rmem->base, &end);

	name = of_get_flat_dt_prop(node, "telechips,pmap-name", NULL);
	if (!name)
		return -EINVAL;


	secured = of_get_flat_dt_prop(node, "telechips,pmap-secured", &len);

	if (secured != NULL) {
		flags |= PMAP_FLAG_SECURED;
		if (len)
			groups = of_read_number(secured, len/4);
	}

	shared = of_get_flat_dt_prop(node, "telechips,pmap-shared", NULL) != NULL;
	if (shared) {
		flags |= PMAP_FLAG_SHARED;
	}

	cma_alloc = of_get_flat_dt_prop(node, "telechips,cma-alloc-size", NULL) != NULL;
	if (cma_alloc) {
		/*
		 * Currently, pmap driver does not support cma allocation for shared or
		 * secured regions.  Gently ignore "telechips,cma-alloc-size" property
		 * of shared or secured regions, until the support is added.
		 */
		if (!flags)
			flags |= PMAP_FLAG_CMA_ALLOC;
	}

	pmap_add(name, rmem->base, rmem->size, flags, groups);

	return 0;
}
RESERVEDMEM_OF_DECLARE(pmap, "telechips,pmap", rmem_pmap_setup);

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("Telechips PMAP driver");
MODULE_LICENSE("GPL");
