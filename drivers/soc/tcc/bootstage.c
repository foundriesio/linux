// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
#include <soc/tcc/tcc-sip.h>

#define bootstage_pr_err(msg, err) \
	pr_err("[ERROR][bootstage] Failed to %s. (err: %d)\n", (msg), err)

#if defined(CONFIG_ARCH_TCC805X)
#define NR_BOOT_STAGES (25U)

static const char *const bootstage_desc[NR_BOOT_STAGES] = {
	/* Boot Stages on Storage Core BL0 */
	"setup sc bl0",
	"storage init",
	"post storage init",
	"reset hsm",
	"load sc fw header",
	"load sc fw",
	"wait hsm",
	"verify sc fw",
	"set hsm ready",
	"set sc ready",
	/* Boot Stages on AP Boot Firmware */
	"load bl1",
	"setup bl1",
	"load h/w config",
	"initialize dram",
	"load bl2",
	"load secure f/w",
	"load bl3",
	"setup bl2",
	/* Boot Stages on AP U-Boot */
	"setup bl3",
	"board init f",
	"relocation",
	"board init r",
	"main loop",
	"boot kernel",
	/* Boot Stages on Kernel */
	"kernel init",
};
#else
#define NR_BOOT_STAGES (0U)
#endif

static inline unsigned long add_boot_timestamp(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_ADD_BOOTTIME, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static inline unsigned long get_boot_timestamp(unsigned long index)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOTTIME, index, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static inline unsigned long get_boot_timestamp_num(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOTTIME_NUM, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static inline void validate_boot_timestamp_num(struct seq_file *m, u32 num)
{
	if (num != NR_BOOT_STAGES) {
		seq_printf(m, "WARNING: Number of stages does not match. (actual: %u, expected: %u)\n\n",
			   num, NR_BOOT_STAGES);
	}
}

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
		/* Remove unnecessary delimiters and zeros in front */
		++idx;
	}

	if (format[idx] == '\0') {
		/* Keep the last zero, if the num is just zero */
		--idx;
	}

	return &format[idx];
}

static int bootstage_report_show(struct seq_file *m, void *v)
{
	/*
	 * CAUTION. This function is **NOT THREAD SAFE** !!!
	 */

	u32 n, num, nr_actual = 0;
	u32 stamp, prev = 0;
	const char *stamp_fmt, *desc;

	num = get_boot_timestamp_num();
	validate_boot_timestamp_num(m, num);

	seq_printf(m, "Timer summary in microseconds (%u records):\n", num);

	seq_printf(m, "%11s%11s  %s\n", "Mark", "Elapsed", "Stage");
	seq_printf(m, "%11u%11u  %s\n", 0U, 0U, "reset");

	for (n = 0; n < num; n++) {
		desc = n < NR_BOOT_STAGES ? bootstage_desc[n] : "-";
		stamp = get_boot_timestamp(n);

		if (stamp != 0) {
			stamp_fmt = u32_to_str_in_format(stamp);
			seq_printf(m, "%11s", stamp_fmt);

			stamp_fmt = u32_to_str_in_format(stamp - prev);
			seq_printf(m, "%11s", stamp_fmt);

			prev = stamp;
			seq_printf(m, "  %s\n", desc);

			++nr_actual;
		}
	}

	seq_printf(m, "\n%u stages out of %u stages were proceeded. (%u stages skipped)\n",
		   nr_actual, num, num - nr_actual);

	return 0;
}

static int bootstage_report_open(struct inode *inode, struct file *file)
{
	return single_open(file, bootstage_report_show, PDE_DATA(inode));
}

static const struct file_operations bootstage_report_fops = {
	.open		= bootstage_report_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int bootstage_data_show(struct seq_file *m, void *v)
{
	/*
	 * CAUTION. This function is **NOT THREAD SAFE** !!!
	 */

	u32 n, num;
	u32 stamp, prev = 0;
	const char *desc;

	num = get_boot_timestamp_num();
	validate_boot_timestamp_num(m, num);

	seq_puts(m, "reset,0\n");

	for (n = 0; n < num; n++) {
		desc = n < NR_BOOT_STAGES ? bootstage_desc[n] : "-";
		stamp = get_boot_timestamp(n);

		if (stamp == 0) {
			seq_printf(m, "%s,0\n", bootstage_desc[n]);
			continue;
		}

		seq_printf(m, "%s,%u\n", bootstage_desc[n], stamp - prev);
		prev = stamp;
	}

	return 0;
}

static int bootstage_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, bootstage_data_show, PDE_DATA(inode));
}

static const struct file_operations bootstage_data_fops = {
	.open		= bootstage_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int bootstage_pm_notifier_call(struct notifier_block *nb,
				      unsigned long action, void *data)
{
	switch (action) {
	case PM_POST_SUSPEND:
		add_boot_timestamp();
		break;
	default:
		break;
	}

	return 0;
}

struct notifier_block bootstage_pm_notifier = {
	.notifier_call = bootstage_pm_notifier_call,
};

static struct proc_dir_entry *bootstage_procfs_init(void)
{
	struct proc_dir_entry *pdir, *pent;

	/* Create `/proc/bootstage` procfs directory */
	pdir = proc_mkdir("bootstage", NULL);
	if (pdir == NULL) {
		/* Nothing to do, just return */
		goto procfs_dir_init_error;
	}

	/* Create `/proc/bootstage/report` procfs entry */
	pent = proc_create("report", 0444, pdir, &bootstage_report_fops);
	if (pent == NULL) {
		pdir = NULL;
		goto procfs_report_init_error;
	}

	/* Create `/proc/bootstage/data` procfs entry */
	pent = proc_create("data", 0444, pdir, &bootstage_data_fops);
	if (pent == NULL) {
		pdir = NULL;
		goto procfs_data_init_error;
	}

	return pdir;

	// Below line will never gonna happened, but keep in comments for
	// future usage:
	// remove_proc_entry("data", pdir);
procfs_data_init_error:
	remove_proc_entry("report", pdir);
procfs_report_init_error:
	remove_proc_entry("bootstage", NULL);
procfs_dir_init_error:
	return pdir;
}

static void bootstage_procfs_free(struct proc_dir_entry *pdir)
{
	remove_proc_entry("data", pdir);
	remove_proc_entry("report", pdir);
	remove_proc_entry("bootstage", NULL);
}
static int bootstage_init(void)
{
	struct proc_dir_entry *pdir;
	int ret = 0;

	/* Create `/proc/bootstage` procfs directory and child entries */
	pdir = bootstage_procfs_init();
	if (pdir == NULL) {
		ret = -ENOMEM;
		bootstage_pr_err("init `proc/bootstage` procfs", ret);
		goto procfs_init_error;
	}

	/* Register PM notifier for boot time until resumption */
	ret = register_pm_notifier(&bootstage_pm_notifier);
	if (ret != 0) {
		bootstage_pr_err("init `proc/bootstage` procfs", ret);
		goto pm_notifier_register_error;
	}

	return 0;

	// Below line will never gonna happened, but keep in comments for
	// future usage:
	// unregister_pm_notifier(&bootstage_pm_notifier);
pm_notifier_register_error:
	bootstage_procfs_free(pdir);
procfs_init_error:
	return ret;
}

late_initcall(bootstage_init);

MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips bootstage driver");
MODULE_LICENSE("GPL");
