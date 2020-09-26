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
#include <asm/mach/arch.h>
#include <asm/system_info.h>
#include <linux/kernel.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl080.h>
#include <linux/amba/pl08x.h>
#include <linux/amba/serial.h>
#include <linux/of.h>
#include "pl08x.h"

#define TCC_PA_VIOC	0x12000000
#define TCC_PA_I2C0	0x16300000
#define TCC_PA_I2C1	0x16310000
#define TCC_PA_I2C2	0x16320000
#define TCC_PA_I2C3	0x16330000

#define TCC_PA_DWC3	0x11B00000
#define TCC_PA_EHCI	0x11A00000
#define TCC_PA_OHCI	0x11A80000
#define TCC_PA_EHCI_MUX	0x11900000
#define TCC_PA_OHCI_MUX	0x11940000
#define TCC_PA_DWC_OTG	0x11980000

/* Wrapper of 'OF_DEV_AUXDATA' to parenthesize arguments */
#define OF_DEV_AUXDATA_P(_compat, _phys, _name) \
	OF_DEV_AUXDATA((_compat), (_phys), (_name), NULL)

static struct of_dev_auxdata tcc899x_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA_P("telechips,vioc-fb", TCC_PA_VIOC, "tccfb"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C0, "i2c.0"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C1, "i2c.1"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C2, "i2c.2"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C3, "i2c.3"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 0, "wmixer0"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 1, "wmixer1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 1, "scaler1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 3, "scaler3"),
#ifdef CONFIG_TCC_ATTACH
	OF_DEV_AUXDATA_P("telechips,attach_drv", 0, "attach0"),
	OF_DEV_AUXDATA_P("telechips,attach_drv", 1, "attach1"),
#endif
	OF_DEV_AUXDATA_P("telechips,tcc_wdma", 0, "wdma"),
	OF_DEV_AUXDATA_P("telechips,tcc_overlay", 0, "overlay"),
	{},
};

static void __init tcc899x_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			     tcc899x_auxdata_lookup, NULL);
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc899x_dt_compat[] __initconst = {
	"telechips,tcc899x",
	NULL
};

DT_MACHINE_START(TCC899X_DT, "Telechips TCC899x (Flattened Device Tree)")
	.init_machine	= tcc899x_dt_init,
	.dt_compat	= tcc899x_dt_compat,
MACHINE_END
