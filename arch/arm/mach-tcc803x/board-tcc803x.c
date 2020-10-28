// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <linux/kernel.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl080.h>
#include <linux/amba/pl08x.h>
#include <linux/amba/serial.h>
#include <linux/of.h>

#define TCC_PA_VIOC	0x12000000
#define TCC_PA_I2C0	0x16300000
#define TCC_PA_I2C1	0x16310000
#define TCC_PA_I2C2	0x16320000
#define TCC_PA_I2C3	0x16330000

#define TCC_PA_NFC	0x16400000
#define TCC_PA_EHCI0	0x11200000
#define TCC_PA_EHCI1	0x11000000
#define TCC_PA_OHCI0	0x11100000
#define TCC_PA_OHCI1	0x11040000
#define TCC_PA_DWC_OTG	0x11080000

#define TCC_PA_SDHC0	0x16440000
#define TCC_PA_SDHC1	0x16450000
#define TCC_PA_SDHC2	0x16460000

/* Wrapper of 'OF_DEV_AUXDATA' to parenthesize arguments */
#define OF_DEV_AUXDATA_P(_compat, _phys, _name) \
	OF_DEV_AUXDATA((_compat), (_phys), (_name), NULL)

static struct of_dev_auxdata tcc803x_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA_P("telechips,vioc-fb", TCC_PA_VIOC, "tccfb"),
	OF_DEV_AUXDATA_P("telechips,nand-v8", TCC_PA_NFC, "tcc_nand"),
	OF_DEV_AUXDATA_P("telechips,tcc-ehci", TCC_PA_EHCI0, "tcc-ehci.0"),
	OF_DEV_AUXDATA_P("telechips,tcc-ehci", TCC_PA_EHCI1, "tcc-ehci.1"),
	OF_DEV_AUXDATA_P("telechips,tcc-ohci", TCC_PA_OHCI0, "tcc-ohci.0"),
	OF_DEV_AUXDATA_P("telechips,tcc-ohci", TCC_PA_OHCI1, "tcc-ohci.1"),
	OF_DEV_AUXDATA_P("telechips,dwc_otg", TCC_PA_DWC_OTG, "dwc_otg"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C0, "i2c.0"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C1, "i2c.1"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C2, "i2c.2"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C3, "i2c.3"),
	OF_DEV_AUXDATA_P("telechips,tcc-sdhc", TCC_PA_SDHC0, "tcc-sdhc.0"),
	OF_DEV_AUXDATA_P("telechips,tcc-sdhc", TCC_PA_SDHC1, "tcc-sdhc.1"),
	OF_DEV_AUXDATA_P("telechips,tcc-sdhc", TCC_PA_SDHC2, "tcc-sdhc.2"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 0, "wmixer0"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 1, "wmixer1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 1, "scaler1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 3, "scaler3"),
#if defined(CONFIG_TCC_ATTACH)
	OF_DEV_AUXDATA_P("telechips,attach_drv", 0, "attach0"),
	OF_DEV_AUXDATA_P("telechips,attach_drv", 1, "attach1"),
#endif
	OF_DEV_AUXDATA_P("telechips,tcc_wdma", 0, "wdma"),
	OF_DEV_AUXDATA_P("telechips,tcc_overlay", 0, "overlay"),
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
	.init_machine	= tcc803x_dt_init,
	.dt_compat	= tcc803x_dt_compat,
MACHINE_END
