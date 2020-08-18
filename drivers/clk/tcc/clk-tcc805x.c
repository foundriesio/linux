// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2020 Telechips Inc.
 */

#include <linux/clocksource.h>
//#include <linux/of_address.h>
#include <linux/clk/tcc.h>
//#include <linux/clk-provider.h>
//#include <linux/irqflags.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/system_info.h>
#endif

#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

#define TCC805X_CKC_DRIVER
#include "clk-tcc805x.h"


#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MAX_TCC_PLL 5
#define MEMBUS_PLL 4

struct ckc_backup_sts {
	unsigned long rate;
	unsigned long en;
};

struct ckc_backup_reg {
	struct ckc_backup_sts	pll[MAX_TCC_PLL];
	struct ckc_backup_sts	clk[FBUS_MAX];
	struct ckc_backup_sts	peri[PERI_MAX];
	unsigned long	ddi_sub[DDIBUS_MAX];
	unsigned long	io_sub[IOBUS_MAX];
	unsigned long	hsio_sub[HSIOBUS_MAX];
	unsigned long	video_sub[VIDEOBUS_MAX];
};

static struct ckc_backup_reg *ckc_backup = NULL;
void tcc_ckc_save(unsigned int clk_down)
{
	struct arm_smccc_res res;
	int i, j;

	ckc_backup = kzalloc(sizeof(struct ckc_backup_reg), GFP_KERNEL);

	for (i = 0; i < PERI_MAX; i++) {
		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->peri[i].rate = res.a0;
		arm_smccc_smc(SIP_CLK_IS_PERI, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->peri[i].en = res.a0;

		if (clk_down) {
			arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, i, ckc_backup->peri[i].en, XIN_CLK_RATE, 0, 0, 0, 0, &res);
		}
	}

	for (i = 0; i < FBUS_MAX; i++) {
		arm_smccc_smc(SIP_CLK_GET_CLKCTRL, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->clk[i].rate = res.a0;
		arm_smccc_smc(SIP_CLK_IS_CLKCTRL, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->clk[i].en = res.a0;

		if (ckc_backup->clk[i].en == 1) {
			switch(i) {
			case FBUS_VBUS:
				for (j = 0; j < VIDEOBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_VPUBUS, j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->video_sub[j] = res.a0;
				}
				break;
			case FBUS_HSIO:
				for (j = 0; j < HSIOBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_HSIOBUS, j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->hsio_sub[j] = res.a0;
				}
				break;
			case FBUS_DDI:
				for (j = 0; j < DDIBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_DDIBUS, j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->ddi_sub[j] = res.a0;
				}
				break;
			case FBUS_IO:
				for (j = 0; j < IOBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_IOBUS, j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->io_sub[j] = res.a0;
				}
				break;
			}
		}
	}

	for (i = 0; i < MAX_TCC_PLL; i++) {
		arm_smccc_smc(SIP_CLK_GET_PLL, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->pll[i].rate = res.a0;
	}
}

void tcc_ckc_restore(void)
{
	struct arm_smccc_res res;
	int i, j;

	arm_smccc_smc(SIP_CLK_INIT, 0, 0, 0, 0, 0, 0, 0, &res);

	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_IO, 1, XIN_CLK_RATE/2, 0, 0, 0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_SMU, 1, XIN_CLK_RATE/2, 0, 0, 0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_HSIO, 1, XIN_CLK_RATE/2, 0, 0, 0, 0, &res);

	for (i = 0; i < MAX_TCC_PLL; i++) {
		if (i == MEMBUS_PLL)
			continue;
		arm_smccc_smc(SIP_CLK_SET_PLL, i, ckc_backup->pll[i].rate, 2, 0, 0, 0, 0, &res);
	}

	for (i = 0; i < FBUS_MAX; i++) {
		if (i == FBUS_MEM || i == FBUS_MEM_SUB || FBUS_MEM_PHY_USER || FBUS_MEM_PHY_PERI)
			continue;

		arm_smccc_smc(SIP_CLK_SET_CLKCTRL, i, ckc_backup->clk[i].en, ckc_backup->clk[i].rate, 0, 0, 0, 0, &res);

		if (ckc_backup->clk[i].en == 1) {
			switch(i) {
			case FBUS_VBUS:
				for (j = 0; j < VIDEOBUS_MAX; j++) {
					if (ckc_backup->video_sub[j] == 1) {
						arm_smccc_smc(SIP_CLK_DISABLE_VPUBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}
					else {
						arm_smccc_smc(SIP_CLK_ENABLE_VPUBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}
				}
				break;
			case FBUS_HSIO:
				for (j = 0; j < HSIOBUS_MAX; j++) {
					if (ckc_backup->hsio_sub[j] == 1) {
						arm_smccc_smc(SIP_CLK_DISABLE_HSIOBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}
					else {
						arm_smccc_smc(SIP_CLK_ENABLE_HSIOBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}

				}
				break;
			case FBUS_DDI:
				for (j = 0; j < DDIBUS_MAX; j++) {
					if (ckc_backup->ddi_sub[j] == 1) {
						arm_smccc_smc(SIP_CLK_DISABLE_DDIBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}
					else {
						arm_smccc_smc(SIP_CLK_ENABLE_DDIBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}

				}
				break;
			case FBUS_IO:
				for (j = 0; j < IOBUS_MAX; j++) {
					if (ckc_backup->io_sub[j] == 1) {
						arm_smccc_smc(SIP_CLK_DISABLE_IOBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}
					else {
						arm_smccc_smc(SIP_CLK_ENABLE_IOBUS, j, 0, 0, 0, 0, 0, 0, &res);
					}

				}
				break;
			}
		}
	}

	for ( i = 0; i < PERI_MAX; i++) {
		arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, i, ckc_backup->peri[i].en, ckc_backup->peri[i].rate, 0, 0, 0, 0, &res);
	}

	kfree(ckc_backup);
	ckc_backup = NULL;
}

static void *tcc_clk_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *tcc_clk_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void tcc_clk_stop(struct seq_file *m, void *v)
{
}

static int tcc_clk_show(struct seq_file *m, void *v)
{
	struct arm_smccc_res res;
	unsigned int rate, enabled;

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CPU0, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CPU0, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "cpu(CA72) : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CPU1, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CPU1, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "cpu(CA53) : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CMBUS, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CMBUS, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "CM BUS     : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CBUS, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CBUS, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "CPU BUS     : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_MEM, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_MEM, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "MEM BUS  : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	/*
	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_MEM_SUB, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_MEM_SUB, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "MEM Sub System  : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_MEM_PHY_USER, 0, 0, 0, 0, 0, 0, &res);
	// 2 divide cause memory phy using divide clock.
	rate = res.a0 / 2;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_MEM_PHY_USER, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "MEM PHY User  : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_MEM_PHY_PERI, 0, 0, 0, 0, 0, 0, &res);
	// 2 divide cause memory phy using divide clock.
	rate = res.a0 / 2;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_MEM_PHY_PERI, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "mem phy peri  : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");
	*/

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_SMU, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_SMU, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "SMU BUS     : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_IO, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_IO, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "IO BUS     : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_HSIO, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_HSIO, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "HSIO BUS    : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_DDI, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_DDI, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "DISPLAY BUS(DDIBUS) : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_GPU, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_GPU, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "Graphic 3D BUS : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_G2D, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_G2D, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "Graphic 2D BUS : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_VBUS, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_VBUS, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "Video BUS   : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CODA, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CODA, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "codec(CODA) : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "hevc_c_dec   : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_BHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_BHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "hevc_b_dec   : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_CHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_CHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "hevc_c_enc   : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL, FBUS_BHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL, FBUS_BHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(m, "hevc_b_enc  : %10lu Hz %s\n", rate, enabled == 1?"":"(disabled)");

	return 0;

}

static const struct seq_operations tcc_clk_op = {
	.start	= tcc_clk_start,
	.next	= tcc_clk_next,
	.stop	= tcc_clk_stop,
	.show	= tcc_clk_show
};

static int tcc_clk_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &tcc_clk_op);
}

static const struct file_operations proc_tcc_clk_operations = {
	.open		= tcc_clk_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int tcc_clk_suspend(void) {
	tcc_ckc_save(0);
	return 0;
}

static void tcc_clk_resume(void) {
	tcc_ckc_restore();
}

static struct syscore_ops tcc_clk_syscore_ops = {
	.suspend	= tcc_clk_suspend,
	.resume		= tcc_clk_resume,
};

static int __init tcc_clk_proc_init(void)
{
	proc_create("clocks", 0, NULL, &proc_tcc_clk_operations);
	register_syscore_ops(&tcc_clk_syscore_ops);
	return 0;
}

__initcall(tcc_clk_proc_init);
