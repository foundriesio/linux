/*
 * arch/arm/mach-tegra/board-colibri_t30-power.c
 *
 * Copyright (C) 2012-2016 Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <asm/mach-types.h>

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/mfd/tps6591x.h>
#include <linux/platform_device.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps62360.h>
#include <linux/regulator/tps6591x-regulator.h>
#include <linux/resource.h>

#include <mach/edp.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>

#include "board-colibri_t30.h"
#include "board.h"
#include "gpio-names.h"
#include "tegra3_tsensor.h"
#include "pm.h"
#include "wakeups.h"
#include "wakeups-t3.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

/* SW1: +V1.35_VDDIO_DDR */
static struct regulator_consumer_supply tps6591x_vdd1_supply_0[] = {
	REGULATOR_SUPPLY("mem_vddio_ddr", NULL),
	REGULATOR_SUPPLY("t30_vddio_ddr", NULL),
};

/* SW2: unused */
static struct regulator_consumer_supply tps6591x_vdd2_supply_0[] = {
	REGULATOR_SUPPLY("unused_rail_vdd2", NULL),
};

/* SW CTRL: +V1.0_VDD_CPU */
static struct regulator_consumer_supply tps6591x_vddctrl_supply_0[] = {
	REGULATOR_SUPPLY("vdd_cpu_pmu", NULL),
	REGULATOR_SUPPLY("vdd_cpu", NULL),
//!=vddio_sys!
	REGULATOR_SUPPLY("vdd_sys", NULL),
};

/* SWIO: +V1.8 */
static struct regulator_consumer_supply tps6591x_vio_supply_0[] = {
	REGULATOR_SUPPLY("vdd_gen1v8", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", NULL),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vdd1v8_satelite", NULL),
	REGULATOR_SUPPLY("vddio_vi", NULL),
	REGULATOR_SUPPLY("pwrdet_vi", NULL),
	REGULATOR_SUPPLY("ldo1", NULL),
	REGULATOR_SUPPLY("ldo2", NULL),
	REGULATOR_SUPPLY("ldo6", NULL),
	REGULATOR_SUPPLY("ldo7", NULL),
	REGULATOR_SUPPLY("ldo8", NULL),
	REGULATOR_SUPPLY("vcore_audio", NULL),
	REGULATOR_SUPPLY("avcore_audio", NULL),
	REGULATOR_SUPPLY("vcore1_lpddr2", NULL),
	REGULATOR_SUPPLY("vcom_1v8", NULL),
	REGULATOR_SUPPLY("pmuio_1v8", NULL),
	REGULATOR_SUPPLY("avdd_ic_usb", NULL),
};

/* unused */
static struct regulator_consumer_supply tps6591x_ldo1_supply_0[] = {
	REGULATOR_SUPPLY("unused_rail_ldo1", NULL),
};

/* EN_+V3.3 switching via FET: +V3.3_AUDIO_AVDD_S, +V3.3 and +V1.8_VDD_LAN
   see also v3_3 fixed supply */
static struct regulator_consumer_supply tps6591x_ldo2_supply_0[] = {
	REGULATOR_SUPPLY("en_V3_3", NULL),
};

/* unused */
static struct regulator_consumer_supply tps6591x_ldo3_supply_0[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", NULL),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

/* +V1.2_VDD_RTC */
static struct regulator_consumer_supply tps6591x_ldo4_supply_0[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};

/* +V2.8_AVDD_VDAC */
//only required for analog RGB
static struct regulator_consumer_supply tps6591x_ldo5_supply_0[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
};

/* +V1.05_AVDD_PLLE */
static struct regulator_consumer_supply tps6591x_ldo6_supply_0[] = {
	REGULATOR_SUPPLY("avdd_plle", NULL),
};

/* +V1.2_AVDD_PLL */
static struct regulator_consumer_supply tps6591x_ldo7_supply_0[] = {
	REGULATOR_SUPPLY("avdd_plla_p_c_s", NULL),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu_d", NULL),
	REGULATOR_SUPPLY("avdd_pllu_d2", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
};

/* +V1.0_VDD_DDR_HS */
static struct regulator_consumer_supply tps6591x_ldo8_supply_0[] = {
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
};

#define TPS_PDATA_INIT(_name, _sname, _minmv, _maxmv, _supply_reg, _always_on, \
	_boot_on, _apply_uv, _init_uV, _init_enable, _init_apply, _ectrl, _flags) \
	static struct tps6591x_regulator_platform_data pdata_##_name##_##_sname = \
	{								\
		.regulator = {						\
			.constraints = {				\
				.min_uV = (_minmv)*1000,		\
				.max_uV = (_maxmv)*1000,		\
				.valid_modes_mask = (REGULATOR_MODE_NORMAL |  \
						     REGULATOR_MODE_STANDBY), \
				.valid_ops_mask = (REGULATOR_CHANGE_MODE |    \
						   REGULATOR_CHANGE_STATUS |  \
						   REGULATOR_CHANGE_VOLTAGE), \
				.always_on = _always_on,		\
				.boot_on = _boot_on,			\
				.apply_uV = _apply_uv,			\
			},						\
			.num_consumer_supplies =			\
				ARRAY_SIZE(tps6591x_##_name##_supply_##_sname),	\
			.consumer_supplies = tps6591x_##_name##_supply_##_sname,	\
			.supply_regulator = _supply_reg,		\
		},							\
		.init_uV =  _init_uV * 1000,				\
		.init_enable = _init_enable,				\
		.init_apply = _init_apply,				\
		.ectrl = _ectrl,					\
		.flags = _flags,					\
	}

TPS_PDATA_INIT(vdd1, 0,         1350, 1350, 0, 1, 1, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(vdd2, 0,         1050, 1050, 0, 0, 1, 0, -1, 0, 0, EXT_CTRL_SLEEP_OFF, 0);
TPS_PDATA_INIT(vddctrl, 0,      800,  1300, 0, 1, 1, 0, -1, 0, 0, EXT_CTRL_EN1, 0);
TPS_PDATA_INIT(vio,  0,         1800, 1800, 0, 1, 1, 0, -1, 0, 0, 0, 0);

TPS_PDATA_INIT(ldo1, 0,         1000, 3300, tps6591x_rails(VIO), 0, 0, 0, -1, 0, 1, 0, 0);
/* Make sure EN_+V3.3 is always on! */
TPS_PDATA_INIT(ldo2, 0,         1200, 1200, tps6591x_rails(VIO), 1, 1, 1, -1, 0, 1, 0, 0);

TPS_PDATA_INIT(ldo3, 0,         1200, 1200, 0, 0, 0, 0, -1, 0, 0, 0, 0);
TPS_PDATA_INIT(ldo4, 0,         900,  1400, 0, 1, 0, 0, -1, 0, 0, 0, LDO_LOW_POWER_ON_SUSPEND);
TPS_PDATA_INIT(ldo5, 0,         2800, 2800, 0, 0, 0, 0, -1, 0, 0, 0, 0);
/* AVDD_PLLE should be 1.05V, but ldo_6 can not be adjusted in a 50mV granularity */
TPS_PDATA_INIT(ldo6, 0,         1000, 1100, tps6591x_rails(VIO), 0, 0, 1, -1, 0, 0, 0, 0);

TPS_PDATA_INIT(ldo7, 0,         1200, 1200, tps6591x_rails(VIO), 1, 1, 1, -1, 0, 0, EXT_CTRL_SLEEP_OFF, LDO_LOW_POWER_ON_SUSPEND);
TPS_PDATA_INIT(ldo8, 0,         1000, 1000, tps6591x_rails(VIO), 1, 0, 0, -1, 0, 0, EXT_CTRL_SLEEP_OFF, LDO_LOW_POWER_ON_SUSPEND);

#if defined(CONFIG_RTC_DRV_TPS6591x)
static struct tps6591x_rtc_platform_data rtc_data = {
	.irq = TPS6591X_IRQ_BASE + TPS6591X_INT_RTC_ALARM,
	.time = {
		.tm_year = 2000,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 0,
		.tm_min = 0,
		.tm_sec = 0,
	},
};

#define TPS_RTC_REG()					\
	{						\
		.id	= 0,				\
		.name	= "rtc_tps6591x",		\
		.platform_data = &rtc_data,		\
	}
#endif

#define TPS_REG(_id, _name, _sname)				\
	{							\
		.id	= TPS6591X_ID_##_id,			\
		.name	= "tps6591x-regulator",			\
		.platform_data	= &pdata_##_name##_##_sname,	\
	}

static struct tps6591x_subdev_info colibri_t30_tps_devs[] = {
	TPS_REG(VDD_1, vdd1, 0),
	TPS_REG(VDD_2, vdd2, 0),
	TPS_REG(VDDCTRL, vddctrl, 0),
	TPS_REG(VIO, vio, 0),
	TPS_REG(LDO_1, ldo1, 0),
	TPS_REG(LDO_2, ldo2, 0),
	TPS_REG(LDO_3, ldo3, 0),
	TPS_REG(LDO_4, ldo4, 0),
	TPS_REG(LDO_5, ldo5, 0),
	TPS_REG(LDO_6, ldo6, 0),
	TPS_REG(LDO_7, ldo7, 0),
	TPS_REG(LDO_8, ldo8, 0),
#if defined(CONFIG_RTC_DRV_TPS6591x)
	TPS_RTC_REG(),
#endif
};

#define TPS_GPIO_INIT_PDATA(gpio_nr, _init_apply, _sleep_en, _pulldn_en, _output_en, _output_val)	\
	[gpio_nr] = {					\
			.sleep_en	= _sleep_en,	\
			.pulldn_en	= _pulldn_en,	\
			.output_mode_en	= _output_en,	\
			.output_val	= _output_val,	\
			.init_apply	= _init_apply,	\
		     }
static struct tps6591x_gpio_init_data tps_gpio_pdata_colibri_t30[] =  {
	TPS_GPIO_INIT_PDATA(0, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(1, 1, 0, 0, 1, 0), /* EN_CORE_DVFS_N */
	TPS_GPIO_INIT_PDATA(2, 1, 1, 0, 1, 1), /* EN_VDD_CORE */
	TPS_GPIO_INIT_PDATA(3, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(4, 1, 0, 0, 0, 0), /* EN_VDD_FUSE# */
	TPS_GPIO_INIT_PDATA(5, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(6, 1, 1, 0, 1, 0), /* EN_VDD_HDMI */
	TPS_GPIO_INIT_PDATA(7, 0, 0, 0, 0, 0),
	TPS_GPIO_INIT_PDATA(8, 0, 0, 0, 0, 0),
};

static struct tps6591x_sleep_keepon_data tps_slp_keepon = {
	.clkout32k_keepon = 1,
};

static struct tps6591x_platform_data tps_platform = {
	.irq_base	= TPS6591X_IRQ_BASE,
	.gpio_base	= TPS6591X_GPIO_BASE,
	.dev_slp_en	= true,
	.slp_keepon	= &tps_slp_keepon,
	.use_power_off	= true,
};

static struct i2c_board_info __initdata colibri_t30_regulators[] = {
	{
		I2C_BOARD_INFO("tps6591x", 0x2D),
//PWR_INT_IN wake18
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &tps_platform,
	},
};

/* TPS62362 DC-DC converter
   SW: +V1.2_VDD_CORE
   Note: Colibri T30 V1.0 have TPS62360 with different voltage levels at startup */
static struct regulator_consumer_supply tps6236x_dcdc_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct tps62360_regulator_platform_data tps6236x_pdata = {
	.reg_init_data = {					\
		.constraints = {				\
			.min_uV =  900000,			\
			.max_uV = 1400000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |  \
					     REGULATOR_MODE_STANDBY), \
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |    \
					   REGULATOR_CHANGE_STATUS |  \
					   REGULATOR_CHANGE_VOLTAGE), \
			.always_on = 1,				\
			.boot_on =  1,				\
			.apply_uV = 0,				\
		},						\
		.num_consumer_supplies = ARRAY_SIZE(tps6236x_dcdc_supply), \
		.consumer_supplies = tps6236x_dcdc_supply,	\
		},						\
	.en_discharge = true,					\
	.en_internal_pulldn = false,				\
	.vsel0_gpio = -1,					\
	.vsel1_gpio = -1,					\
	.vsel0_def_state = 0,					\
	.vsel1_def_state = 0,					\
};

static struct i2c_board_info __initdata tps6236x_boardinfo[] = {
	{
		I2C_BOARD_INFO("tps62360", 0x60),
		.platform_data	= &tps6236x_pdata,
	},
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG_OD(_id, _var, _name, _in_supply, _always_on,		\
		_boot_on, _gpio_nr, _active_high, _boot_state,		\
		_millivolts, _od_state)					\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
		.supply_regulator = _in_supply,				\
		.num_consumer_supplies =				\
			ARRAY_SIZE(fixed_reg_##_name##_supply),		\
		.consumer_supplies = fixed_reg_##_name##_supply,	\
		.constraints = {					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL),	\
			.valid_ops_mask = (REGULATOR_CHANGE_STATUS),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
		},							\
	};								\
	static struct fixed_voltage_config fixed_reg_##_var##_pdata =	\
	{								\
		.supply_name = FIXED_SUPPLY(_name),			\
		.microvolts = _millivolts * 1000,			\
		.gpio = _gpio_nr,					\
		.enable_high = _active_high,				\
		.enabled_at_boot = _boot_state,				\
		.init_data = &ri_data_##_var,				\
		.gpio_is_open_drain = _od_state,			\
	};								\
	static struct platform_device fixed_reg_##_var##_dev = {	\
		.name   = "reg-fixed-voltage",				\
		.id     = _id,						\
		.dev    = {						\
			.platform_data = &fixed_reg_##_var##_pdata,	\
		},							\
	}

#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
		 _gpio_nr, _active_high, _boot_state, _millivolts)	\
	FIXED_REG_OD(_id, _var, _name, _in_supply, _always_on, _boot_on,  \
		_gpio_nr, _active_high, _boot_state, _millivolts, false)

#define ADD_FIXED_REG(_name)	(&fixed_reg_##_name##_dev)

/* PMU GP6: EN_VDD_HDMI switching via FET: +V1.8_AVDD_HDMI_PLL and +V3.3_AVDD_HDMI */
static struct regulator_consumer_supply fixed_reg_en_hdmi_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
//	REGULATOR_SUPPLY("vdd_3v3_hdmi_cec", NULL),
};

/* EN_VDD_HDMI */
FIXED_REG(0, en_hdmi, en_hdmi, NULL, 0, 0, TPS6591X_GPIO_6, true, 1, 1800);

/* +V3.3 is switched on by LDO2, As this can not be modeled we use a fixed
   regulator without enable, 3.3V must not be switched off anyway.
+V3.3:
VDD_DDR_RX
VDDIO_LCD_1
VDDIO_LCD_2
VDDIO_CAM
LM95245
VDDIO_SYS_01
VDDIO_SYS_02
VDDIO_BB
VDDIO_AUDIO
VDDIO_GMI_1
VDDIO_GMI_2
VDDIO_GMI_3
VDDIO_UART
VDDIO_SDMMC1
AVDD_USB
VDDIO_SDMMC3
74AVCAH164245
VDDIO_PEX_CTL
TPS65911 VDDIO
MT29F16G08
SGTL5000 VDDIO
STMPE811
AX88772B VCC3x
SDIN5D2-2G VCCx  */
static struct regulator_consumer_supply fixed_reg_v3_3_supply[] = {
	REGULATOR_SUPPLY("avdd_audio", NULL),
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.1"),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_lcd_pmu", NULL),
	REGULATOR_SUPPLY("pwrdet_lcd", NULL),
	REGULATOR_SUPPLY("vddio_cam", NULL),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	/* if this supply is defined, the sdhci driver tries
	 * to set it to 1.8V */
//	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.1"),
	REGULATOR_SUPPLY("pwrdet_sdmmc2", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
	REGULATOR_SUPPLY("pwrdet_pex_ctl", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),

	/* SGTL5000 */
	REGULATOR_SUPPLY("VDDA", "4-000a"),
	REGULATOR_SUPPLY("VDDIO", "4-000a"),
};

FIXED_REG(1, v3_3, v3_3, NULL, 1, 1, -1, true, 1, 3300);

/* Gpio switch regulator platform data */
static struct platform_device *fixed_reg_devs_colibri_t30[] = {
		ADD_FIXED_REG(en_hdmi),
		ADD_FIXED_REG(v3_3),
};

int __init colibri_t30_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	/* The regulator details have complete constraints */
	regulator_has_full_constraints();

	tps_platform.num_subdevs = ARRAY_SIZE(colibri_t30_tps_devs);
	tps_platform.subdevs = colibri_t30_tps_devs;

	tps_platform.gpio_init_data = tps_gpio_pdata_colibri_t30;
	tps_platform.num_gpioinit_data = ARRAY_SIZE(tps_gpio_pdata_colibri_t30);

	i2c_register_board_info(4, colibri_t30_regulators, 1);

	/* Register the TPS6236x. */
	pr_info("Registering the device TPS62360\n");
	i2c_register_board_info(4, tps6236x_boardinfo, 1);

	return 0;
}

int __init colibri_t20_fixed_regulator_init(void)
{
	return platform_add_devices(fixed_reg_devs_colibri_t30,
				    ARRAY_SIZE(fixed_reg_devs_colibri_t30));
}
subsys_initcall_sync(colibri_t20_fixed_regulator_init);

static void colibri_t30_board_suspend(int lp_state, enum suspend_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_SUSPEND_BEFORE_CPU))
		tegra_console_uart_suspend();
}

static void colibri_t30_board_resume(int lp_state, enum resume_stage stg)
{
	if ((lp_state == TEGRA_SUSPEND_LP1) && (stg == TEGRA_RESUME_AFTER_CPU))
		tegra_console_uart_resume();
}

static struct tegra_suspend_platform_data colibri_t30_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 200,
	.suspend_mode	= TEGRA_SUSPEND_LP1,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 2000,
	.board_suspend = colibri_t30_board_suspend,
	.board_resume = colibri_t30_board_resume,
};

int __init colibri_t30_suspend_init(void)
{
	/* CORE_PWR_REQ to be high required to enable the dc-dc converter tps62362 */
	colibri_t30_suspend_data.corereq_high = true;

//required?
	colibri_t30_suspend_data.cpu_timer = 5000;
	colibri_t30_suspend_data.cpu_off_timer = 5000;

	tegra_init_suspend(&colibri_t30_suspend_data);
	return 0;
}

#ifdef CONFIG_TEGRA_EDP_LIMITS
int __init colibri_t30_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA) {
		regulator_mA = 6000; /* regular T30/s */
	}
	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);

	tegra_init_cpu_edp_limits(regulator_mA);
	return 0;
}
#endif
