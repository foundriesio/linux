/*
 * Telechips TCC893x Flattened Device Tree enabled machine
 *
 * Copyright (c) 2014 Telechips Co., Ltd.
 *		http://www.telechips.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/serial_core.h>
#include <asm/mach/arch.h>
#include <plat/platsmp.h>
#include <mach/iomap.h>

#include "common.h"

/* Wrapper of 'OF_DEV_AUXDATA' to parenthesize arguments */
#define OF_DEV_AUXDATA_P(_compat, _phys, _name) \
	OF_DEV_AUXDATA((_compat), (_phys), (_name), NULL)

static struct of_dev_auxdata tcc897x_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA_P("telechips,vioc-fb", TCC_PA_VIOC, "tccfb"),
	OF_DEV_AUXDATA_P("telechips,tcc897x-sdhc", TCC_PA_SDMMC0, "tcc-sdhc.0"),
	OF_DEV_AUXDATA_P("telechips,tcc897x-sdhc", TCC_PA_SDMMC2, "tcc-sdhc.2"),
	OF_DEV_AUXDATA_P("telechips,tcc897x-sdhc", TCC_PA_SDMMC3, "tcc-sdhc.3"),
	OF_DEV_AUXDATA_P("telechips,nand-v8", TCC_PA_NFC, "tcc_nand"),
	OF_DEV_AUXDATA_P("telechips,tcc-ehci", TCC_PA_EHCI, "tcc-ehci"),
	OF_DEV_AUXDATA_P("telechips,tcc-ohci", TCC_PA_OHCI, "tcc-ohci"),
	OF_DEV_AUXDATA_P("telechips,dwc_otg", TCC_PA_DWC_OTG, "dwc_otg"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C0, "i2c.0"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C1, "i2c.1"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C2, "i2c.2"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C3, "i2c.3"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 0, "wmixer0"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 1, "wmixer1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 1, "scaler1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 3, "scaler3"),
	OF_DEV_AUXDATA_P("telechips,tcc_wdma", 0, "wdma"),
	OF_DEV_AUXDATA_P("telechips,tcc_overlay", 0, "overlay"),
	{},
};

static void __init tcc897x_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			tcc897x_auxdata_lookup, NULL);
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc897x_dt_compat[] __initconst = {
	"telechips,tcc897x",
	NULL
};

DT_MACHINE_START(TCC897X_DT, "Telechips TCC897x (Flattened Device Tree)")
	.init_machine	= tcc897x_dt_init,
	.smp		= smp_ops(tcc_smp_ops),
	.smp_init	= smp_init_ops(tcc_smp_init_ops),
	.map_io		= tcc_map_common_io,
	.dt_compat	= tcc897x_dt_compat,
	.reserve	= tcc_mem_reserve,
MACHINE_END
