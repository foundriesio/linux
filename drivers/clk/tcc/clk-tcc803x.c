// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clocksource.h>
//#include <linux/of_address.h>
#include <linux/clk/tcc.h>
//#include <linux/clk-provider.h>
//#include <linux/irqflags.h>
#include <linux/slab.h>
#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/system_info.h>
#endif

#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

#define TCC803X_CKC_DRIVER
#include "clk-tcc803x.h"


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
	unsigned int	isoip_top[ISOIP_TOP_MAX];
};

static struct ckc_backup_reg *ckc_backup;
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
			arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, i,
				  ckc_backup->peri[i].en, XIN_CLK_RATE, 0,
				  0, 0, 0, &res);
		}
	}

	for (i = 0; i < FBUS_MAX; i++) {
		arm_smccc_smc(SIP_CLK_GET_CLKCTRL, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->clk[i].rate = res.a0;
		arm_smccc_smc(SIP_CLK_IS_CLKCTRL, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->clk[i].en = res.a0;

		if (ckc_backup->clk[i].en == 1) {
			switch (i) {
			case FBUS_VBUS:
				for (j = 0; j < VIDEOBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_VPUBUS, j, 0,
						      0, 0, 0, 0, 0, &res);
					ckc_backup->video_sub[j] = res.a0;
				}
				break;
			case FBUS_HSIO:
				for (j = 0; j < HSIOBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_HSIOBUS, j, 0,
						      0, 0, 0, 0, 0, &res);
					ckc_backup->hsio_sub[j] = res.a0;
				}
				break;
			case FBUS_DDI:
				for (j = 0; j < DDIBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_DDIBUS, j, 0,
						      0, 0, 0, 0, 0, &res);
					ckc_backup->ddi_sub[j] = res.a0;
				}
				break;
			case FBUS_IO:
				for (j = 0; j < IOBUS_MAX; j++) {
					arm_smccc_smc(SIP_CLK_IS_IOBUS, j, 0,
						      0, 0, 0, 0, 0, &res);
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

	for (i = 0; i < ISOIP_TOP_MAX; i++) {
		arm_smccc_smc(SIP_CLK_IS_ISOTOP, i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->isoip_top[i] = res.a0;
	}
}

void tcc_ckc_restore(void)
{
	struct arm_smccc_res res;
	int i, j;

	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_IO, 1, XIN_CLK_RATE/2, 0, 0,
		      0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_SMU, 1, XIN_CLK_RATE/2, 0, 0,
		      0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_HSIO, 1, XIN_CLK_RATE/2, 0, 0,
		      0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, FBUS_CMBUS, 1, XIN_CLK_RATE/2, 0,
		      0, 0, 0, &res);

	for (i = 0; i < ISOIP_TOP_MAX; i++) {
		if (ckc_backup->isoip_top[i] == 1) {
			arm_smccc_smc(SIP_CLK_DISABLE_ISOTOP, i, 0, 0, 0, 0, 0,
				      0, &res);
		} else {
			arm_smccc_smc(SIP_CLK_ENABLE_ISOTOP, i, 0, 0, 0, 0, 0,
				      0, &res);
		}
	}

	for (i = 0; i < MAX_TCC_PLL; i++) {
		if (i == MEMBUS_PLL)
			continue;
		arm_smccc_smc(SIP_CLK_SET_PLL, i, ckc_backup->pll[i].rate, 2, 0,
			      0, 0, 0, &res);
	}

	for (i = 0; i < FBUS_MAX; i++) {
		if (i == FBUS_MEM || i == FBUS_MEM_PHY)
			continue;

		arm_smccc_smc(SIP_CLK_SET_CLKCTRL, i, ckc_backup->clk[i].en,
			      ckc_backup->clk[i].rate, 0, 0, 0, 0, &res);

		if (ckc_backup->clk[i].en == 1) {
			switch (i) {
			case FBUS_VBUS:
				for (j = 0; j < VIDEOBUS_MAX; j++) {
					if (ckc_backup->video_sub[j] == 1) {
						arm_smccc_smc
						    (SIP_CLK_DISABLE_VPUBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					} else {
						arm_smccc_smc
						    (SIP_CLK_ENABLE_VPUBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					}
				}
				break;
			case FBUS_HSIO:
				for (j = 0; j < HSIOBUS_MAX; j++) {
					if (ckc_backup->hsio_sub[j] == 1) {
						arm_smccc_smc
						    (SIP_CLK_DISABLE_HSIOBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					} else {
						arm_smccc_smc
						    (SIP_CLK_ENABLE_HSIOBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					}

				}
				break;
			case FBUS_DDI:
				for (j = 0; j < DDIBUS_MAX; j++) {
					if (ckc_backup->ddi_sub[j] == 1) {
						arm_smccc_smc
						    (SIP_CLK_DISABLE_DDIBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					} else {
						arm_smccc_smc
						    (SIP_CLK_ENABLE_DDIBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					}

				}
				break;
			case FBUS_IO:
				for (j = 0; j < IOBUS_MAX; j++) {
					if (ckc_backup->io_sub[j] == 1) {
						arm_smccc_smc
						    (SIP_CLK_DISABLE_IOBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					} else {
						arm_smccc_smc
						    (SIP_CLK_ENABLE_IOBUS, j,
						     0, 0, 0, 0, 0, 0, &res);
					}

				}
				break;
			}
		}
	}

	for (i = 0; i < PERI_MAX; i++) {
		arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, i, ckc_backup->peri[i].en,
			      ckc_backup->peri[i].rate, 0, 0, 0, 0, &res);
	}

	kfree(ckc_backup);
	ckc_backup = NULL;
}

static int __init tcc_clk_proc_init(void)
{
	proc_symlink("clocks", NULL, "/sys/kernel/debug/clk/clk_summary");
	return 0;
}
device_initcall(tcc_clk_proc_init);
