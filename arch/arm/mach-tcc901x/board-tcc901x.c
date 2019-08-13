/*
 * Telechips TCC901x Flattened Device Tree enabled machine
 *
 * Copyright (c) 2019 Telechips Co., Ltd.
 *		http://www.telechips.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <asm/system_info.h>
#include <linux/kernel.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl080.h>
#include <linux/amba/pl08x.h>
#include <linux/amba/serial.h>
#include <linux/of.h>
#include "pl08x.h"

#define TCC901X_PA_VIOC		0x12000000
#define TCC901X_PA_I2C0		0x16300000
#define TCC901X_PA_I2C1		0x16310000
#define TCC901X_PA_I2C2		0x16320000
#define TCC901X_PA_I2C3		0x16330000

#define TCC901X_PA_DWC3		0x11B00000
#define TCC901X_PA_EHCI		0x11A00000
#define TCC901X_PA_OHCI		0x11A80000
#define TCC901X_PA_EHCI_MUX	0x11900000
#define TCC901X_PA_OHCI_MUX	0x11940000
#define TCC901X_PA_DWC_OTG	0x11980000

extern void __init tcc_map_common_io(void);

static struct of_dev_auxdata tcc901x_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("telechips,vioc-fb", TCC901X_PA_VIOC, "tccfb", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC901X_PA_I2C0, "i2c.0", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC901X_PA_I2C1, "i2c.1", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC901X_PA_I2C2, "i2c.2", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC901X_PA_I2C3, "i2c.3", NULL),
	OF_DEV_AUXDATA("telechips,wmixer_drv", 0, "wmixer0", NULL),
	OF_DEV_AUXDATA("telechips,wmixer_drv", 1, "wmixer1", NULL),
	OF_DEV_AUXDATA("telechips,scaler_drv", 1, "scaler1", NULL),
	OF_DEV_AUXDATA("telechips,scaler_drv", 3, "scaler3", NULL),
#ifdef CONFIG_TCC_ATTACH
	OF_DEV_AUXDATA("telechips,attach_drv", 0, "attach0", NULL),
	OF_DEV_AUXDATA("telechips,attach_drv", 1, "attach1", NULL),
#endif
	OF_DEV_AUXDATA("telechips,tcc_wdma", 0, "wdma", NULL),
	OF_DEV_AUXDATA("telechips,tcc_overlay", 0, "overlay", NULL),
	{},
};

static void __init tcc901x_dt_init(void)
{
	#if 0
	void __iomem *reg;
	unsigned int chip_rev;

	/* TODO: get chip_rev from OTP rom */
	reg = ioremap(0xE0003000, 0x1000);
	chip_rev = (unsigned int)readl_relaxed(reg+0xC10);
	iounmap(reg);
	if (chip_rev & (0xF<<8)) {
		system_rev = (chip_rev>>8)&0xF;
		system_rev += 1;
	}
	else {
		reg = ioremap(0xF4000000, 0x1000);
		chip_rev = (unsigned int)readl_relaxed(reg+0x24);
		if (chip_rev == 0x17072800)
			system_rev = 0x0000;
		else
			system_rev = 0xFFFF;
		iounmap(reg);
	}
	#endif
	of_platform_populate(NULL, of_default_bus_match_table,
			     tcc901x_auxdata_lookup, NULL);
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc901x_dt_compat[] __initconst = {
	"telechips,tcc901x",
	NULL
};

DT_MACHINE_START(TCC901X_DT, "Telechips TCC901x (Flattened Device Tree)")
	.init_machine	= tcc901x_dt_init,
	.dt_compat	= tcc901x_dt_compat,
MACHINE_END
