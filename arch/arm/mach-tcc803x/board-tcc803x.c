/*
 * Telechips TCC803x Flattened Device Tree enabled machine
 *
 * Copyright (c) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <linux/kernel.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl080.h>
#include <linux/amba/pl08x.h>
#include <linux/amba/serial.h>
#include <linux/of.h>

#define TCC803X_PA_VIOC		0x12000000
#define TCC803X_PA_I2C0		0x16300000
#define TCC803X_PA_I2C1		0x16310000
#define TCC803X_PA_I2C2		0x16320000
#define TCC803X_PA_I2C3		0x16330000

#define TCC803X_PA_NFC		0x16400000
#define TCC_PA_EHCI0		0x11200000
#define TCC_PA_EHCI1		0x11000000
#define TCC_PA_OHCI0		0x11100000
#define TCC_PA_OHCI1		0x11040000
#define TCC_PA_DWC_OTG		0x11080000

#define TCC803X_PA_SDHC0	0x16440000
#define TCC803X_PA_SDHC1	0x16450000
#define TCC803X_PA_SDHC2	0x16460000

extern void __init tcc_map_common_io(void);

static struct of_dev_auxdata tcc803x_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("telechips,vioc-fb", TCC803X_PA_VIOC, "tccfb", NULL),
	OF_DEV_AUXDATA("telechips,nand-v8", TCC803X_PA_NFC,"tcc_nand", NULL),
	OF_DEV_AUXDATA("telechips,tcc-ehci", TCC_PA_EHCI0, "tcc-ehci.0", NULL),
	OF_DEV_AUXDATA("telechips,tcc-ehci", TCC_PA_EHCI1, "tcc-ehci.1", NULL),
	OF_DEV_AUXDATA("telechips,tcc-ohci", TCC_PA_OHCI0, "tcc-ohci.0", NULL),
	OF_DEV_AUXDATA("telechips,tcc-ohci", TCC_PA_OHCI1, "tcc-ohci.1", NULL),
	OF_DEV_AUXDATA("telechips,dwc_otg", TCC_PA_DWC_OTG,"dwc_otg", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC803X_PA_I2C0, "i2c.0", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC803X_PA_I2C1, "i2c.1", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC803X_PA_I2C2, "i2c.2", NULL),
	OF_DEV_AUXDATA("telechips,i2c", TCC803X_PA_I2C3, "i2c.3", NULL),
	OF_DEV_AUXDATA("telechips,tcc-sdhc", TCC803X_PA_SDHC0, "tcc-sdhc.0", NULL),
	OF_DEV_AUXDATA("telechips,tcc-sdhc", TCC803X_PA_SDHC1, "tcc-sdhc.1", NULL),
	OF_DEV_AUXDATA("telechips,tcc-sdhc", TCC803X_PA_SDHC2, "tcc-sdhc.2", NULL),
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

static void __init tcc803x_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			     tcc803x_auxdata_lookup, NULL);
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc803x_dt_compat[] __initconst = {
	"telechips,tcc803x",
	NULL
};

DT_MACHINE_START(TCC898X_DT, "Telechips TCC803x (Flattened Device Tree)")
	//.map_io		= tcc_map_common_io,
	.init_machine	= tcc803x_dt_init,
	.dt_compat	= tcc803x_dt_compat,
MACHINE_END
