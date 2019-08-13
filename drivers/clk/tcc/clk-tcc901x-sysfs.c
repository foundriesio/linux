#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <linux/arm-smccc.h>
#include <linux/clk/tcc.h>
#include <soc/tcc/tcc-sip.h>

#include "clk-tcc901x.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

static struct dentry *rootdir;

static int tcc_clk_peri_show(struct seq_file *s, void *data)
{
	struct arm_smccc_res res;
	int i;
	int en;

	for (i = 0; i < PERI_MAX; i++) {
		arm_smccc_smc(SIP_CLK_IS_PERI, i, 0, 0, 0, 0, 0, 0, &res);
		en = res.a0;

		seq_printf(s, "PERI : %3d ", i);
		if (en == 1) {
			arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, i, 0, 0, 0, 0, 0, 0, &res);
			seq_printf(s, "Source : PLL_%d   ", res.a1);
			seq_printf(s, "div : %6d   ", res.a2+1);
			seq_printf(s, "Rate : %10d   ", res.a0);
			seq_printf(s, "(enabled) ");
		}
		else {
			seq_printf(s, "(disabled) ");
		}
		seq_printf(s, "\n");
	}

	return 0;
}

static int tcc_clk_peri_open(struct inode *inode, struct file *file)
{
	return single_open(file, tcc_clk_peri_show, inode->i_private);
}

static const struct file_operations tcc_clk_fops = {
	.open		= tcc_clk_peri_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tcc_clk_debug_init(void)
{
	struct clk_core *core;
	struct dentry *d;

	rootdir = debugfs_create_dir("tcc_clk", NULL);

	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("peri_summary", S_IRUGO, rootdir, NULL,
				&tcc_clk_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

__initcall(tcc_clk_debug_init);
#endif
