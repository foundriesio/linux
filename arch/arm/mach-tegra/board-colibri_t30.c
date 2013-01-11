/*
 * arch/arm/mach-tegra/board-colibri_t30.c
 *
 * Copyright (c) 2012-2013 Toradex, Inc.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/colibri_usb.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/lm95245.h>
#include <linux/mfd/stmpe.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>
#include <linux/tegra_uart.h>

#include <mach/io_dpd.h>
#include <mach/sdhci.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/tegra_fiq_debugger.h>
#include <mach/thermal.h>
#include <mach/usb_phy.h>

#include "board-colibri_t30.h"
#include "board.h"
#include "clock.h"
#include "devices.h"
#include "gpio-names.h"
#include "pm.h"

/* ADC */

//TODO

/* Audio */

static struct tegra_asoc_platform_data colibri_t30_audio_sgtl5000_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= -1,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
	.i2s_param[HIFI_CODEC] = {
		.audio_port_id	= 0,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.is_i2s_master	= 1,
		.sample_size	= 16,
	},
	.i2s_param[BASEBAND] = {
		.audio_port_id	= -1,
	},
	.i2s_param[BT_SCO] = {
		.audio_port_id	= -1,
	},
};

static struct platform_device colibri_t30_audio_sgtl5000_device = {
	.name	= "tegra-snd-colibri_t30-sgtl5000",
	.id	= 0,
	.dev = {
		.platform_data = &colibri_t30_audio_sgtl5000_pdata,
	},
};

#ifdef CONFIG_TEGRA_CAMERA
/* Camera */
static struct platform_device tegra_camera = {
	.name	= "tegra_camera",
	.id	= -1,
};
#endif /* CONFIG_TEGRA_CAMERA */

/* Clocks */
static struct tegra_clk_init_table colibri_t30_clk_init_table[] __initdata = {
	/* name		parent		rate		enabled */
	{"audio1",	"i2s1_sync",	0,		false},
	{"audio2",	"i2s2_sync",	0,		false},
	{"audio3",	"i2s3_sync",	0,		false},
	{"blink",	"clk_32k",	32768,		true},
	{"d_audio",	"clk_m",	12000000,	false},
	{"dam0",	"clk_m",	12000000,	false},
	{"dam1",	"clk_m",	12000000,	false},
	{"dam2",	"clk_m",	12000000,	false},

//required?
	{"hda",	"pll_p",	108000000,	false},
	{"hda2codec_2x","pll_p",	48000000,	false},

	{"i2c1",	"pll_p",	3200000,	false},
	{"i2c2",	"pll_p",	3200000,	false},
	{"i2c3",	"pll_p",	3200000,	false},
	{"i2c4",	"pll_p",	3200000,	false},
	{"i2c5",	"pll_p",	3200000,	false},
	{"i2s0",	"pll_a_out0",	0,		false},
	{"i2s1",	"pll_a_out0",	0,		false},
	{"i2s2",	"pll_a_out0",	0,		false},
	{"i2s3",	"pll_a_out0",	0,		false},
	{"pll_m",	NULL,		0,		false},
	{"pwm",		"pll_p",	3187500,	false},
	{"spdif_out",	"pll_a_out0",	0,		false},
	{"vi",		"pll_p",	0,		false},
	{"vi_sensor",	"pll_p",	150000000,	false},
	{NULL,		NULL,		0,		0},
};

/* GPIO */

#define DDC_SCL		TEGRA_GPIO_PV4	/* X2-15 */
#define DDC_SDA		TEGRA_GPIO_PV5	/* X2-16 */

#ifdef COLIBRI_T30_V10
#define EMMC_DETECT	TEGRA_GPIO_PC7
#endif

#define EN_MIC_GND	TEGRA_GPIO_PT1

#define I2C_SCL		TEGRA_GPIO_PC4	/* SODIMM 196 */
#define I2C_SDA		TEGRA_GPIO_PC5	/* SODIMM 194 */

#define LAN_EXT_WAKEUP	TEGRA_GPIO_PDD1
#define LAN_PME		TEGRA_GPIO_PDD3
#define LAN_RESET	TEGRA_GPIO_PDD0
#define LAN_V_BUS	TEGRA_GPIO_PDD2

#ifdef COLIBRI_T30_V10
#define MMC_CD		TEGRA_GPIO_PU6	/* SODIMM 43 */
#else
#define MMC_CD		TEGRA_GPIO_PC7	/* SODIMM 43 */
#endif

#define PWR_I2C_SCL	TEGRA_GPIO_PZ6
#define PWR_I2C_SDA	TEGRA_GPIO_PZ7

#define TOUCH_PEN_INT	TEGRA_GPIO_PV0

#define THERMD_ALERT	TEGRA_GPIO_PD2

#define USBC_DET	TEGRA_GPIO_PK5	/* SODIMM 137 */
#define USBH_OC		TEGRA_GPIO_PW3	/* SODIMM 131 */
#define USBH_PEN	TEGRA_GPIO_PW2	/* SODIMM 129 */

//TODO: sysfs GPIO exports

/* I2C */

/* GEN1_I2C: I2C_SDA/SCL on SODIMM pin 194/196 (e.g. RTC on carrier board) */
static struct i2c_board_info colibri_t30_i2c_bus1_board_info[] __initdata = {
	{
		/* M41T0M6 real time clock on Iris carrier board */
		I2C_BOARD_INFO("rtc-ds1307", 0x68),
			.type = "m41t00",
	},
};

static struct tegra_i2c_platform_data colibri_t30_i2c1_platform_data = {
	.adapter_nr	= 0,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {400000, 0},
	.bus_count	= 1,
	.scl_gpio	= {I2C_SCL, 0},
	.sda_gpio	= {I2C_SDA, 0},
	.slave_addr	= 0x00FC,
};

/* GEN2_I2C: unused */

/* DDC_CLOCK/DATA on X3 pin 15/16 (e.g. display EDID) */
static struct tegra_i2c_platform_data colibri_t30_i2c4_platform_data = {
	.adapter_nr	= 3,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {10000, 10000},
	.bus_count	= 1,
	.scl_gpio	= {DDC_SCL, 0},
	.sda_gpio	= {DDC_SDA, 0},
	.slave_addr	= 0x00FC,
};

/* PWR_I2C: power I2C to audio codec, PMIC, temperature sensor and touch screen
	    controller */

/* STMPE811 touch screen controller */
static struct stmpe_ts_platform_data stmpe811_ts_data = {
	.adc_freq		= 1, /* 3.25 MHz ADC clock speed */
	.ave_ctrl		= 3, /* 8 sample average control */
	.fraction_z		= 7, /* 7 length fractional part in z */
	.i_drive		= 1, /* 50 mA typical 80 mA max touchscreen drivers current limit value */
	.mod_12b		= 1, /* 12-bit ADC */
	.ref_sel		= 0, /* internal ADC reference */
	.sample_time		= 4, /* ADC converstion time: 80 clocks */
	.settling		= 3, /* 1 ms panel driver settling time */
	.touch_det_delay	= 5, /* 5 ms touch detect interrupt delay */
};

static struct stmpe_platform_data stmpe811_data = {
	.blocks		= STMPE_BLOCK_TOUCHSCREEN,
	.id		= 1,
	.irq_base	= STMPE811_IRQ_BASE,
	.irq_trigger	= IRQF_TRIGGER_FALLING,
	.ts		= &stmpe811_ts_data,
};

static void lm95245_probe_callback(struct device *dev);

static struct lm95245_platform_data colibri_t30_lm95245_pdata = {
	.enable_os_pin	= true,
	.probe_callback	= lm95245_probe_callback,
};

static struct i2c_board_info colibri_t30_i2c_bus5_board_info[] __initdata = {
	{
		/* SGTL5000 audio codec */
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
	{
		/* STMPE811 touch screen controller */
		I2C_BOARD_INFO("stmpe", 0x41),
			.flags = I2C_CLIENT_WAKE,
			.irq = TEGRA_GPIO_TO_IRQ(TOUCH_PEN_INT),
			.platform_data = &stmpe811_data,
			.type = "stmpe811",
	},
	{
		/* LM95245 temperature sensor
		   Note: OVERT_N directly connected to PMIC PWRDN */
		I2C_BOARD_INFO("lm95245", 0x4c),
			.platform_data = &colibri_t30_lm95245_pdata,
	},
};

static struct tegra_i2c_platform_data colibri_t30_i2c5_platform_data = {
	.adapter_nr	= 4,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {400000, 0},
	.bus_count	= 1,
	.scl_gpio	= {PWR_I2C_SCL, 0},
	.sda_gpio	= {PWR_I2C_SDA, 0},
};

static void __init colibri_t30_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &colibri_t30_i2c1_platform_data;
	tegra_i2c_device4.dev.platform_data = &colibri_t30_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &colibri_t30_i2c5_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device5);

	i2c_register_board_info(0, colibri_t30_i2c_bus1_board_info, ARRAY_SIZE(colibri_t30_i2c_bus1_board_info));

	/* enable touch interrupt GPIO */
	gpio_request(TOUCH_PEN_INT, "TOUCH_PEN_INT");
	gpio_direction_input(TOUCH_PEN_INT);

	i2c_register_board_info(4, colibri_t30_i2c_bus5_board_info, ARRAY_SIZE(colibri_t30_i2c_bus5_board_info));
}

/* Keys */

//TODO

/* MMC/SD */

#ifndef COLIBRI_T30_SDMMC4B
static struct tegra_sdhci_platform_data colibri_t30_emmc_platform_data = {
	.cd_gpio	= -1,
	.ddr_clk_limit	= 52000000,
	.is_8bit	= 1,
	.mmc_data = {
		.built_in = 1,
	},
	.power_gpio	= -1,
	.tap_delay	= 0x0f,
	.wp_gpio	= -1,
};
#endif /* COLIBRI_T30_SDMMC4B */

static struct tegra_sdhci_platform_data colibri_t30_sdcard_platform_data = {
	.cd_gpio	= MMC_CD,
	.ddr_clk_limit	= 52000000,
	.power_gpio	= -1,
	.tap_delay	= 0x0f,
	.wp_gpio	= -1,
};

static void __init colibri_t30_sdhci_init(void)
{
	/* register eMMC first */
	tegra_sdhci_device4.dev.platform_data =
#ifdef COLIBRI_T30_SDMMC4B
			&colibri_t30_sdcard_platform_data;
#else
			&colibri_t30_emmc_platform_data;
#endif
	platform_device_register(&tegra_sdhci_device4);

#ifndef COLIBRI_T30_SDMMC4B
	tegra_sdhci_device2.dev.platform_data =
			&colibri_t30_sdcard_platform_data;
	platform_device_register(&tegra_sdhci_device2);
#endif
}

/* RTC */

#ifdef CONFIG_RTC_DRV_TEGRA
static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start	= TEGRA_RTC_BASE,
		.end	= TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_RTC,
		.end	= INT_RTC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name		= "tegra_rtc",
	.id		= -1,
	.resource	= tegra_rtc_resources,
	.num_resources	= ARRAY_SIZE(tegra_rtc_resources),
};
#endif /* CONFIG_RTC_DRV_TEGRA */

/* SPI */

#if defined(CONFIG_SPI_TEGRA) && defined(CONFIG_SPI_SPIDEV)
static struct spi_board_info tegra_spi_devices[] __initdata = {
	{
		.bus_num	= 0,		/* SPI1 */
		.chip_select	= 0,
		.irq		= 0,
		.max_speed_hz	= 50000000,
		.modalias	= "spidev",
		.mode		= SPI_MODE_0,
		.platform_data	= NULL,
	},
};

static void __init colibri_t30_register_spidev(void)
{
	spi_register_board_info(tegra_spi_devices,
				ARRAY_SIZE(tegra_spi_devices));
}
#else /* CONFIG_SPI_TEGRA && CONFIG_SPI_SPIDEV */
#define colibri_t30_register_spidev() do {} while (0)
#endif /* CONFIG_SPI_TEGRA && CONFIG_SPI_SPIDEV */

static struct platform_device *colibri_t30_spi_devices[] __initdata = {
	&tegra_spi_device1,
};

static struct spi_clk_parent spi_parent_clk[] = {
	[0] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
#else /* !CONFIG_TEGRA_PLLM_RESTRICTED */
	[1] = {.name = "clk_m"},
#endif /* !CONFIG_TEGRA_PLLM_RESTRICTED */
};

static struct tegra_spi_platform_data colibri_t30_spi_pdata = {
	.is_dma_based		= true,
	.max_dma_buffer		= 16 * 1024,
	.is_clkon_always	= false,
	.max_rate		= 100000000,
};

static void __init colibri_t30_spi_init(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(spi_parent_clk); ++i) {
		c = tegra_get_clock_by_name(spi_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						spi_parent_clk[i].name);
			continue;
		}
		spi_parent_clk[i].parent_clk = c;
		spi_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	colibri_t30_spi_pdata.parent_clk_list = spi_parent_clk;
	colibri_t30_spi_pdata.parent_clk_count = ARRAY_SIZE(spi_parent_clk);
	tegra_spi_device1.dev.platform_data = &colibri_t30_spi_pdata;
	platform_add_devices(colibri_t30_spi_devices,
				ARRAY_SIZE(colibri_t30_spi_devices));
}

/* Thermal throttling */

static void *colibri_t30_alert_data;
static void (*colibri_t30_alert_func)(void *);
static int colibri_t30_low_edge = 0;
static int colibri_t30_low_hysteresis = 3000;
static int colibri_t30_low_limit = 0;
static struct device *lm95245_device = NULL;
static int thermd_alert_irq_disabled = 0;
struct work_struct thermd_alert_work;
struct workqueue_struct *thermd_alert_workqueue;

static struct balanced_throttle throttle_list[] = {
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	{
		.id = BALANCED_THROTTLE_ID_TJ,
		.throt_tab_size = 10,
		.throt_tab = {
			{      0, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 760000, 1000 },
			{ 760000, 1050 },
			{1000000, 1050 },
			{1000000, 1100 },
		},
	},
#endif /* CONFIG_TEGRA_THERMAL_THROTTLE */
#ifdef CONFIG_TEGRA_SKIN_THROTTLE
	{
		.id = BALANCED_THROTTLE_ID_SKIN,
		.throt_tab_size = 6,
		.throt_tab = {
			{ 640000, 1200 },
			{ 640000, 1200 },
			{ 760000, 1200 },
			{ 760000, 1200 },
			{1000000, 1200 },
			{1000000, 1200 },
		},
	},
#endif /* CONFIG_TEGRA_SKIN_THROTTLE */
};

/* All units are in millicelsius */
static struct tegra_thermal_data thermal_data = {
	.shutdown_device_id = THERMAL_DEVICE_ID_NCT_EXT,
	.temp_shutdown = 115000,

#if defined(CONFIG_TEGRA_EDP_LIMITS) || defined(CONFIG_TEGRA_THERMAL_THROTTLE)
	.throttle_edp_device_id = THERMAL_DEVICE_ID_NCT_EXT,
#endif
#ifdef CONFIG_TEGRA_EDP_LIMITS
	.edp_offset = TDIODE_OFFSET, /* edp based on tdiode */
	.hysteresis_edp = 3000,
#endif
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	.temp_throttle = 85000,
	.tc1 = 0,
	.tc2 = 1,
	.passive_delay = 2000,
#endif /* CONFIG_TEGRA_THERMAL_THROTTLE */
#ifdef CONFIG_TEGRA_SKIN_THROTTLE
	.skin_device_id = THERMAL_DEVICE_ID_SKIN,
	.temp_throttle_skin = 43000,
	.tc1_skin = 0,
	.tc2_skin = 1,
	.passive_delay_skin = 5000,

	.skin_temp_offset = 9793,
	.skin_period = 1100,
	.skin_devs_size = 2,
	.skin_devs = {
		{
			THERMAL_DEVICE_ID_NCT_EXT,
			{
				2, 1, 1, 1,
				1, 1, 1, 1,
				1, 1, 1, 0,
				1, 1, 0, 0,
				0, 0, -1, -7
			}
		},
		{
			THERMAL_DEVICE_ID_NCT_INT,
			{
				-11, -7, -5, -3,
				-3, -2, -1, 0,
				0, 0, 1, 1,
				1, 2, 2, 3,
				4, 6, 11, 18
			}
		},
	},
#endif /* CONFIG_TEGRA_SKIN_THROTTLE */
};

/* Over-temperature shutdown OS aka high limit GPIO pin interrupt handler */
static irqreturn_t thermd_alert_irq(int irq, void *data)
{
	disable_irq_nosync(irq);
	thermd_alert_irq_disabled = 1;
	queue_work(thermd_alert_workqueue, &thermd_alert_work);

	return IRQ_HANDLED;
}

/* Gets both entered by THERMD_ALERT GPIO interrupt as well as re-scheduled. */
static void thermd_alert_work_func(struct work_struct *work)
{
	int temp = 0;

	lm95245_get_remote_temp(lm95245_device, &temp);

	/* This emulates NCT1008 low limit behaviour */
	if (!colibri_t30_low_edge && temp <= colibri_t30_low_limit) {
		colibri_t30_alert_func(colibri_t30_alert_data);
		colibri_t30_low_edge = 1;
	} else if (colibri_t30_low_edge && temp > colibri_t30_low_limit + colibri_t30_low_hysteresis) {
		colibri_t30_low_edge = 0;
	}

	/* Avoid unbalanced enable for IRQ 367 */
	if (thermd_alert_irq_disabled) {
		colibri_t30_alert_func(colibri_t30_alert_data);
		thermd_alert_irq_disabled = 0;
		enable_irq(TEGRA_GPIO_TO_IRQ(THERMD_ALERT));
	}

	/* Keep re-scheduling */
	msleep(2000);
	queue_work(thermd_alert_workqueue, &thermd_alert_work);
}

static int lm95245_get_temp(void *_data, long *temp)
{
	struct device *lm95245_device = _data;
	int lm95245_temp = 0;
	lm95245_get_remote_temp(lm95245_device, &lm95245_temp);
	*temp = lm95245_temp;
	return 0;
}

static int lm95245_get_temp_low(void *_data, long *temp)
{
	*temp = 0;
	return 0;
}

/* Our temperature sensor only allows triggering an interrupt on over-
   temperature shutdown aka the high limit we therefore need to setup a
   workqueue to catch leaving the low limit. */
static int lm95245_set_limits(void *_data,
			long lo_limit_milli,
			long hi_limit_milli)
{
	struct device *lm95245_device = _data;
	colibri_t30_low_limit = lo_limit_milli;
	if (lm95245_device) lm95245_set_remote_os_limit(lm95245_device, hi_limit_milli);
	return 0;
}

static int lm95245_set_alert(void *_data,
				void (*alert_func)(void *),
				void *alert_data)
{
	lm95245_device = _data;
	colibri_t30_alert_func = alert_func;
	colibri_t30_alert_data = alert_data;
	return 0;
}

static int lm95245_set_shutdown_temp(void *_data, long shutdown_temp)
{
	struct device *lm95245_device = _data;
	if (lm95245_device) lm95245_set_remote_critical_limit(lm95245_device, shutdown_temp);
	return 0;
}

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
/* Internal aka local board/case temp */
static int lm95245_get_itemp(void *dev_data, long *temp)
{
	struct device *lm95245_device = dev_data;
	int lm95245_temp = 0;
	lm95245_get_local_temp(lm95245_device, &lm95245_temp);
	*temp = lm95245_temp;
	return 0;
}
#endif /* CONFIG_TEGRA_SKIN_THROTTLE */

static void lm95245_probe_callback(struct device *dev)
{
	struct tegra_thermal_device *lm95245_remote;

	lm95245_remote = kzalloc(sizeof(struct tegra_thermal_device),
					GFP_KERNEL);
	if (!lm95245_remote) {
		pr_err("unable to allocate thermal device\n");
		return;
	}

	lm95245_remote->name = "lm95245_remote";
	lm95245_remote->id = THERMAL_DEVICE_ID_NCT_EXT;
	lm95245_remote->data = dev;
	lm95245_remote->offset = TDIODE_OFFSET;
	lm95245_remote->get_temp = lm95245_get_temp;
	lm95245_remote->get_temp_low = lm95245_get_temp_low;
	lm95245_remote->set_limits = lm95245_set_limits;
	lm95245_remote->set_alert = lm95245_set_alert;
	lm95245_remote->set_shutdown_temp = lm95245_set_shutdown_temp;

	tegra_thermal_device_register(lm95245_remote);

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
	{
		struct tegra_thermal_device *lm95245_local;
		lm95245_local = kzalloc(sizeof(struct tegra_thermal_device),
						GFP_KERNEL);
		if (!lm95245_local) {
			kfree(lm95245_local);
			pr_err("unable to allocate thermal device\n");
			return;
		}

		lm95245_local->name = "lm95245_local";
		lm95245_local->id = THERMAL_DEVICE_ID_NCT_INT;
		lm95245_local->data = dev;
		lm95245_local->get_temp = lm95245_get_itemp;

		tegra_thermal_device_register(lm95245_local);
	}
#endif /* CONFIG_TEGRA_SKIN_THROTTLE */

	if (request_irq(TEGRA_GPIO_TO_IRQ(THERMD_ALERT), thermd_alert_irq,
			IRQF_TRIGGER_LOW, "THERMD_ALERT", NULL))
		pr_err("%s: unable to register THERMD_ALERT interrupt\n", __func__);
}

static void colibri_t30_thermd_alert_init(void)
{
	gpio_request(THERMD_ALERT, "THERMD_ALERT");
	gpio_direction_input(THERMD_ALERT);

	thermd_alert_workqueue = create_singlethread_workqueue("THERMD_ALERT");

	INIT_WORK(&thermd_alert_work, thermd_alert_work_func);
}

/* UART */

static struct platform_device *colibri_t30_uart_devices[] __initdata = {
	&tegra_uarta_device, /* FF */
	&tegra_uartb_device, /* STD */
	&tegra_uartd_device, /* BT */
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[2] = {.name = "pll_m"},
#endif
};

static struct tegra_uart_platform_data colibri_t30_uart_pdata;

static void __init uart_debug_init(void)
{
	int debug_port_id;

	debug_port_id = get_tegra_uart_debug_port_id();
	if (debug_port_id < 0) {
		debug_port_id = 0;
	}

	switch (debug_port_id) {
	case 0:
		/* UARTA is the debug port. */
		pr_info("Selecting UARTA as the debug console\n");
		colibri_t30_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		break;

	case 1:
		/* UARTB is the debug port. */
		pr_info("Selecting UARTB as the debug console\n");
		colibri_t30_uart_devices[1] = &debug_uartb_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartb");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		break;

	case 3:
		/* UARTD is the debug port. */
		pr_info("Selecting UARTD as the debug console\n");
		colibri_t30_uart_devices[2] = &debug_uartd_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartd");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
		break;

	default:
		pr_info("The debug console id %d is invalid, Assuming UARTA", debug_port_id);
		colibri_t30_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		break;
	}
	return;
}

static void __init colibri_t30_uart_init(void)
{
	struct clk *c;
	int i;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	colibri_t30_uart_pdata.parent_clk_list = uart_parent_clk;
	colibri_t30_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uarta_device.dev.platform_data = &colibri_t30_uart_pdata;
	tegra_uartb_device.dev.platform_data = &colibri_t30_uart_pdata;
	tegra_uartd_device.dev.platform_data = &colibri_t30_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs()) {
		uart_debug_init();
		/* Clock enable for the debug channel */
		if (!IS_ERR_OR_NULL(debug_uart_clk)) {
			pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
			c = tegra_get_clock_by_name("pll_p");
			if (IS_ERR_OR_NULL(c))
				pr_err("Not getting the parent clock pll_p\n");
			else
				clk_set_parent(debug_uart_clk, c);

			clk_enable(debug_uart_clk);
			clk_set_rate(debug_uart_clk, clk_get_rate(c));
		} else {
			pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
		}
	}

	platform_add_devices(colibri_t30_uart_devices,
				ARRAY_SIZE(colibri_t30_uart_devices));
}

/* USB */

//TODO: overcurrent

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.has_hostpc	= true,
	.op_mode	= TEGRA_USB_OPMODE_DEVICE,
	.phy_intf	= TEGRA_USB_PHY_INTF_UTMI,
	.port_otg	= true,
	.u_cfg.utmi = {
		.elastic_limit		= 16,
		.hssync_start_delay	= 0,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 8,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
	.u_data.dev = {
		.charging_supported		= false,
		.remote_wakeup_supported	= false,
		.vbus_gpio			= -1,
		.vbus_pmu_irq			= 0,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.has_hostpc	= true,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.phy_intf	= TEGRA_USB_PHY_INTF_UTMI,
	.port_otg	= true,
	.u_cfg.utmi = {
		.elastic_limit		= 16,
		.hssync_start_delay	= 0,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 15,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
	.u_data.host = {
		.hot_plug			= true,
		.power_off_on_suspend		= false,
		.remote_wakeup_supported	= true,
		.vbus_gpio			= -1,
		.vbus_reg			= NULL,
	},
};

static void ehci2_utmi_platform_post_phy_on(void)
{
	/* enable VBUS */
	gpio_set_value(LAN_V_BUS, 1);

	/* reset */
	gpio_set_value(LAN_RESET, 0);

	udelay(5);

	/* unreset */
	gpio_set_value(LAN_RESET, 1);
}

static void ehci2_utmi_platform_pre_phy_off(void)
{
	/* disable VBUS */
	gpio_set_value(LAN_V_BUS, 0);
}

static struct tegra_usb_phy_platform_ops ehci2_utmi_plat_ops = {
	.post_phy_on = ehci2_utmi_platform_post_phy_on,
	.pre_phy_off = ehci2_utmi_platform_pre_phy_off,
};

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
	.has_hostpc	= true,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.ops		= &ehci2_utmi_plat_ops,
	.phy_intf	= TEGRA_USB_PHY_INTF_UTMI,
	.port_otg	= false,
	.u_cfg.utmi = {
		.elastic_limit		= 16,
		.hssync_start_delay	= 0,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 15,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
	.u_data.host = {
		.hot_plug			= false,
		.power_off_on_suspend		= true,
		.remote_wakeup_supported	= true,
		.vbus_gpio			= -1,
		.vbus_reg			= NULL,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.has_hostpc	= true,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.phy_intf	= TEGRA_USB_PHY_INTF_UTMI,
	.port_otg	= false,
	.u_cfg.utmi = {
		.elastic_limit		= 16,
		.hssync_start_delay	= 0,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 8,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
	.u_data.host = {
		.hot_plug			= true,
		.power_off_on_suspend		= false,
		.remote_wakeup_supported	= true,
		.vbus_gpio			= USBH_PEN,
		.vbus_gpio_inverted		= 1,
		.vbus_reg			= NULL,
	},
};

#ifndef CONFIG_USB_TEGRA_OTG
static struct platform_device *tegra_usb_otg_host_register(void)
{
	struct platform_device *pdev;
	void *platform_data;
	int val;

	pdev = platform_device_alloc(tegra_ehci1_device.name,
				     tegra_ehci1_device.id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, tegra_ehci1_device.resource,
					    tegra_ehci1_device.num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask = tegra_ehci1_device.dev.dma_mask;
	pdev->dev.coherent_dma_mask = tegra_ehci1_device.dev.coherent_dma_mask;

	platform_data = kmalloc(sizeof(struct tegra_usb_platform_data),
				GFP_KERNEL);
	if (!platform_data)
		goto error;

	memcpy(platform_data, &tegra_ehci1_utmi_pdata,
	       sizeof(struct tegra_usb_platform_data));
	pdev->dev.platform_data = platform_data;

	val = platform_device_add(pdev);
	if (val)
		goto error_add;

	return pdev;

error_add:
	kfree(platform_data);
error:
	pr_err("%s: failed to add the host controller device\n", __func__);
	platform_device_put(pdev);
	return NULL;
}

static void tegra_usb_otg_host_unregister(struct platform_device *pdev)
{
	kfree(pdev->dev.platform_data);
	pdev->dev.platform_data = NULL;
	platform_device_unregister(pdev);
}

static struct colibri_otg_platform_data colibri_otg_pdata = {
	.cable_detect_gpio	= USBC_DET,
	.host_register		= &tegra_usb_otg_host_register,
	.host_unregister	= &tegra_usb_otg_host_unregister,
};
#else /* !CONFIG_USB_TEGRA_OTG */
static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};
#endif /* !CONFIG_USB_TEGRA_OTG */

#ifndef CONFIG_USB_TEGRA_OTG
struct platform_device colibri_otg_device = {
	.name	= "colibri-otg",
	.id	= -1,
	.dev = {
		.platform_data = &colibri_otg_pdata,
	},
};
#endif /* !CONFIG_USB_TEGRA_OTG */

static void colibri_t30_usb_init(void)
{
	gpio_request(LAN_V_BUS, "LAN_V_BUS");
	gpio_direction_output(LAN_V_BUS, 0);
	gpio_export(LAN_V_BUS, false);

	gpio_request(LAN_RESET, "LAN_RESET");
	gpio_direction_output(LAN_RESET, 0);
	gpio_export(LAN_RESET, false);

	/* OTG should be the first to be registered
	   EHCI instance 0: USB1_DP/N -> USBOTG_P/N */
#ifndef CONFIG_USB_TEGRA_OTG
	platform_device_register(&colibri_otg_device);
#else /* !CONFIG_USB_TEGRA_OTG */
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);
#endif /* !CONFIG_USB_TEGRA_OTG */

	/* setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	platform_device_register(&tegra_udc_device);

	/* EHCI instance 1: ASIX ETH */
	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_utmi_pdata;
	platform_device_register(&tegra_ehci2_device);

	/* EHCI instance 2: USB3_DP/N -> USBH1_P/N */
	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
}

static struct platform_device *colibri_t30_devices[] __initdata = {
	&tegra_pmu_device,
#if defined(CONFIG_RTC_DRV_TEGRA)
	&tegra_rtc_device,
#endif
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
	&tegra_wdt0_device,
	&tegra_wdt1_device,
	&tegra_wdt2_device,
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
#ifdef CONFIG_TEGRA_CAMERA
	&tegra_camera,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra_se_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif

	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&spdif_dit_device,
	&tegra_pcm_device,
	&colibri_t30_audio_sgtl5000_device,

	&tegra_cec_device,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
};

static void __init colibri_t30_init(void)
{
	tegra_thermal_init(&thermal_data,
				throttle_list,
				ARRAY_SIZE(throttle_list));
	tegra_clk_init_from_table(colibri_t30_clk_init_table);
	colibri_t30_pinmux_init();
	colibri_t30_thermd_alert_init();
	colibri_t30_i2c_init();
	colibri_t30_spi_init();
	colibri_t30_usb_init();
#ifdef CONFIG_TEGRA_EDP_LIMITS
	colibri_t30_edp_init();
#endif
	colibri_t30_uart_init();
	platform_add_devices(colibri_t30_devices, ARRAY_SIZE(colibri_t30_devices));
	tegra_ram_console_debug_init();
	tegra_io_dpd_init();
	colibri_t30_sdhci_init();
	colibri_t30_regulator_init();
	colibri_t30_suspend_init();
	colibri_t30_panel_init();
//	colibri_t30_sensors_init();
	colibri_t30_emc_init();
	colibri_t30_register_spidev();

	tegra_release_bootloader_fb();
#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif
	tegra_serial_debug_init(TEGRA_UARTD_BASE, INT_WDT_CPU, NULL, -1, -1);

	/* Activate Mic Bias */
	gpio_request(EN_MIC_GND, "EN_MIC_GND");
	gpio_direction_output(EN_MIC_GND, 1);
}

static void __init colibri_t30_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	/* Support 1920X1080 32bpp,double buffered on HDMI*/
	tegra_reserve(0, SZ_8M + SZ_1M, SZ_16M);
#else
	tegra_reserve(SZ_128M, SZ_8M, SZ_8M);
#endif
	tegra_ram_console_debug_reserve(SZ_1M);
}

static const char *colibri_t30_dt_board_compat[] = {
	"toradex,colibri_t30",
	NULL
};

MACHINE_START(COLIBRI_T30, "Toradex Colibri T30")
	.boot_params	= 0x80000100,
	.dt_compat	= colibri_t30_dt_board_compat,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.init_machine	= colibri_t30_init,
	.map_io		= tegra_map_common_io,
	.reserve	= colibri_t30_reserve,
	.timer		= &tegra_timer,
MACHINE_END
