/*
 * arch/arm/mach-tegra/board-colibri_t30.c
 *
 * Copyright (c) 2012, Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/mfd/stmpe.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>
#include <linux/tegra_uart.h>

#include <mach/clk.h>
#include <mach/i2s.h>
#include <mach/io_dpd.h>
#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/pinmux.h>
#include <mach/sdhci.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/tegra_fiq_debugger.h>
#include <mach/thermal.h>
#include <mach/usb_phy.h>

#include "board-colibri_t30.h"
#include "board.h"
#include "clock.h"
#include "devices.h"
#include "fuse.h"
#include "gpio-names.h"
#include "pm.h"
#include "wdt-recovery.h"

#define ETHERNET_VBUS_GPIO     TEGRA_GPIO_PDD2
#define ETHERNET_RESET_GPIO    TEGRA_GPIO_PDD0

/* Audio */

static struct tegra_asoc_platform_data colibri_t30_audio_sgtl5000_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= -1,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 0,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.is_i2s_master	= 1,
		.sample_size	= 16,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= -1,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= -1,
	},
};

static struct platform_device colibri_t30_audio_sgtl5000_device = {
	.name	= "tegra-snd-colibri_t30-sgtl5000",
	.id	= 0,
	.dev	= {
		.platform_data = &colibri_t30_audio_sgtl5000_pdata,
	},
};

/* Camera */
static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

/* Clocks */
static struct tegra_clk_init_table colibri_t30_clk_init_table[] __initdata = {
	/* name		parent		rate		enabled */
	{ "audio1",	"i2s1_sync",	0,		false},
	{ "audio2",	"i2s2_sync",	0,		false},
	{ "audio3",	"i2s3_sync",	0,		false},
	{ "blink",	"clk_32k",	32768,		true},
	{ "d_audio",	"clk_m",	12000000,	false},
	{ "dam0",	"clk_m",	12000000,	false},
	{ "dam1",	"clk_m",	12000000,	false},
	{ "dam2",	"clk_m",	12000000,	false},

//required?
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x","pll_p",	48000000,	false},

	{ "i2c1",	"pll_p",	3200000,	false},
	{ "i2c2",	"pll_p",	3200000,	false},
	{ "i2c3",	"pll_p",	3200000,	false},
	{ "i2c4",	"pll_p",	3200000,	false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ "i2s0",	"pll_a_out0",	0,		false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ "i2s3",	"pll_a_out0",	0,		false},
	{ "pll_m",	NULL,		0,		false},
	{ "pwm",	"pll_p",	3187500,	false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "vi",		"pll_p",	0,		false},
	{ "vi_sensor",	"pll_p",	150000000,	false},
	{ NULL,		NULL,		0,		0},
};

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
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
	.arb_recovery = arb_lost_recovery,
};

/* HDMI_DDC */
static struct tegra_i2c_platform_data colibri_t30_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 10000, 0 },
//	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PV4, 0},
	.sda_gpio		= {TEGRA_GPIO_PV5, 0},
	.arb_recovery = arb_lost_recovery,
};

/* PWR_I2C */

/* STMPE811 touch screen controller */
static struct stmpe_ts_platform_data stmpe811_ts_data = {
	.sample_time		= 4, /* ADC converstion time: 80 clocks */
	.mod_12b		= 1, /* 12-bit ADC */
	.ref_sel		= 0, /* internal ADC reference */
	.adc_freq		= 1, /* 3.25 MHz ADC clock speed */
	.ave_ctrl		= 3, /* 8 sample average control */
	.touch_det_delay	= 5, /* 5 ms touch detect interrupt delay */
	.settling		= 3, /* 1 ms panel driver settling time */
	.fraction_z		= 7, /* 7 length fractional part in z */
	.i_drive		= 1, /* 50 mA typical 80 mA max touchscreen drivers current limit value */
};

static struct stmpe_platform_data stmpe811_data = {
	.id				= 1,
	.blocks				= STMPE_BLOCK_TOUCHSCREEN,
	.irq_base       		= STMPE811_IRQ_BASE,
	.irq_trigger    		= IRQF_TRIGGER_FALLING,
	.ts				= &stmpe811_ts_data,
};

static struct i2c_board_info colibri_t30_i2c_bus5_board_info[] __initdata = {
	{
		/* SGTL5000 audio codec */
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
	{
		/* STMPE811 touch screen controller */
		I2C_BOARD_INFO("stmpe", 0x41),
			.type = "stmpe811",
			.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV0),
			.platform_data = &stmpe811_data,
			.flags = I2C_CLIENT_WAKE,
	},
	{
		/* LM95245 temperature sensor */
		I2C_BOARD_INFO("lm95245", 0x4c),
	},
};

static struct tegra_i2c_platform_data colibri_t30_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
//	.bus_clk_rate	= { 100000, 0 },
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery = arb_lost_recovery,
};

static void __init colibri_t30_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &colibri_t30_i2c1_platform_data;
	tegra_i2c_device4.dev.platform_data = &colibri_t30_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &colibri_t30_i2c5_platform_data;

	i2c_register_board_info(0, colibri_t30_i2c_bus1_board_info, ARRAY_SIZE(colibri_t30_i2c_bus1_board_info));

	/* enable touch interrupt GPIO */
	gpio_request(TEGRA_GPIO_PV0, "TOUCH_PEN_INT");
	gpio_direction_input(TEGRA_GPIO_PV0);

	/* setting audio codec on i2c_4 */
	i2c_register_board_info(4, colibri_t30_i2c_bus5_board_info, ARRAY_SIZE(colibri_t30_i2c_bus5_board_info));

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device1);
}

/* MMC/SD */

static struct tegra_sdhci_platform_data colibri_t30_emmc_platform_data = {
	.cd_gpio	= -1,
//.ddr_clk_limit	= 41000000,
	.ddr_clk_limit	= 52000000,
	.is_8bit	= 1,
	.mmc_data	= {
		.built_in = 1,
	},
	.power_gpio	= -1,
//.tap_delay	= 6,
	.tap_delay	= 0x0f,
	.wp_gpio	= -1,
};

static struct tegra_sdhci_platform_data colibri_t30_sdcard_platform_data = {
	.cd_gpio	= TEGRA_GPIO_PU6, /* MM_CD */
//.ddr_clk_limit	= 41000000,
	.ddr_clk_limit	= 52000000,
	.power_gpio	= -1,
//.tap_delay	= 6,
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

#if 0
/* NAND */

#if defined(CONFIG_MTD_NAND_TEGRA)
static struct resource nand_resources[] = {
	[0] = {
		.start = INT_NANDFLASH,
		.end   = INT_NANDFLASH,
		.flags = IORESOURCE_IRQ
	},
	[1] = {
		.start = TEGRA_NAND_BASE,
		.end = TEGRA_NAND_BASE + TEGRA_NAND_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Micron MT29F16G08CBACA */
	[0] = {
		.vendor_id		= 0x2c,
		.device_id		= 0x48,
		.read_id_fourth_byte	= 0x4a,
		.capacity		= 2048,
		.timing			= {
			/* mode 4 */
			.trp		= 12,
			.trh		= 10,	/* tREH */
			.twp		= 12,
			.twh		= 10,
			.tcs		= 20,	/* Max(tCS, tCH, tALS, tALH) */
			.twhr		= 60,
			.tcr_tar_trr	= 20,	/* Max(tCR, tAR, tRR) */
			.twb		= 100,
			.trp_resp	= 12,	/* tRP */
			.tadl		= 70,
		},
	},
};

static struct tegra_nand_platform nand_data = {
	.max_chips	= 8,
	.chip_parms	= nand_chip_parms,
	.nr_chip_parms  = ARRAY_SIZE(nand_chip_parms),
};

static struct platform_device tegra_nand_device = {
	.name          = "tegra_nand",
	.id            = -1,
	.resource      = nand_resources,
	.num_resources = ARRAY_SIZE(nand_resources),
	.dev            = {
		.platform_data = &nand_data,
	},
};

static void __init colibri_t30_nand_init(void)
{
	/* eMMC vs. NAND flash detection */
	if (!gpio_get_value(TEGRA_GPIO_PC7)) {
		pr_info("Detected NAND flash variant, registering controller driver.\n");
		platform_device_register(&tegra_nand_device);
	}
	tegra_gpio_disable(TEGRA_GPIO_PC7);
}
#else
static inline void colibri_t30_nand_init(void) {}
#endif
#endif

/* RTC */

#if defined(CONFIG_RTC_DRV_TEGRA)
static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};
#endif

/* SPI */
//TBR
static struct platform_device *colibri_t30_spi_devices[] __initdata = {
	&tegra_spi_device4,
};

static struct spi_clk_parent spi_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};

static struct tegra_spi_platform_data colibri_t30_spi_pdata = {
	.is_dma_based		= true,
	.max_dma_buffer		= (16 * 1024),
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
	tegra_spi_device4.dev.platform_data = &colibri_t30_spi_pdata;
	platform_add_devices(colibri_t30_spi_devices,
				ARRAY_SIZE(colibri_t30_spi_devices));
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

//can't be initdata
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
		debug_uart_clk =  clk_get_sys("serial8250.0", "uartb");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		break;

	case 3:
		/* UARTD is the debug port. */
		pr_info("Selecting UARTD as the debug console\n");
		colibri_t30_uart_devices[2] = &debug_uartd_device;
		debug_uart_clk =  clk_get_sys("serial8250.0", "uartd");
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

#if defined(CONFIG_USB_SUPPORT)
#if 0
static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.vbus_reg = "vdd_vbus_micro_usb",
		.hot_plug = true,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};
#endif

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode        = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio		= TEGRA_GPIO_PW2,	/* USBH_PEN */
		.vbus_gpio_inverted	= 1,
//		.vbus_reg = NULL,
//		.vbus_reg = "vdd_vbus_typea_usb",
		.hot_plug = true,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

#if 0
static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = 0,
		.vbus_gpio = -1,
		.charging_supported = false,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};
#endif

static void colibri_t30_usb_init(void)
{
#if 0
	/* OTG should be the first to be registered */
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	/* setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
#endif
	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_utmi_pdata;
	platform_device_register(&tegra_ehci2_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
}
#else /* CONFIG_USB_SUPPORT */
static inline void colibri_t30_usb_init(void) { }
#endif /* CONFIG_USB_SUPPORT */

static struct platform_device *colibri_t30_devices[] __initdata = {
	&tegra_pmu_device,
#if defined(CONFIG_RTC_DRV_TEGRA)
	&tegra_rtc_device,
#endif
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU) ||  defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
	&tegra_wdt0_device,
	&tegra_wdt1_device,
	&tegra_wdt2_device,
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
	&tegra_camera,
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
#if 0
	tegra_thermal_init(&thermal_data,
				throttle_list,
				ARRAY_SIZE(throttle_list));
#endif
	tegra_clk_init_from_table(colibri_t30_clk_init_table);
	colibri_t30_pinmux_init();
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
	colibri_t30_pins_state_init();
	colibri_t30_emc_init();
//	colibri_t30_nand_init();
	tegra_release_bootloader_fb();
#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif
	tegra_serial_debug_init(TEGRA_UARTD_BASE, INT_WDT_CPU, NULL, -1, -1);

	/* Activate Mic Bias */
	gpio_request(TEGRA_GPIO_PT1, "EN_MIC_GND");
	gpio_direction_output(TEGRA_GPIO_PT1, 1);
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

MACHINE_START(COLIBRI_T30, "Toradex Colibri T30")
	.boot_params    = 0x80000100,
	.init_early	= tegra_init_early,
	.init_irq       = tegra_init_irq,
	.init_machine   = colibri_t30_init,
	.map_io         = tegra_map_common_io,
	.reserve        = colibri_t30_reserve,
	.timer          = &tegra_timer,
MACHINE_END
