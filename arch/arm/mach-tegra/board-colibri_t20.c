/*
 * arch/arm/mach-tegra/board-colibri_t20.c
 *
 * Copyright (C) 2011 Toradex, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <linux/clk.h>
#include <linux/colibri_usb.h>
#include <linux/dma-mapping.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/mfd/tps6586x.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/suspend.h>
#include <linux/tegra_uart.h>
#include <linux/wm97xx.h>

#include <mach/audio.h>
#include <mach/clk.h>
#include <mach/gpio.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/sdhci.h>
#include <mach/spdif.h>
#include <mach/usb_phy.h>

#include "board-colibri_t20.h"
#include "board.h"
#include "clock.h"
#include "devices.h"
#include "gpio-names.h"
//move to board-colibri_t20-power.c?
#include "pm.h"

//conflicts with MECS Tellurium xPOD2 SSPTXD2
#define USB_CABLE_DETECT_GPIO	TEGRA_GPIO_PK5	/* USBC_DET */

/* ADC */

static struct wm97xx_batt_pdata colibri_t20_adc_pdata = {
	.batt_aux = WM97XX_AUX_ID1,	/* AD0 - ANALOG_IN0 */
	.temp_aux = WM97XX_AUX_ID2,	/* AD1 - ANALOG_IN1 */
	.charge_gpio = -1,
	.batt_div = 1,
	.batt_mult = 1,
	.temp_div = 1,
	.temp_mult = 1,
	.batt_name = "colibri_t20-analog_inputs",
};

static struct wm97xx_pdata colibri_t20_wm97xx_pdata = {
	.batt_pdata = &colibri_t20_adc_pdata,
};

/* Audio */

static struct platform_device colibri_t20_audio_device = {
	.name	= "colibri_t20-snd-wm9715l",
	.id	= 0,
//	.dev	= {
//		.platform_data  = &colibri_t20_audio_pdata,
//	},
};

#ifdef CAMERA_INTERFACE
/* Camera */
static struct platform_device tegra_camera = {
	.name	= "tegra_camera",
	.id	= -1,
};
#endif /* CAMERA_INTERFACE */

/* Clock */
static __initdata struct tegra_clk_init_table colibri_t20_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{"blink",	"clk_32k",	32768,		false},
	/* SMSC3340 REFCLK 24 MHz */
	{"pll_p_out4",	"pll_p",	24000000,	true},
	{"pwm",		"clk_32k",	32768,		false},
	{"i2s1",	"pll_a_out0",	0,		false},
	{"i2s2",	"pll_a_out0",	0,		false},
	{"spdif_out",	"pll_a_out0",	0,		false},

//required otherwise getting disabled by "Disabling clocks left on by bootloader" stage
	{"uarta",	"pll_p",	216000000,	true},

//required otherwise uses pll_p_out4 as parent and changing its rate to 72 MHz
	{"sclk",	"pll_p_out3",	108000000,	true },

	{"ac97",	"pll_a_out0",	24576000,	true},
	/* WM9715L XTL_IN 24.576 MHz */
//[    0.372722] Unable to set parent pll_a_out0 of clock cdev1: -38
//	{"cdev1",	"pll_a_out0",	24576000,	true},
//	{"pll_a_out0",	"pll_a",	24576000,	true},

	{"vde",		"pll_c",	240000000,	false},
//dynamic
//	{"avp.sclk",	"virt_sclk",	250000000,	false},
//dynamic
	{"apbdma",	"pclk",		36000000,	false},
	{"ndflash",	"pll_p",	144000000,	false},

//[    2.284308] kernel BUG at drivers/spi/spi-tegra.c:254!
//[    2.289454] Unable to handle kernel NULL pointer dereference at virtual address 00000000
	{"sbc4",	"pll_p",	12000000,	false},

	{NULL,		NULL,		0,		0},
};

/* GPIO */

static struct gpio colibri_t20_gpios[] = {
//conflicts with CAN interrupt on Colibri Evaluation Board and MECS Tellurium xPOD1 CAN
	{TEGRA_GPIO_PA0,	GPIOF_IN,	"SODIMM pin 73"},
	{TEGRA_GPIO_PA2,	GPIOF_IN,	"SODIMM pin 186"},
	{TEGRA_GPIO_PA3,	GPIOF_IN,	"SODIMM pin 184"},
	{TEGRA_GPIO_PB2,	GPIOF_IN,	"SODIMM pin 154"},
//conflicts with MECS Tellurium xPOD2 SSPCLK2
	{TEGRA_GPIO_PB6,	GPIOF_IN,	"SODIMM pin 55"},
//conflicts with MECS Tellurium xPOD2 SSPFRM2
	{TEGRA_GPIO_PB7,	GPIOF_IN,	"SODIMM pin 63"},
#ifndef CAMERA_INTERFACE
	{TEGRA_GPIO_PD5,	GPIOF_IN,	"SODI-98, Iris X16-13"},
	{TEGRA_GPIO_PD6,	GPIOF_IN,	"SODIMM pin 81"},
	{TEGRA_GPIO_PD7,	GPIOF_IN,	"SODIMM pin 94"},
#endif
	{TEGRA_GPIO_PI3,	GPIOF_IN,	"SODIMM pin 130"},
	{TEGRA_GPIO_PI4,	GPIOF_IN,	"SODIMM pin 87"},
	{TEGRA_GPIO_PI6,	GPIOF_IN,	"SODIMM pin 132"},
	{TEGRA_GPIO_PK0,	GPIOF_IN,	"SODIMM pin 150"},
	{TEGRA_GPIO_PK1,	GPIOF_IN,	"SODIMM pin 152"},
//conflicts with CAN reset on MECS Tellurium xPOD1 CAN
	{TEGRA_GPIO_PK4,	GPIOF_IN,	"SODIMM pin 106"},
//	{TEGRA_GPIO_PK5,	GPIOF_IN,	"USBC_DET"},
#ifndef CAMERA_INTERFACE
	{TEGRA_GPIO_PL0,	GPIOF_IN,	"SOD-101, Iris X16-16"},
	{TEGRA_GPIO_PL1,	GPIOF_IN,	"SOD-103, Iris X16-15"},
//conflicts with Ethernet interrupt on Protea
	{TEGRA_GPIO_PL2,	GPIOF_IN,	"SODI-79, Iris X16-19"},
	{TEGRA_GPIO_PL3,	GPIOF_IN,	"SODI-97, Iris X16-17"},
	{TEGRA_GPIO_PL4,	GPIOF_IN,	"SODIMM pin 67"},
	{TEGRA_GPIO_PL5,	GPIOF_IN,	"SODIMM pin 59"},
	{TEGRA_GPIO_PL6,	GPIOF_IN,	"SODI-85, Iris X16-18"},
	{TEGRA_GPIO_PL7,	GPIOF_IN,	"SODIMM pin 65"},
#endif
	{TEGRA_GPIO_PN0,	GPIOF_IN,	"SODIMM pin 174"},
	{TEGRA_GPIO_PN1,	GPIOF_IN,	"SODIMM pin 176"},
	{TEGRA_GPIO_PN2,	GPIOF_IN,	"SODIMM pin 178"},
	{TEGRA_GPIO_PN3,	GPIOF_IN,	"SODIMM pin 180"},
	{TEGRA_GPIO_PN4,	GPIOF_IN,	"SODIMM pin 160"},
	{TEGRA_GPIO_PN5,	GPIOF_IN,	"SODIMM pin 158"},
	{TEGRA_GPIO_PN6,	GPIOF_IN,	"SODIMM pin 162"},
//conflicts with ADDRESS13
	{TEGRA_GPIO_PP4,	GPIOF_IN,	"SODIMM pin 120"},
//conflicts with ADDRESS14
	{TEGRA_GPIO_PP5,	GPIOF_IN,	"SODIMM pin 122"},
//conflicts with ADDRESS15
	{TEGRA_GPIO_PP6,	GPIOF_IN,	"SODIMM pin 124"},
	{TEGRA_GPIO_PP7,	GPIOF_IN,	"SODIMM pin 188"},
#ifndef CAMERA_INTERFACE
	{TEGRA_GPIO_PT0,	GPIOF_IN,	"SODIMM pin 96"},
	{TEGRA_GPIO_PT1,	GPIOF_IN,	"SODIMM pin 75"},
	{TEGRA_GPIO_PT2,	GPIOF_IN,	"SODIMM pin 69"},
	{TEGRA_GPIO_PT3,	GPIOF_IN,	"SODIMM pin 77"},
//conflicts with BL_ON
//	{TEGRA_GPIO_PT4,	GPIOF_IN,	"SODIMM pin 71"},
#endif
//conflicts with ADDRESS12
	{TEGRA_GPIO_PU6,	GPIOF_IN,	"SODIMM pin 118"},
//conflicts with power key (WAKE1)
	{TEGRA_GPIO_PV3,	GPIOF_IN,	"SODI-45, Iris X16-20"},
	{TEGRA_GPIO_PX0,	GPIOF_IN,	"SODIMM pin 142"},
	{TEGRA_GPIO_PX1,	GPIOF_IN,	"SODIMM pin 140"},
	{TEGRA_GPIO_PX2,	GPIOF_IN,	"SODIMM pin 138"},
	{TEGRA_GPIO_PX3,	GPIOF_IN,	"SODIMM pin 136"},
	{TEGRA_GPIO_PX4,	GPIOF_IN,	"SODIMM pin 134"},
	{TEGRA_GPIO_PX6,	GPIOF_IN,	"102, I X13 ForceOFF#"},
	{TEGRA_GPIO_PX7,	GPIOF_IN,	"104, I X14 ForceOFF#"},
	{TEGRA_GPIO_PZ2,	GPIOF_IN,	"SODIMM pin 156"},
	{TEGRA_GPIO_PZ4,	GPIOF_IN,	"SODIMM pin 164"},
#ifndef SDHCI_8BIT
	{TEGRA_GPIO_PAA4,	GPIOF_IN,	"SODIMM pin 166"},
	{TEGRA_GPIO_PAA5,	GPIOF_IN,	"SODIMM pin 168"},
	{TEGRA_GPIO_PAA6,	GPIOF_IN,	"SODIMM pin 170"},
	{TEGRA_GPIO_PAA7,	GPIOF_IN,	"SODIMM pin 172"},
#endif
	{TEGRA_GPIO_PBB2,	GPIOF_IN,	"SOD-133, Iris X16-14"},
	{TEGRA_GPIO_PBB3,	GPIOF_IN,	"SODIMM pin 127"},
	{TEGRA_GPIO_PBB4,	GPIOF_IN,	"SODIMM pin 22"},
	{TEGRA_GPIO_PBB5,	GPIOF_IN,	"SODIMM pin 24"},
};

static void colibri_t20_gpio_init(void)
{
	int i = 0;
	int length = sizeof(colibri_t20_gpios) / sizeof(struct gpio);
	int err = 0;

	for (i = 0; i < length; i++) {
		err = gpio_request_one(colibri_t20_gpios[i].gpio,
				       colibri_t20_gpios[i].flags,
				       colibri_t20_gpios[i].label);

		if (err) {
			pr_warning("gpio_request(%s)failed, err = %d",
				   colibri_t20_gpios[i].label, err);
		} else {
			tegra_gpio_enable(colibri_t20_gpios[i].gpio);
			gpio_export(colibri_t20_gpios[i].gpio, true);
		}
	}
}

/* I2C*/

/* GEN1_I2C: I2C_SDA/SCL on SODIMM pin 194/196 (e.g. RTC on carrier board) */
static struct i2c_board_info colibri_t20_i2c_bus1_board_info[] = {
	{
		/* M41T0M6 real time clock on Iris carrier board */
		I2C_BOARD_INFO("rtc-ds1307", 0x68),
			.type = "m41t00",
	},
#if 0
//#ifdef CAMERA_INTERFACE
	{
		I2C_BOARD_INFO("adv7180", 0x21),
	},
	{
		I2C_BOARD_INFO("mt9v111", 0x5c),
			.platform_data = (void *)&camera_mt9v111_data,
	},
#endif /* CAMERA_INTERFACE */
};

static struct tegra_i2c_platform_data colibri_t20_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= {400000, 0},
	.slave_addr	= 0x00FC,
	.scl_gpio	= {TEGRA_GPIO_PC4, 0},	/* I2C_SDA */
	.sda_gpio	= {TEGRA_GPIO_PC5, 0},	/* I2C_SCL */
	.arb_recovery	= arb_lost_recovery,
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

/* GEN2_I2C: SODIMM pin 93/99 */
static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data colibri_t20_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate	= {10000, 10000},
	.bus_mux	= {&i2c2_ddc, &i2c2_gen2},
	.bus_mux_len	= {1, 1},
	.slave_addr	= 0x00FC,
	.scl_gpio	= {0, TEGRA_GPIO_PT5},
	.sda_gpio	= {0, TEGRA_GPIO_PT6},
	.arb_recovery	= arb_lost_recovery,
};

/* CAM_I2C SODIMM pin 127/133 */
static struct tegra_i2c_platform_data colibri_t20_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= {400000, 0},
	.slave_addr	= 0x00FC,
	.scl_gpio	= {TEGRA_GPIO_PBB2, 0},
	.sda_gpio	= {TEGRA_GPIO_PBB3, 0},
	.arb_recovery	= arb_lost_recovery,
};

/* PWR_I2C: power I2C to PMIC and temperature sensor */
static struct i2c_board_info colibri_t20_i2c_bus4_board_info[] __initdata = {
	{
		/* LM95245 temperature sensor on PWR_I2C_SCL/SDA */
		I2C_BOARD_INFO("lm95245", 0x4c),
	},
};

static struct tegra_i2c_platform_data colibri_t20_dvc_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= {400000, 0},
	.is_dvc		= true,
	.scl_gpio	= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio	= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery	= arb_lost_recovery,
};

static void colibri_t20_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &colibri_t20_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &colibri_t20_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &colibri_t20_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &colibri_t20_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);

	i2c_register_board_info(0, colibri_t20_i2c_bus1_board_info, ARRAY_SIZE(colibri_t20_i2c_bus1_board_info));
	i2c_register_board_info(4, colibri_t20_i2c_bus4_board_info, ARRAY_SIZE(colibri_t20_i2c_bus4_board_info));
}

/* Keys */

/* MMC/SD */

static struct tegra_sdhci_platform_data colibri_t20_sdhci_platform_data = {
	.cd_gpio	= TEGRA_GPIO_PC7, /* MM_CD */
#ifndef SDHCI_8BIT
	.is_8bit	= 0,
#else
	.is_8bit	= 1,
#endif
	.power_gpio	= -1,
	.wp_gpio	= -1,
};

int __init colibri_t20_sdhci_init(void)
{
	tegra_gpio_enable(colibri_t20_sdhci_platform_data.cd_gpio);

	tegra_sdhci_device4.dev.platform_data =
			&colibri_t20_sdhci_platform_data;
	platform_device_register(&tegra_sdhci_device4);

	return 0;
}

/* NAND */

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Micron MT29F4G08ABBDAH4 */
	[0] = {
		.vendor_id		= 0x2C,
		.device_id		= 0xAC,
		.read_id_fourth_byte	= 0x15,
		.capacity		= 512,
		.timing = {
			.trp		= 12,
			.trh		= 10,	/* tREH */
			.twp		= 12,
			.twh		= 10,
			.tcs		= 20,	/* Max(tCS, tCH, tALS, tALH) */
			.twhr		= 80,
			.tcr_tar_trr	= 20,	/* Max(tCR, tAR, tRR) */
			.twb		= 100,
			.trp_resp	= 12,	/* tRP */
			.tadl		= 70,
		},
	},
	/* Micron MT29F4G08ABBEAH4 */
	[1] = {
		.vendor_id		= 0x2C,
		.device_id		= 0xAC,
		.read_id_fourth_byte	= 0x26,
		.capacity		= 512,
		.timing = {
			.trp		= 15,
			.trh		= 10,	/* tREH */
			.twp		= 15,
			.twh		= 10,
			.tcs		= 25,	/* Max(tCS, tCH, tALS, tALH) */
			.twhr		= 80,
			.tcr_tar_trr	= 20,	/* Max(tCR, tAR, tRR) */
			.twb		= 100,
			.trp_resp	= 15,	/* tRP */
			.tadl		= 100,
		},
	},
	/* Micron MT29F8G08ABCBB on Colibri T20 before V1.2 */
	[2] = {
		.vendor_id		= 0x2C,
		.device_id		= 0x38,
		.read_id_fourth_byte	= 0x26,
		.capacity		= 1024,
		.timing = {
			/* timing mode 4 */
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
	/* Micron MT29F8G08ADBDAH4 */
	[3] = {
		.vendor_id		= 0x2C,
		.device_id		= 0xA3,
		.read_id_fourth_byte	= 0x15,
		.capacity		= 1024,
		.timing = {
			.trp		= 12,
			.trh		= 10,	/* tREH */
			.twp		= 12,
			.twh		= 10,
			.tcs		= 20,	/* Max(tCS, tCH, tALS, tALH) */
			.twhr		= 80,
			.tcr_tar_trr	= 20,	/* Max(tCR, tAR, tRR) */
			.twb		= 100,
			.trp_resp	= 12,	/* tRP */
			.tadl		= 70,
		},
	},
	/* Micron MT29F8G08ABBCA */
	[4] = {
		.vendor_id		= 0x2C,
		.device_id		= 0xA3,
		.read_id_fourth_byte	= 0x26,
		.capacity		= 1024,
		.timing = {
			.trp		= 15,
			.trh		= 10,	/* tREH */
			.twp		= 15,
			.twh		= 10,
			.tcs		= 25,	/* Max(tCS, tCH, tALS, tALH) */
			.twhr		= 80,
			.tcr_tar_trr	= 20,	/* Max(tCR, tAR, tRR) */
			.twb		= 100,
			.trp_resp	= 15,	/* tRP */
			.tadl		= 100,
		},
	},
};

struct tegra_nand_platform colibri_t20_nand_data = {
	.max_chips	= 8,
	.chip_parms	= nand_chip_parms,
	.nr_chip_parms	= ARRAY_SIZE(nand_chip_parms),
	.wp_gpio	= TEGRA_GPIO_PS0,
};

static struct resource resources_nand[] = {
	[0] = {
		.start	= INT_NANDFLASH,
		.end	= INT_NANDFLASH,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_nand_device = {
	.name		= "tegra_nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_nand),
	.resource	= resources_nand,
	.dev		= {
		.platform_data = &colibri_t20_nand_data,
	},
};

/* RTC */
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

/* SPI */

#if defined(CONFIG_SPI_TEGRA) && defined(CONFIG_SPI_SPIDEV)
static struct spi_board_info tegra_spi_devices[] __initdata = {
	{
		.bus_num	= 3,
		.chip_select	= 0,
		.irq		= 0,
		.max_speed_hz	= 50000000,
		.modalias	= "spidev",
		.mode		= SPI_MODE_0,
		.platform_data	= NULL,
	},
};

static void __init colibri_t20_register_spidev(void)
{
	spi_register_board_info(tegra_spi_devices,
				ARRAY_SIZE(tegra_spi_devices));
}
#else /* CONFIG_SPI_TEGRA && CONFIG_SPI_SPIDEV */
#define colibri_t20_register_spidev() do {} while (0)
#endif /* CONFIG_SPI_TEGRA && CONFIG_SPI_SPIDEV */

/* UART */

static struct platform_device *colibri_t20_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartd_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
#if 1
	[0] = {.name = "pll_m"},
	[1] = {.name = "pll_p"},
	[2] = {.name = "clk_m"},
#else
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "pll_p"},
#endif
};

static struct tegra_uart_platform_data colibri_t20_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *c;

	/* UARTA is the debug port. */
	pr_info("Selecting UARTA as the debug console\n");
	colibri_t20_uart_devices[0] = &debug_uarta_device;
	debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
	debug_uart_clk = clk_get_sys("serial8250.0", "uarta");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->uartclk;
		pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_p");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_p\n");
		else
			clk_set_parent(debug_uart_clk, c);

		clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, rate);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
	}
}

static void __init colibri_t20_uart_init(void)
{
	int i;
	struct clk *c;

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
	colibri_t20_uart_pdata.parent_clk_list = uart_parent_clk;
	colibri_t20_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uarta_device.dev.platform_data = &colibri_t20_uart_pdata;
	tegra_uartb_device.dev.platform_data = &colibri_t20_uart_pdata;
	tegra_uartd_device.dev.platform_data = &colibri_t20_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(colibri_t20_uart_devices,
				ARRAY_SIZE(colibri_t20_uart_devices));
}

/* USB */

//overcurrent?

//USB1_IF_USB_PHY_VBUS_WAKEUP_ID_0
//Offset: 408h 
//ID_PU: ID pullup enable. Set to 1.

static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
//bias 6
		.elastic_limit		= 16,
		.hssync_start_delay	= 0,
//		.hssync_start_delay	= 9,
		.idle_wait_delay	= 17,
//squelch 2
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 15,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
	[1] = {
//bias 0
		.elastic_limit		= 16,
		.hssync_start_delay	= 0,
//		.hssync_start_delay	= 9,
		.idle_wait_delay	= 17,
//squelch 2
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 8,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
};

static struct tegra_ulpi_config colibri_t20_ehci2_ulpi_phy_config = {
	.clk		= "cdev2",
	.reset_gpio	= TEGRA_GPIO_PV1, /* USB3340 RESETB */
};

static struct tegra_ehci_platform_data colibri_t20_ehci2_ulpi_platform_data = {
	.operating_mode			= TEGRA_USB_HOST,
	.phy_config			= &colibri_t20_ehci2_ulpi_phy_config,
	.phy_type			= TEGRA_USB_PHY_TYPE_LINK_ULPI,
	.power_down_on_bus_suspend	= 1,
};

static struct usb_phy_plat_data tegra_usb_phy_pdata[] = {
	[0] = {
		.instance	= 0,
		.vbus_gpio	= -1,
	},
	[1] = {
		.instance	= 1,
		.vbus_gpio	= -1,
//		.vbus_gpio	= TEGRA_GPIO_PBB1,	/* ETHERNET_VBUS_GPIO */
	},
	[2] = {
		.instance		= 2,
		.vbus_gpio		= TEGRA_GPIO_PW2,	/* USBH_PEN */
		.vbus_gpio_inverted	= 1,
	},
};

static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
		.phy_config			= &utmi_phy_config[0],
		.operating_mode			= TEGRA_USB_OTG,
		.power_down_on_bus_suspend	= 0, /* otherwise might prevent enumeration */
	},
	[1] = {
		.phy_config			= &colibri_t20_ehci2_ulpi_phy_config,
		.operating_mode			= TEGRA_USB_HOST,
		.power_down_on_bus_suspend	= 1,
		.phy_type			= TEGRA_USB_PHY_TYPE_LINK_ULPI,
	},
	[2] = {
		.phy_config			= &utmi_phy_config[1],
		.operating_mode			= TEGRA_USB_HOST,
		.power_down_on_bus_suspend	= 0, /* otherwise might prevent enumeration */
		.hotplug			= 1,
	},
};

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

	platform_data = kmalloc(sizeof(struct tegra_ehci_platform_data),
				GFP_KERNEL);
	if (!platform_data)
		goto error;

	memcpy(platform_data, &tegra_ehci_pdata[0],
	       sizeof(struct tegra_ehci_platform_data));
	pdev->dev.platform_data = platform_data;

	val = platform_device_add(pdev);
	if (val)
		goto error_add;

	return pdev;

error_add:
	kfree(platform_data);
error:
	pr_err("%s: failed to add the host contoller device\n", __func__);
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
	.cable_detect_gpio	= USB_CABLE_DETECT_GPIO,
	.host_register		= &tegra_usb_otg_host_register,
	.host_unregister	= &tegra_usb_otg_host_unregister,
};

struct platform_device colibri_otg_device = {
	.name	= "colibri-otg",
	.id	= -1,
	.dev = {
		.platform_data = &colibri_otg_pdata,
	},
};

static void colibri_t20_usb_init(void)
{
#ifdef CONFIG_USB_SUPPORT
	tegra_usb_phy_init(tegra_usb_phy_pdata, ARRAY_SIZE(tegra_usb_phy_pdata));
#endif

	/* OTG should be the first to be registered
	   EHCI instance 0: USB1_DP/N -> USBOTG_P/N */
	platform_device_register(&colibri_otg_device);
	platform_device_register(&tegra_udc_device);

	/* EHCI instance 1: ULPI PHY -> ASIX ETH */
	tegra_ehci2_device.dev.platform_data = &tegra_ehci_pdata[1];
	platform_device_register(&tegra_ehci2_device);

	/* EHCI instance 2: USB3_DP/N -> USBH1_P/N */
	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];
	platform_device_register(&tegra_ehci3_device);

#ifdef MECS_TELLURIUM
//SD card multiplexing: pull GPIO_PT2 and GPIO_PBB2 low
//working even without any modifications
	{
		int gpio_status;
		unsigned int i2c_scl = TEGRA_GPIO_PC5;
		unsigned int i2c_sda = TEGRA_GPIO_PC4;
		unsigned int tellurium_usb_hub_reset = TEGRA_GPIO_PBB3;

		printk("MECS Tellurium USB Hub Initialisation\n");

		/* configure USB hub reset line as output and pull low into reset */
		gpio_status = gpio_request(tellurium_usb_hub_reset, "USB_HUB_RESET");
		if (gpio_status < 0)
			pr_warning("USB_HUB_RESET request GPIO FAILED\n");
		tegra_gpio_enable(tellurium_usb_hub_reset);
		gpio_status = gpio_direction_output(tellurium_usb_hub_reset, 0);
		if (gpio_status < 0)
			pr_warning("USB_HUB_RESET request GPIO DIRECTION FAILED\n");

		/* configure I2C pins as outputs and pull low */
		tegra_gpio_enable(i2c_scl);
		gpio_status = gpio_direction_output(i2c_scl, 0);
		if (gpio_status < 0)
			pr_warning("I2C_SCL request GPIO DIRECTION FAILED\n");
		tegra_gpio_enable(i2c_sda);
		gpio_status = gpio_direction_output(i2c_sda, 0);
		if (gpio_status < 0)
			pr_warning("I2C_SDA request GPIO DIRECTION FAILED\n");

		/* pull USB hub out of reset */
		gpio_set_value(tellurium_usb_hub_reset, 1);

		/* release I2C pins again */
		tegra_gpio_disable(i2c_scl);
		tegra_gpio_disable(i2c_sda);
	}
#endif /* MECS_TELLURIUM */
}

static struct platform_device *colibri_t20_devices[] __initdata = {
	&tegra_rtc_device,
	&tegra_nand_device,

	&tegra_pmu_device,
	&tegra_gart_device,
	&tegra_aes_device,
#ifdef CONFIG_KEYBOARD_GPIO
//	&colibri_t20_keys_device,
#endif
	&tegra_wdt_device,
	&tegra_avp_device,
#ifdef CAMERA_INTERFACE
	&tegra_camera,
#endif
	&tegra_ac97_device,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,
//bluetooth
	&tegra_pcm_device,
	&colibri_t20_audio_device,

	&tegra_spi_device4,
};

static void __init tegra_colibri_t20_init(void)
{
	tegra_clk_init_from_table(colibri_t20_clk_init_table);
	colibri_t20_pinmux_init();
	colibri_t20_i2c_init();
	colibri_t20_uart_init();
//
	tegra_ac97_device.dev.platform_data = &colibri_t20_wm97xx_pdata;
//
	tegra_ehci2_device.dev.platform_data
		= &colibri_t20_ehci2_ulpi_platform_data;
	platform_add_devices(colibri_t20_devices,
			     ARRAY_SIZE(colibri_t20_devices));
	tegra_ram_console_debug_init();
	colibri_t20_sdhci_init();
	colibri_t20_regulator_init();

//	tegra_das_device.dev.platform_data = &tegra_das_pdata;
//	tegra_ac97_device.dev.platform_data = &tegra_audio_pdata;
//	tegra_spdif_input_device.name = "spdif";
//	tegra_spdif_input_device.dev.platform_data = &tegra_spdif_audio_pdata;

#ifdef CONFIG_KEYBOARD_GPIO
//keys
#endif

	colibri_t20_usb_init();
	colibri_t20_panel_init();
//sensors

	/* Note: V1.1c modules require proper BCT setting 666 rather than
	   721.5 MHz EMC clock */
	colibri_t20_emc_init();

	colibri_t20_gpio_init();
	colibri_t20_register_spidev();

	tegra_release_bootloader_fb();
}

int __init tegra_colibri_t20_protected_aperture_init(void)
{
	if (!machine_is_colibri_t20())
		return 0;

	tegra_protected_aperture_init(tegra_grhost_aperture);
	return 0;
}
late_initcall(tegra_colibri_t20_protected_aperture_init);

void __init tegra_colibri_t20_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	/* we specify zero for special handling due to already reserved
	   fbmem/nvmem (U-Boot 2011.06 compatibility from our V1.x images) */
	tegra_reserve(0, SZ_8M + SZ_1M, SZ_16M);
}

MACHINE_START(COLIBRI_T20, "Toradex Colibri T20")
	.boot_params	= 0x00000100,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.init_machine	= tegra_colibri_t20_init,
	.map_io		= tegra_map_common_io,
	.reserve        = tegra_colibri_t20_reserve,
	.timer		= &tegra_timer,
MACHINE_END
