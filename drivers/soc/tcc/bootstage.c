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

static inline void add_boot_timestamp(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_ADD_BOOTTIME, 0, 0, 0, 0, 0, 0, 0, &res);
}

static inline u32 get_boot_timestamp(ulong index)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOTTIME, index, 0, 0, 0, 0, 0, 0, &res);

	return (u32)res.a0;
}

static inline u32 get_boot_timestamp_num(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOTTIME_NUM, 0, 0, 0, 0, 0, 0, 0, &res);

	return (u32)res.a0;
}

static inline void validate_boot_timestamp_num(struct seq_file *m, u32 num)
{
	if (num != NR_BOOT_STAGES) {
		seq_printf(m, "WARNING: Number of stages does not match. (actual: %u, expected: %u)\n\n",
			   num, NR_BOOT_STAGES);
	}
}

static inline const char *u32_to_str_in_format(u32 num, char *format)
{
	u32 ths = 1000000U;
	s32 idx = 0;

	for (idx = 0; idx < 12; idx += 4) {
		(void)sprintf(&format[idx], ",%03u", (num / ths) % 1000U);
		ths /= 1000U;
	}

	idx = 0;

	/* Remove unnecessary delimiters and zeros in front */
	while (((u8)format[idx] == (u8)',') || ((u8)format[idx] == (u8)'0')) {
		++idx;
	}

	if ((u8)format[idx] == (u8)'\0') {
		/* Keep the last zero, if the num is just zero */
		--idx;
	}

	return &format[idx];
}

static int bootstage_report_show(struct seq_file *m, void *v)
{
	u32 n, num, stamp, prev = 0;
	s32 nr_actual = 0;
	const char *fmt, *desc;
	char buf[13]; /* ",000,000,000" */

	num = get_boot_timestamp_num();
	validate_boot_timestamp_num(m, num);

	seq_printf(m, "Timer summary in microseconds (%u records):\n", num);

	seq_printf(m, "%11s%11s  %s\n", "Mark", "Elapsed", "Stage");
	seq_printf(m, "%11u%11u  %s\n", 0U, 0U, "reset");

	for (n = 0; n < num; n++) {
		desc = (n < NR_BOOT_STAGES) ? bootstage_desc[n] : "-";
		stamp = get_boot_timestamp(n);

		if (stamp != 0U) {
			fmt = u32_to_str_in_format(stamp, buf);
			seq_printf(m, "%11s", fmt);

			if (stamp >= prev) {
				fmt = u32_to_str_in_format(stamp - prev, buf);
			} else {
				fmt = "overflow";
			}
			seq_printf(m, "%11s", fmt);

			prev = stamp;
			seq_printf(m, "  %s\n", desc);

			++nr_actual;
		}
	}

	seq_printf(m, "\n%d stages out of %u stages were proceeded. (%d stages skipped)\n",
		   nr_actual, num, (s32)num - nr_actual);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(bootstage_report);

static int bootstage_data_show(struct seq_file *m, void *v)
{
	u32 n, num;
	u32 stamp, prev = 0;
	const char *desc;

	num = get_boot_timestamp_num();
	validate_boot_timestamp_num(m, num);

	seq_puts(m, "reset,0\n");

	for (n = 0; n < num; n++) {
		desc = (n < NR_BOOT_STAGES) ? bootstage_desc[n] : "-";
		stamp = get_boot_timestamp(n);

		if (stamp == 0U) {
			seq_printf(m, "%s,0\n", desc);
			continue;
		}

		if (stamp >= prev) {
			seq_printf(m, "%s,%u\n", desc, stamp - prev);
		} else {
			seq_printf(m, "%s,overflow\n", desc);
		}
		prev = stamp;
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(bootstage_data);

static int bootstage_pm_notifier_call(struct notifier_block *nb,
				      unsigned long action, void *data)
{
	switch (action) {
	case PM_POST_SUSPEND:
		add_boot_timestamp();
		break;
	default:
		/* Nothing to do */
		break;
	}

	return 0;
}

static struct notifier_block bootstage_pm_notifier = {
	.notifier_call = bootstage_pm_notifier_call,
};

static struct proc_dir_entry *bootstage_procfs_init(void)
{
	struct proc_dir_entry *pdir, *pent;

	/* Create '/proc/bootstage' procfs directory */
	pdir = proc_mkdir("bootstage", NULL);
	if (pdir == NULL) {
		/* Nothing to do, just return */
		goto procfs_dir_init_error;
	}

	/* Create '/proc/bootstage/report' procfs entry */
	pent = proc_create("report", 0444, pdir, &bootstage_report_fops);
	if (pent == NULL) {
		pdir = NULL;
		goto procfs_report_init_error;
	}

	/* Create '/proc/bootstage/data' procfs entry */
	pent = proc_create("data", 0444, pdir, &bootstage_data_fops);
	if (pent == NULL) {
		pdir = NULL;
		goto procfs_data_init_error;
	}

	return pdir;

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
	s32 ret = 0;

	/* Create '/proc/bootstage' procfs directory and child entries */
	pdir = bootstage_procfs_init();
	if (pdir == NULL) {
		ret = -ENOMEM;
		(void)bootstage_pr_err("init 'proc/bootstage' procfs", ret);
		goto procfs_init_error;
	}

	/* Register PM notifier for boot time until resumption */
	ret = register_pm_notifier(&bootstage_pm_notifier);
	if (ret != 0) {
		(void)bootstage_pr_err("register pm notifier", ret);
		goto pm_notifier_register_error;
	}

	return 0;

pm_notifier_register_error:
	bootstage_procfs_free(pdir);
procfs_init_error:
	return ret;
}

late_initcall(bootstage_init);

MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips bootstage driver");
MODULE_LICENSE("GPL");
