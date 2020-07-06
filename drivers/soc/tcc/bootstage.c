// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/of.h>

#if defined(CONFIG_PROC_FS)
#  include <linux/proc_fs.h>
#  include <linux/seq_file.h>
#endif

#define MAX_STAGES	64U

static unsigned int nr_stages = 0;

typedef struct {
	const char *name;
	u32 stamp;

	struct list_head list;
} stage_t;

static stage_t stage_pool[MAX_STAGES];

static LIST_HEAD(bootstage_list_head);
static LIST_HEAD(bootaccum_list_head);

#if defined(CONFIG_PROC_FS)
static inline const char *u32_to_str_in_format(u32 num)
{
	static char format[13]; /* ",000,000,000" */
	u32 idx = 0U, div = 1000000U;

	while (div != 0U) {
		sprintf(&format[idx], ",%03u", (num / div) % 1000U);
		div /= 1000U;
		idx += 4U;
	}

	idx = 0U;

	while (format[idx] == ',' || format[idx] == '0') {
		++idx;
	}

	if (format[idx] == '\0') {
		--idx;
	}

	return &format[idx];
}

static int proc_bootstage_show(struct seq_file *m, void *v)
{
	/*
	 * CAUTION. This function is **NOT THREAD SAFE** !!!
	 */

	const char *stamp_s;
	stage_t *stage;
	u32 prev = 0U;

	seq_printf(m, "Timer summary in microseconds (%u records):\n",
			nr_stages);

	seq_printf(m, "%11s%11s  %s\n", "Mark", "Elapsed", "Stage");
	list_for_each_entry(stage, &bootstage_list_head, list) {
		stamp_s = u32_to_str_in_format(stage->stamp);
		seq_printf(m, "%11s", stamp_s);

		stamp_s = u32_to_str_in_format(stage->stamp - prev);
		seq_printf(m, "%11s", stamp_s);

		prev = stage->stamp;
		seq_printf(m, "  %s\n", stage->name);
	}

	seq_printf(m, "\nAccumulated time:\n");
	list_for_each_entry(stage, &bootaccum_list_head, list) {
		stamp_s = u32_to_str_in_format(stage->stamp);
		seq_printf(m, "%11s%11s  %s\n", "", stamp_s, stage->name);
	}

	return 0;
}

static int proc_bootstage_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_bootstage_show, PDE_DATA(inode));
}

static const struct file_operations proc_bootstage_fops = {
	.open		= proc_bootstage_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif

static void __init parse_one_bootstage(struct device_node *node)
{
	struct list_head *head = NULL;
	stage_t *stage = NULL;
	int ret;

	if (nr_stages >= MAX_STAGES) {
		return;
	}

	stage = &stage_pool[nr_stages];

	ret = of_property_read_string(node, "name", &(stage->name));
	if (ret == 0) {
		ret = of_property_read_u32(node, "mark", &(stage->stamp));
		if (ret == 0) {
			head = &bootstage_list_head;
		} else {
			ret = of_property_read_u32(node, "accum", &(stage->stamp));
			if (ret == 0) {
				head = &bootaccum_list_head;
			}
		}
	}

	if (head != NULL) {
		list_add_tail(&(stage->list), head);
		++nr_stages;
	}
}

static int __init cmp_stages(void *p, struct list_head *a, struct list_head *b)
{
	stage_t *sa = list_entry(a, stage_t, list);
	stage_t *sb = list_entry(b, stage_t, list);

	return (sa->stamp <= sb->stamp) ? -1 : 1;
}

static int __init tcc_bootstage_init(void)
{
	struct device_node *node, *sub;

#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *pdir;

	pdir = proc_create("bootstage", S_IRUGO, NULL, &proc_bootstage_fops);
	if (pdir == NULL) {
		return -ENOMEM;
	}
#endif

	node = of_find_node_by_name(NULL, "bootstage");

	/*
	 * Bootstage configuration(s) on U-Boot may not be enabled,
	 * so it is acceptable to be failed on finding `bootstage` node.
	 */
	if (node == NULL) {
		return 0;
	}

	for_each_child_of_node(node, sub) {
		parse_one_bootstage(sub);
	}

	list_sort(NULL, &bootstage_list_head, cmp_stages);
	list_sort(NULL, &bootaccum_list_head, cmp_stages);

	return 0;
}
late_initcall(tcc_bootstage_init);

MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips bootstage driver");
MODULE_LICENSE("GPL");
