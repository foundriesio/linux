/* linux/arch/arm/plat-tcc/pmap.c
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/module.h>
#include <soc/tcc/pmap.h>

#define ATAG_TCC_PMAP	0x5443436d	/* TCCm */

#define MAX_PMAPS		64

static int num_pmaps = 0;
static pmap_t pmap_table[MAX_PMAPS];

int pmap_get_info(const char *name, pmap_t *mem)
{
	int i;
	for (i = 0; i < num_pmaps; i++) {
		if (strcmp(name, pmap_table[i].name) == 0) {
			memcpy(mem, &pmap_table[i], sizeof(pmap_t));
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(pmap_get_info);

#ifdef CONFIG_OF
extern pmap_t * tcc_get_pmap_table(void);
extern unsigned tcc_get_pmap_size(void);

int pmap_early_init(void)
{
	pmap_t *pmap = pmap_table;
	pmap_t *entry = tcc_get_pmap_table();
	unsigned count, n;

	if (entry == NULL)
		return -1;

	count = tcc_get_pmap_size();
	if (count > MAX_PMAPS)
		count = MAX_PMAPS;

	for (n = 0; n < count; n++) {
		memcpy(pmap->name, entry->name, TCC_PMAP_NAME_LEN - 1);
		pmap->name[TCC_PMAP_NAME_LEN - 1] = 0;
		pmap->base = entry->base;
		pmap->size = entry->size;
		entry++;
		pmap++;
	}
	num_pmaps = count;
	return 0;
}
EXPORT_SYMBOL(pmap_early_init);

pmap_t * __weak tcc_get_pmap_table(void)
{
	return NULL;
}

unsigned __weak tcc_get_pmap_size(void)
{
	return 0;
}
#else
static int __init parse_tag_tcc_pmap(const struct tag *tag)
{
	pmap_t *pmap = pmap_table;
	pmap_t *entry = (void *) &tag->u;
	unsigned count, n;

	count = (tag->hdr.size - 2) / ((sizeof(pmap_t) / sizeof(__u32)));
	if (count > MAX_PMAPS)
		count = MAX_PMAPS;

	for (n = 0; n < count; n++) {
		memcpy(pmap->name, entry->name, TCC_PMAP_NAME_LEN - 1);
		pmap->name[TCC_PMAP_NAME_LEN - 1] = 0;
		pmap->base = entry->base;
		pmap->size = entry->size;
		entry++;
		pmap++;
	}
	num_pmaps = count;
	return 0;
}
__tagtable(ATAG_TCC_PMAP, parse_tag_tcc_pmap);
#endif

static int proc_pmap_show(struct seq_file *m, void *v)
{
	pmap_t *p = pmap_table;
	int i;

	seq_printf(m, "%-10s %-10s %s\n", "base_addr", "size", "name");
	for (i = 0; i < num_pmaps; i++, p++) {
		seq_printf(m, "0x%8.8x 0x%8.8x %s\n",
				p->base, p->size, p->name);
	}
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

static int __init tcc_pmap_init(void)
{
#ifdef CONFIG_PROC_FS
	if (!proc_create("pmap", S_IRUGO, NULL, &proc_pmap_fops))
		return -ENOMEM;
#endif
	return 0;
}
postcore_initcall(tcc_pmap_init);
