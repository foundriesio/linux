/*
 * PMAP driver for Telechips SoCs
 *
 * Copyright (C) 2010-2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/memblock.h>
#include <uapi/asm/setup.h>
#include <soc/tcc/pmap.h>
#include <video/tcc/tcc_vpu_wbuffer.h>

#define MAX_PMAPS		64

typedef struct {
	__u32 base;
	__u32 size;
}pmap_secured_groups_t;

static int num_pmaps = 0;
static unsigned pmap_total_size = 0;

struct pmap_table {
    pmap_t info;
    struct list_head list;
};
static struct pmap_table stPmap[MAX_PMAPS];

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

int pmap_get_info(const char *name, pmap_t *mem)
{
	struct pmap_table *pmap_tbl = NULL;

	list_for_each_entry(pmap_tbl, &pmap_list_head, list) {
		if (strcmp(name, pmap_tbl->info.name) == 0) {
			memcpy(mem, &pmap_tbl->info, sizeof(pmap_t));
			return 1;
		}
	}

	mem->base = 0x00;
	mem->size = 0x00;
	return 0;
}
EXPORT_SYMBOL(pmap_get_info);

int pmap_check_region(__u32 base, __u32 size)
{
	__u32 start, end;
	struct pmap_table *pmap_tbl = NULL;

	list_for_each_entry(pmap_tbl, &pmap_list_head, list) {
		start = pmap_tbl->info.base;
		end = pmap_tbl->info.base + pmap_tbl->info.size;

		if (start <= base && end >= base + size)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL(pmap_check_region);

static int proc_pmap_show(struct seq_file *m, void *v)
{
	struct pmap_table *pmap_tbl = NULL;
	int bShared_Info = 0, bSecured_Info = 0;

	seq_printf(m, "%-10s %-10s %s\n", "base_addr", "size", "name");

	list_for_each_entry(pmap_tbl, &pmap_list_head, list) {
		if(!bShared_Info && pmap_tbl->info.flags & PMAP_FLAG_SHARED)
		{
			seq_printf(m, " ======= Shared Area Info. ======= \n");
			bShared_Info = 1;
		}

		if (!bSecured_Info && strncmp("secure_area", pmap_tbl->info.name, 11) == 0)
		{
			seq_printf(m, " ======= Secured Area Info. ======= \n");
			bSecured_Info = 1;
		}

		if(pmap_tbl->info.size)
			seq_printf(m, "0x%8.8x 0x%8.8x %s\n",
					pmap_tbl->info.base, pmap_tbl->info.size, pmap_tbl->info.name);
	}

	seq_printf(m, " ======= Total Area Info. ======= \n");
	seq_printf(m, "0x00000000 0x%8.8x total\n", pmap_total_size);

	return 0;
}

static int proc_pmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_pmap_show, PDE_DATA(inode));
}

static const struct file_operations proc_pmap_fops = {
	.open		= proc_pmap_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static struct pmap_table* get_new_pmap_table(const char *name, unsigned long base, unsigned long size, unsigned int flags, unsigned int groups)
{
	struct pmap_table *pmap_tbl = NULL;

	pmap_tbl = &stPmap[num_pmaps];//kmalloc(sizeof(struct pmap_table), GFP_KERNEL);
	pr_debug("get_new_pmap_table 0x%x :: 0x%8.8x 0x%8.8x %s\n", pmap_tbl, base, size, name);

	if(!pmap_tbl)
		return -ENOMEM;

	num_pmaps++;
	strncpy(pmap_tbl->info.name, name, TCC_PMAP_NAME_LEN);
	pmap_tbl->info.base = base;
	pmap_tbl->info.size = size;
	pmap_tbl->info.flags = flags;
	pmap_tbl->info.groups = groups;

	return pmap_tbl;
}


static int pmap_add(const char *name, unsigned long base, unsigned long size, unsigned int flags, unsigned int groups)
{
	struct pmap_table *pmap_tbl = NULL;
	int added_done = 0;

    pmap_tbl = get_new_pmap_table(name, base, size, flags, groups);
    if(!pmap_tbl)
        return 0;

	if(list_empty(&pmap_list_head)){
	    list_add(&pmap_tbl->list, &pmap_list_head);
		added_done = 1;
	}
	else
	{
		struct pmap_table *pmap_prev = NULL, *pmap_curr = NULL;
		list_for_each_entry(pmap_curr, &pmap_list_head, list) {
			if( pmap_curr && (pmap_tbl->info.base <= pmap_curr->info.base) )
			{
				pr_debug("final add:: 0x%8.8x 0x%8.8x %s\n", pmap_curr->info.base, pmap_curr->info.size, pmap_curr->info.name);

				if(!pmap_prev)
					list_add(&pmap_tbl->list, &pmap_list_head);
				else
					list_add(&pmap_tbl->list, &pmap_prev->list);

				added_done = 1;
				break;
			}
			pmap_prev = pmap_curr;
		}
	}

	if(added_done == 0){
		list_add_tail(&pmap_tbl->list, &pmap_list_head);
	}

	return 0;
}

static int pmap_add_secure_info(const char *name, unsigned long base, unsigned long size, unsigned int flags, unsigned int groups)
{
	struct pmap_table *pmap_tbl = NULL;

    pmap_tbl = get_new_pmap_table(name, base, size, flags, groups);
    if(!pmap_tbl)
        return 0;

    list_add_tail(&pmap_tbl->list, &pmap_list_head);

	return 0;
}

static int pmap_add_shared_info(const char *name, unsigned long base, unsigned long size, unsigned int flags)
{
	struct pmap_table *pmap_tbl = NULL;

	pr_debug("pmap_add_shared_info :: 0x%8.8x 0x%8.8x %s\n", base, size, name);

	list_for_each_entry(pmap_tbl, &pmap_list_head, list) {
		if (strcmp(name, pmap_tbl->info.name) == 0) {
			//del
			list_del(&pmap_tbl->list);
			pmap_tbl->info.base = base;
			pmap_tbl->info.size = size;
			pmap_tbl->info.flags = flags;
			//add
			list_add_tail(&pmap_tbl->list, &pmap_list_head);
			return 0;
		}
	}

	return -ENOENT;
}

static int __init tcc_pmap_init(void)
{
	struct device_node *np;
	unsigned size;
	int i, groups = 0;
	pmap_secured_groups_t secured_groups[MAX_PMAPS];
	struct pmap_table *pmap_tbl = NULL;

#ifdef CONFIG_PROC_FS
	if (!proc_create("pmap", S_IRUGO, NULL, &proc_pmap_fops))
		return -ENOMEM;
#endif

	memset(secured_groups, 0 , sizeof(secured_groups));
	list_for_each_entry(pmap_tbl, &pmap_list_head, list) {
		size += pmap_tbl->info.size;

		if (pmap_tbl->info.flags & PMAP_FLAG_SECURED) {
			groups = pmap_tbl->info.groups;
			if((groups != 0) && (groups < MAX_PMAPS))  {
				if (!secured_groups[groups].base || pmap_tbl->info.base < secured_groups[groups].base)
					secured_groups[groups].base  = pmap_tbl->info.base;

				secured_groups[groups].size += pmap_tbl->info.size;
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
		pmap_t pmap;
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

		if (!pmap_get_info(name, &pmap))
			continue;

		naddr = of_n_addr_cells(np);
		nsize = of_n_size_cells(np);

		if (pmap.flags & PMAP_FLAG_SHARED) {
			const char *shared_name;
			struct device_node *node;
			pmap_t shared_pmap;
			u32 offset, size;

			node = of_parse_phandle(np, "telechips,pmap-shared", 0);
			ret = of_property_read_string(node, "telechips,pmap-name", &shared_name);
			if (ret)
				continue;

			ret = pmap_get_info(shared_name, &shared_pmap);
			if (!ret)
				continue;

			ret = of_property_read_u32(np, "telechips,pmap-offset", &offset);
			if (ret)
				offset = 0;

			ret = of_property_read_u32(np, "telechips,pmap-shared-size", &size);
			if (ret)
				size = 0;

			pmap_add_shared_info(name, shared_pmap.base + offset, size,
				 pmap.flags);
		}

		prop = kzalloc(sizeof(*prop), GFP_KERNEL);
		val = kzalloc((naddr + nsize) * sizeof(__be32), GFP_KERNEL);
		pval = val;
		if (naddr > 1)
			*pval++ = 0;
		*pval++ = cpu_to_be32(pmap.base);
		if (nsize > 1)
			*pval++ = 0;
		*pval++ = cpu_to_be32(pmap.size);
		prop->name = kstrdup("reg", GFP_KERNEL);
		prop->length = (naddr + nsize) * sizeof(__be32);
		prop->value = val;
		of_add_property(np, prop);
	}

	for(i = 0; i < MAX_PMAPS; i++)	{
		char buf[TCC_PMAP_NAME_LEN];
		if(secured_groups[i].size != 0) {
			snprintf(buf, TCC_PMAP_NAME_LEN, "secure_area%d", i);
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
	int shared;
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
		if(len)
			groups = of_read_number(secured, len/4);
	}

	shared = of_get_flat_dt_prop(node, "telechips,pmap-shared", NULL) != NULL;
	if (shared){
		flags |= PMAP_FLAG_SHARED;
	}

	pmap_add(name, rmem->base, rmem->size, flags, groups);

	return 0;
}
RESERVEDMEM_OF_DECLARE(pmap, "telechips,pmap", rmem_pmap_setup);

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("Telechips PMAP driver");
MODULE_LICENSE("GPL");
