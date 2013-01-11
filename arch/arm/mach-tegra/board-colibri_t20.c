/*
 * arch/arm/mach-tegra/board-colibri_t20.c
 *
 * Copyright (c) 2011-2013 Toradex, Inc.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/colibri_usb.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/leds_pwm.h>
#include <linux/lm95245.h>
#include <linux/memblock.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/serial_8250.h>
#include <linux/spi/spi.h>
#include <linux/tegra_uart.h>
#include <linux/wm97xx.h>

#include <mach/gpio.h>
#include <mach/nand.h>
#include <mach/sdhci.h>
#include <mach/usb_phy.h>
#include <mach/w1.h>

#include "board-colibri_t20.h"
#include "board.h"
#include "clock.h"
#include "cpu-tegra.h"
#include "devices.h"
#include "gpio-names.h"
#include "pm.h"
#include "wakeups-t2.h"

/* ADC */

static struct wm97xx_batt_pdata colibri_t20_adc_pdata = {
	.batt_aux	= WM97XX_AUX_ID1,	/* AD0 - ANALOG_IN0 */
	.temp_aux	= WM97XX_AUX_ID2,	/* AD1 - ANALOG_IN1 */
	.charge_gpio	= -1,
	.batt_div	= 1,
	.batt_mult	= 1,
	.temp_div	= 1,
	.temp_mult	= 1,
	.batt_name	= "colibri_t20-analog_inputs",
};

static struct wm97xx_pdata colibri_t20_wm97xx_pdata = {
	.batt_pdata = &colibri_t20_adc_pdata,
};

/* Audio */

static struct platform_device colibri_t20_audio_device = {
	.name	= "colibri_t20-snd-wm9715l",
	.id	= 0,
};

void *get_colibri_t20_audio_platform_data(void)
{
	return &colibri_t20_wm97xx_pdata;
}
EXPORT_SYMBOL(get_colibri_t20_audio_platform_data);

#ifdef CONFIG_TEGRA_CAMERA
/* Camera */
static struct platform_device tegra_camera = {
	.name	= "tegra_camera",
	.id	= -1,
};
#endif /* CONFIG_TEGRA_CAMERA */

/* Clocks */
static struct tegra_clk_init_table colibri_t20_clk_init_table[] __initdata = {
	/* name		parent		rate		enabled */
	{"blink",	"clk_32k",	32768,		false},
	/* SMSC3340 REFCLK 24 MHz */
	{"pll_p_out4",	"pll_p",	24000000,	true},
	{"pwm",		"clk_m",	0,		false},
	{"spdif_out",	"pll_a_out0",	0,		false},

//required otherwise getting disabled by "Disabling clocks left on by bootloader" stage
	{"uarta",	"pll_p",	216000000,	true},

//required otherwise uses pll_p_out4 as parent and changing its rate to 72 MHz
	{"sclk",	"pll_p_out3",	108000000,	true},

	/* AC97 incl. touch (note: unfortunately no clk source mux exists) */
	{"ac97",	"pll_a_out0",	24576000,	true},

	/* WM9715L XTL_IN 24.576 MHz */
//[    0.372722] Unable to set parent pll_a_out0 of clock cdev1: -38
//	{"cdev1",	"pll_a_out0",	24576000,	true},
//	{"pll_a_out0",	"pll_a",	24576000,	true},

	{"vde",		"pll_c",	240000000,	false},

	{"ndflash",	"pll_p",	108000000,	false},

//[    2.284308] kernel BUG at drivers/spi/spi-tegra.c:254!
//[    2.289454] Unable to handle kernel NULL pointer dereference at virtual address 00000000
	{"sbc4",	"pll_p",	12000000,	false},

	{NULL,		NULL,		0,		0},
};

/* GPIO */

#define FF_DCD		TEGRA_GPIO_PC6	/* SODIMM 31 */
#define FF_DSR		TEGRA_GPIO_PC1	/* SODIMM 29 */

#define I2C_SCL		TEGRA_GPIO_PC4	/* SODIMM 196 */
#define I2C_SDA		TEGRA_GPIO_PC5	/* SODIMM 194 */

#define LAN_EXT_WAKEUP	TEGRA GPIO_PV5
#define LAN_PME		TEGRA_GPIO_PV6
#define LAN_RESET	TEGRA_GPIO_PV4
#define LAN_V_BUS	TEGRA_GPIO_PBB1

#define MMC_CD		TEGRA_GPIO_PC7	/* SODIMM 43 */

#define NAND_WP_N	TEGRA_GPIO_PS0

#define PWR_I2C_SCL	TEGRA_GPIO_PZ6
#define PWR_I2C_SDA	TEGRA_GPIO_PZ7

#define MECS_USB_HUB_RESET	TEGRA_GPIO_PBB3	/* SODIMM 127 */

#define THERMD_ALERT	TEGRA_GPIO_PV7

#define TOUCH_PEN_INT	TEGRA_GPIO_PV2

#define USB3340_RESETB	TEGRA_GPIO_PV1
//conflicts with MECS Tellurium xPOD2 SSPTXD2
#define USBC_DET	TEGRA_GPIO_PK5	/* SODIMM 137 */
#define USBH_OC		TEGRA_GPIO_PW3	/* SODIMM 131 */
#define USBH_PEN	TEGRA_GPIO_PW2	/* SODIMM 129 */

static struct gpio colibri_t20_gpios[] = {
//conflicts with CAN interrupt on Colibri Evaluation Board and MECS Tellurium xPOD1 CAN
//conflicts with DAC_PSAVE# on Iris
#ifndef IRIS
	{TEGRA_GPIO_PA0,	GPIOF_IN,	"SODIMM pin 73"},
#endif
	{TEGRA_GPIO_PA2,	GPIOF_IN,	"SODIMM pin 186"},
	{TEGRA_GPIO_PA3,	GPIOF_IN,	"SODIMM pin 184"},
//multiplexed VI_D6
	{TEGRA_GPIO_PA7,	GPIOF_IN,	"SODIMM pin 67"},
	{TEGRA_GPIO_PB2,	GPIOF_IN,	"SODIMM pin 154"},
//multiplexed VI_D7
	{TEGRA_GPIO_PB4,	GPIOF_IN,	"SODIMM pin 59"},
//conflicts with MECS Tellurium xPOD2 SSPCLK2
	{TEGRA_GPIO_PB6,	GPIOF_IN,	"SODIMM pin 55"},
//conflicts with MECS Tellurium xPOD2 SSPFRM2
	{TEGRA_GPIO_PB7,	GPIOF_IN,	"SODIMM pin 63"},
#ifndef COLIBRI_T20_VI
	{TEGRA_GPIO_PD5,	GPIOF_IN,	"SODI-98, Iris X16-13"},
	{TEGRA_GPIO_PD6,	GPIOF_IN,	"SODIMM pin 81"},
	{TEGRA_GPIO_PD7,	GPIOF_IN,	"SODIMM pin 94"},
#endif
	{TEGRA_GPIO_PI3,	GPIOF_IN,	"SODIMM pin 130"},
	{TEGRA_GPIO_PI6,	GPIOF_IN,	"SODIMM pin 132"},
	{TEGRA_GPIO_PK0,	GPIOF_IN,	"SODIMM pin 150"},
//multiplexed OWR
	{TEGRA_GPIO_PK1,	GPIOF_IN,	"SODIMM pin 152"},
//conflicts with CAN reset on MECS Tellurium xPOD1 CAN
	{TEGRA_GPIO_PK4,	GPIOF_IN,	"SODIMM pin 106"},
//	{TEGRA_GPIO_PK5,	GPIOF_IN,	"USBC_DET"},
#ifndef CONFIG_KEYBOARD_GPIO
	{TEGRA_GPIO_PK6,	GPIOF_IN,	"SODIMM pin 135"},
#endif
#ifndef COLIBRI_T20_VI
	{TEGRA_GPIO_PL0,	GPIOF_IN,	"SOD-101, Iris X16-16"},
	{TEGRA_GPIO_PL1,	GPIOF_IN,	"SOD-103, Iris X16-15"},
//conflicts with Ethernet interrupt on Protea
	{TEGRA_GPIO_PL2,	GPIOF_IN,	"SODI-79, Iris X16-19"},
	{TEGRA_GPIO_PL3,	GPIOF_IN,	"SODI-97, Iris X16-17"},
	{TEGRA_GPIO_PL6,	GPIOF_IN,	"SODI-85, Iris X16-18"},
	{TEGRA_GPIO_PL7,	GPIOF_IN,	"SODIMM pin 65"},
#endif

//multiplexed SPI2_CS0_N, SPI2_MISO, SPI2_MOSI and SPI2_SCK
	{TEGRA_GPIO_PM2,	GPIOF_IN,	"SODIMM pin 136"},
	{TEGRA_GPIO_PM3,	GPIOF_IN,	"SODIMM pin 138"},
	{TEGRA_GPIO_PM4,	GPIOF_IN,	"SODIMM pin 140"},
	{TEGRA_GPIO_PM5,	GPIOF_IN,	"SODIMM pin 142"},

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
#ifndef COLIBRI_T20_VI
	{TEGRA_GPIO_PT0,	GPIOF_IN,	"SODIMM pin 96"},
	{TEGRA_GPIO_PT1,	GPIOF_IN,	"SODIMM pin 75"},
#endif
	{TEGRA_GPIO_PT2,	GPIOF_IN,	"SODIMM pin 69"},
#ifndef CONFIG_KEYBOARD_GPIO
//conflicts with find key
	{TEGRA_GPIO_PT3,	GPIOF_IN,	"SODIMM pin 77"},
#endif
//conflicts with BL_ON
//	{TEGRA_GPIO_PT4,	GPIOF_IN,	"SODIMM pin 71"},
//conflicts with ADDRESS12
	{TEGRA_GPIO_PU6,	GPIOF_IN,	"SODIMM pin 118"},
#ifndef CONFIG_KEYBOARD_GPIO
//conflicts with power key (WAKE1)
	{TEGRA_GPIO_PV3,	GPIOF_IN,	"SODI-45, Iris X16-20"},
#endif

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
#ifndef CONFIG_KEYBOARD_GPIO
//conflicts with back key
	{TEGRA_GPIO_PBB2,	GPIOF_IN,	"SOD-133, Iris X16-14"},
//conflicts with home key
	{TEGRA_GPIO_PBB3,	GPIOF_IN,	"SODIMM pin 127"},
//conflicts with volume up key
	{TEGRA_GPIO_PBB4,	GPIOF_IN,	"SODIMM pin 22"},
//conflicts with volume down key
	{TEGRA_GPIO_PBB5,	GPIOF_IN,	"SODIMM pin 24"},
#endif
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
			pr_warning("gpio_request(%s) failed, err = %d",
				   colibri_t20_gpios[i].label, err);
		} else {
			gpio_export(colibri_t20_gpios[i].gpio, true);
		}
	}
}

/* I2C */

/* GEN1_I2C: I2C_SDA/SCL on SODIMM pin 194/196 (e.g. RTC on carrier board) */
static struct i2c_board_info colibri_t20_i2c_bus1_board_info[] __initdata = {
	{
		/* M41T0M6 real time clock on Iris carrier board */
		I2C_BOARD_INFO("rtc-ds1307", 0x68),
			.type = "m41t00",
	},
#ifdef CONFIG_VIDEO_ADV7180
	{
		I2C_BOARD_INFO("adv7180", 0x21),
	},
#endif /* CONFIG_VIDEO_ADV7180 */
#ifdef CONFIG_VIDEO_MT9V111
	{
		I2C_BOARD_INFO("mt9v111", 0x5c),
			.platform_data = (void *)&camera_mt9v111_data,
	},
#endif /* CONFIG_VIDEO_MT9V111 */
};

static struct tegra_i2c_platform_data colibri_t20_i2c1_platform_data = {
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
static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data colibri_t20_i2c2_platform_data = {
	.adapter_nr	= 1,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {10000, 10000},
	.bus_count	= 1,
	.slave_addr	= 0x00FC,
};

/* PWR_I2C: power I2C to PMIC and temperature sensor */

static void lm95245_probe_callback(struct device *dev);

static struct lm95245_platform_data colibri_t20_lm95245_pdata = {
	.enable_os_pin	= true,
	.probe_callback	= lm95245_probe_callback,
};

static struct i2c_board_info colibri_t20_i2c_bus4_board_info[] __initdata = {
	{
		/* LM95245 temperature sensor */
		I2C_BOARD_INFO("lm95245", 0x4c),
			.platform_data = &colibri_t20_lm95245_pdata,
	},
};

static struct tegra_i2c_platform_data colibri_t20_dvc_platform_data = {
	.adapter_nr	= 4,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {400000, 0},
	.bus_count	= 1,
	.is_dvc		= true,
	.scl_gpio	= {PWR_I2C_SCL, 0},
	.sda_gpio	= {PWR_I2C_SDA, 0},
};

static void colibri_t20_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &colibri_t20_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &colibri_t20_i2c2_platform_data;
	tegra_i2c_device4.dev.platform_data = &colibri_t20_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device4);

	i2c_register_board_info(0, colibri_t20_i2c_bus1_board_info, ARRAY_SIZE(colibri_t20_i2c_bus1_board_info));
	i2c_register_board_info(4, colibri_t20_i2c_bus4_board_info, ARRAY_SIZE(colibri_t20_i2c_bus4_board_info));
}

/* Keys
   Note: active-low means pull-ups required on carrier board resp. via pin-muxing
   Note2: power-key active-high due to EvalBoard v3.1a having 100 K pull-down on SODIMM pin 45 */

#ifdef CONFIG_KEYBOARD_GPIO
#define GPIO_KEY(_id, _gpio, _lowactive, _iswake)	\
	{						\
		.code = _id,				\
		.gpio = TEGRA_GPIO_##_gpio,		\
		.active_low = _lowactive,		\
		.desc = #_id,				\
		.type = EV_KEY,				\
		.wakeup = _iswake,			\
		.debounce_interval = 10,		\
	}

static struct gpio_keys_button colibri_t20_keys[] = {
	[0] = GPIO_KEY(KEY_FIND, PT3, 1, 0),		/* SODIMM pin 77 */
	[1] = GPIO_KEY(KEY_HOME, PBB3, 1, 0),		/* SODIMM pin 127 */
	[2] = GPIO_KEY(KEY_BACK, PBB2, 1, 0),		/* SODIMM pin 133, Iris X16-14 */
	[3] = GPIO_KEY(KEY_VOLUMEUP, PBB4, 1, 0),	/* SODIMM pin 22 */
	[4] = GPIO_KEY(KEY_VOLUMEDOWN, PBB5, 1, 0),	/* SODIMM pin 24 */
	[5] = GPIO_KEY(KEY_POWER, PV3, 0, 1),		/* SODIMM pin 45, Iris X16-20 */
	[6] = GPIO_KEY(KEY_MENU, PK6, 1, 0),		/* SODIMM pin 135 */
};

#define PMC_WAKE_STATUS 0x14

static int colibri_t20_wakeup_key(void)
{
	unsigned long status =
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

	return (status & (1 << TEGRA_WAKE_GPIO_PV3)) ?
		KEY_POWER : KEY_RESERVED;
}

static struct gpio_keys_platform_data colibri_t20_keys_platform_data = {
	.buttons	= colibri_t20_keys,
	.nbuttons	= ARRAY_SIZE(colibri_t20_keys),
	.wakeup_key	= colibri_t20_wakeup_key,
};

static struct platform_device colibri_t20_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev = {
		.platform_data = &colibri_t20_keys_platform_data,
	},
};
#endif /* CONFIG_KEYBOARD_GPIO */

/* MMC/SD */

static struct tegra_sdhci_platform_data colibri_t20_sdhci_platform_data = {
	.cd_gpio	= MMC_CD,
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
	/* Samsung K9K8G08U0B */
	[5] = {
		.vendor_id		= 0xec,
		.device_id		= 0xd3,
		.read_id_fourth_byte	= 0x95,
		.capacity		= 1024,
		.timing			= {
			.trp		= 12,	/* tRP, ND_nRE pulse width */
			.trh		= 100,	/* tRHZ, ND_nRE high duration */
			.twp		= 12,	/* tWP, ND_nWE pulse time */
			.twh		= 10,	/* tWH, ND_nWE high duration */
			.tcs		= 20,	/* Max(tCS, tCH, tALS, tALH) */
			.twhr		= 60,	/* tWHR, ND_nWE high to ND_nRE low delay for
		                                   status read */
			.tcr_tar_trr	= 20,	/* Max(tCR, tAR, tRR) */
			.twb		= 100,
			.trp_resp	= 12,	/* tRP */
			.tadl		= 70,
		},
	},
};

struct tegra_nand_platform colibri_t20_nand_data = {
	.max_chips	= 8,
	.chip_parms	= nand_chip_parms,
	.nr_chip_parms	= ARRAY_SIZE(nand_chip_parms),
	.wp_gpio	= NAND_WP_N,
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
	.dev = {
		.platform_data = &colibri_t20_nand_data,
	},
};

/* PWM LEDs */
static struct led_pwm tegra_leds_pwm[] = {
	{
		.name		= "pwm_b",
		.pwm_id		= 1,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
#ifndef MECS_TELLURIUM
	{
		.name		= "pwm_c",
		.pwm_id		= 2,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
#else /* MECS_TELLURIUM */
	{
		.name		= "pwm_a",
		.pwm_id		= 0,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
#endif /* MECS_TELLURIUM */
	{
		.name		= "pwm_d",
		.pwm_id		= 3,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
};

static struct led_pwm_platform_data tegra_leds_pwm_data = {
	.num_leds	= ARRAY_SIZE(tegra_leds_pwm),
	.leds		= tegra_leds_pwm,
};

static struct platform_device tegra_led_pwm_device = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev = {
		.platform_data = &tegra_leds_pwm_data,
	},
};

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
		.bus_num	= 3,		/* SPI4 */
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
#else /* CONFIG_SPI_TEGRA & CONFIG_SPI_SPIDEV */
#define colibri_t20_register_spidev() do {} while (0)
#endif /* CONFIG_SPI_TEGRA & CONFIG_SPI_SPIDEV */

/* Thermal throttling
   Note: As our hardware only allows triggering an interrupt on
	 over-temperature shutdown we first use it to catch entering throttle
	 and only then set it up to catch an actual over-temperature shutdown.
	 While throttling we setup a workqueue to catch leaving it again. */

static int colibri_t20_shutdown_temp = 115000;
static int colibri_t20_throttle_hysteresis = 3000;
static int colibri_t20_throttle_temp = 90000;
static struct device *lm95245_device = NULL;
static int thermd_alert_irq_disabled = 0;
struct work_struct thermd_alert_work;
struct workqueue_struct *thermd_alert_workqueue;

/* Over-temperature shutdown OS pin GPIO interrupt handler */
static irqreturn_t thermd_alert_irq(int irq, void *data)
{
	disable_irq_nosync(irq);
	thermd_alert_irq_disabled = 1;
	queue_work(thermd_alert_workqueue, &thermd_alert_work);

	return IRQ_HANDLED;
}

/* Gets both entered by THERMD_ALERT GPIO interrupt as well as re-scheduled
   while throttling. */
static void thermd_alert_work_func(struct work_struct *work)
{
	int temp = 0;

	lm95245_get_remote_temp(lm95245_device, &temp);

	if (temp > colibri_t20_shutdown_temp) {
		/* First check for hardware over-temperature condition mandating
		   immediate shutdown */
		pr_err("over-temperature condition %d degC reached, initiating "
				"immediate shutdown", temp);
		kernel_power_off();
	} else if (temp < colibri_t20_throttle_temp - colibri_t20_throttle_hysteresis) {
		/* Make sure throttling gets disabled again */
		if (tegra_is_throttling()) {
			tegra_throttling_enable(false);
			lm95245_set_remote_os_limit(lm95245_device, colibri_t20_throttle_temp);
		}
	} else if (temp < colibri_t20_throttle_temp) {
		/* Operating within hysteresis so keep re-scheduling to catch
		   leaving below throttle again */
		if (tegra_is_throttling()) {
			msleep(100);
			queue_work(thermd_alert_workqueue, &thermd_alert_work);
		}
	} else if (temp >= colibri_t20_throttle_temp) {
		/* Make sure throttling gets enabled and set shutdown limit */
		if (!tegra_is_throttling()) {
			tegra_throttling_enable(true);
			lm95245_set_remote_os_limit(lm95245_device, colibri_t20_shutdown_temp);
		}
		/* And re-schedule again */
		msleep(100);
		queue_work(thermd_alert_workqueue, &thermd_alert_work);
	}

	/* Avoid unbalanced enable for IRQ 367 */
	if (thermd_alert_irq_disabled) {
		thermd_alert_irq_disabled = 0;
		enable_irq(TEGRA_GPIO_TO_IRQ(THERMD_ALERT));
	}
}

static void colibri_t20_thermd_alert_init(void)
{
	gpio_request(THERMD_ALERT, "THERMD_ALERT");
	gpio_direction_input(THERMD_ALERT);

	thermd_alert_workqueue = create_singlethread_workqueue("THERMD_ALERT");

	INIT_WORK(&thermd_alert_work, thermd_alert_work_func);
}

static void lm95245_probe_callback(struct device *dev)
{
	lm95245_device = dev;

	lm95245_set_remote_os_limit(lm95245_device, colibri_t20_throttle_temp);

	if (request_irq(TEGRA_GPIO_TO_IRQ(THERMD_ALERT), thermd_alert_irq,
			IRQF_TRIGGER_LOW, "THERMD_ALERT", NULL))
		pr_err("%s: unable to register THERMD_ALERT interrupt\n", __func__);
}

#ifdef CONFIG_DEBUG_FS
static int colibri_t20_thermal_get_throttle_temp(void *data, u64 *val)
{
	*val = (u64)colibri_t20_throttle_temp;
	return 0;
}

static int colibri_t20_thermal_set_throttle_temp(void *data, u64 val)
{
	colibri_t20_throttle_temp = val;
	if (!tegra_is_throttling() && lm95245_device)
		lm95245_set_remote_os_limit(lm95245_device, colibri_t20_throttle_temp);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(throttle_fops,
			colibri_t20_thermal_get_throttle_temp,
			colibri_t20_thermal_set_throttle_temp,
			"%llu\n");

static int colibri_t20_thermal_get_shutdown_temp(void *data, u64 *val)
{
	*val = (u64)colibri_t20_shutdown_temp;
	return 0;
}

static int colibri_t20_thermal_set_shutdown_temp(void *data, u64 val)
{
	colibri_t20_shutdown_temp = val;
	if (tegra_is_throttling() && lm95245_device)
		lm95245_set_remote_os_limit(lm95245_device, colibri_t20_shutdown_temp);

	/* Carefull as we can only actively monitor one temperatur limit and
	   assumption is throttling is lower than shutdown one. */
	if (colibri_t20_shutdown_temp < colibri_t20_throttle_temp)
		colibri_t20_thermal_set_throttle_temp(NULL, colibri_t20_shutdown_temp);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(shutdown_fops,
			colibri_t20_thermal_get_shutdown_temp,
			colibri_t20_thermal_set_shutdown_temp,
			"%llu\n");

static int __init colibri_t20_thermal_debug_init(void)
{
	struct dentry *thermal_debugfs_root;

	thermal_debugfs_root = debugfs_create_dir("thermal", 0);

	if (!debugfs_create_file("throttle", 0644, thermal_debugfs_root,
					NULL, &throttle_fops))
		return -ENOMEM;

	if (!debugfs_create_file("shutdown", 0644, thermal_debugfs_root,
					NULL, &shutdown_fops))
		return -ENOMEM;

	return 0;
}
late_initcall(colibri_t20_thermal_debug_init);
#endif /* CONFIG_DEBUG_FS */

/* UART */

static struct platform_device *colibri_t20_uart_devices[] __initdata = {
	&tegra_uarta_device, /* FF */
	&tegra_uartb_device, /* STD */
	&tegra_uartd_device, /* BT */
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
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

//TODO: overcurrent

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.has_hostpc	= false,
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
	.has_hostpc	= false,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.phy_intf	= TEGRA_USB_PHY_INTF_UTMI,
	.port_otg	= true,
	.u_cfg.utmi = {
		.elastic_limit		= 16,
		.hssync_start_delay	= 9,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 8,
	},
	.u_data.host = {
		.hot_plug			= true,
		.power_off_on_suspend		= false,
		.remote_wakeup_supported	= false,
		.vbus_gpio			= -1,
		.vbus_reg			= NULL,
	},
};

static void ulpi_link_platform_open(void)
{
	int reset_gpio = USB3340_RESETB;

	gpio_request(reset_gpio, "ulpi_phy_reset");
	gpio_direction_output(reset_gpio, 0);
	msleep(5);
	gpio_direction_output(reset_gpio, 1);
}

static void ulpi_link_platform_post_phy_on(void)
{
	/* enable VBUS */
	gpio_set_value(LAN_V_BUS, 1);

	/* reset */
	gpio_set_value(LAN_RESET, 0);

	udelay(5);

	/* unreset */
	gpio_set_value(LAN_RESET, 1);
}

static void ulpi_link_platform_pre_phy_off(void)
{
	/* disable VBUS */
	gpio_set_value(LAN_V_BUS, 0);
}

static struct tegra_usb_phy_platform_ops ulpi_link_plat_ops = {
	.open = ulpi_link_platform_open,
	.post_phy_on = ulpi_link_platform_post_phy_on,
	.pre_phy_off = ulpi_link_platform_pre_phy_off,
};

static struct tegra_usb_platform_data tegra_ehci2_ulpi_link_pdata = {
	.has_hostpc	= false,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.ops		= &ulpi_link_plat_ops,
	.phy_intf	= TEGRA_USB_PHY_INTF_ULPI_LINK,
	.port_otg	= false,
	.u_cfg.ulpi = {
		.clk			= "cdev2",
		.clock_out_delay	= 1,
		.data_trimmer		= 4,
		.dir_trimmer		= 4,
		.shadow_clk_delay	= 10,
		.stpdirnxt_trimmer	= 4,
	},
	.u_data.host = {
		.hot_plug			= false,
		.power_off_on_suspend		= true,
		.remote_wakeup_supported	= false,
		.vbus_gpio			= -1,
		.vbus_reg			= NULL,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.has_hostpc	= false,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.phy_intf	= TEGRA_USB_PHY_INTF_UTMI,
	.port_otg	= false,
	.u_cfg.utmi = {
		.elastic_limit		= 16,
		.hssync_start_delay	= 9,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup		= 8,
	},
	.u_data.host = {
		.hot_plug			= true,
		.power_off_on_suspend		= false,
		.remote_wakeup_supported	= false,
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

static void colibri_t20_usb_init(void)
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

	/* EHCI instance 1: ULPI PHY -> ASIX ETH */
	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_ulpi_link_pdata;
	platform_device_register(&tegra_ehci2_device);

	/* EHCI instance 2: USB3_DP/N -> USBH1_P/N */
	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);

#ifdef MECS_TELLURIUM
//SD card multiplexing: pull GPIO_PT2 and GPIO_PBB2 low
//working even without any modifications
	{
		int gpio_status;
		unsigned int i2c_scl = I2C_SCL;
		unsigned int i2c_sda = I2C_SDA;
		unsigned int tellurium_usb_hub_reset = MECS_USB_HUB_RESET;

		printk("MECS Tellurium USB Hub Initialisation\n");

		/* configure USB hub reset line as output and pull low into reset */
		gpio_status = gpio_request(tellurium_usb_hub_reset, "USB_HUB_RESET");
		if (gpio_status < 0)
			pr_warning("USB_HUB_RESET request GPIO FAILED\n");
		gpio_status = gpio_direction_output(tellurium_usb_hub_reset, 0);
		if (gpio_status < 0)
			pr_warning("USB_HUB_RESET request GPIO DIRECTION FAILED\n");

		/* configure I2C pins as outputs and pull low */
		gpio_status = gpio_direction_output(i2c_scl, 0);
		if (gpio_status < 0)
			pr_warning("I2C_SCL request GPIO DIRECTION FAILED\n");
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

/* W1, aka OWR, aka OneWire */

#ifdef CONFIG_W1_MASTER_TEGRA
struct tegra_w1_timings colibri_t20_w1_timings = {
		.tsu		= 1,
		.trelease	= 0xf,
		.trdv		= 0xf,
		.tlow0		= 0x3c,
		.tlow1		= 1,
		.tslot		= 0x77,

		.tpdl		= 0x78,
		.tpdh		= 0x1e,
		.trstl		= 0x1df,
		.trsth		= 0x1df,
		.rdsclk		= 0x7,
		.psclk		= 0x50,
};

struct tegra_w1_platform_data colibri_t20_w1_platform_data = {
	.clk_id		= "tegra_w1",
	.timings	= &colibri_t20_w1_timings,
};
#endif /* CONFIG_W1_MASTER_TEGRA */

static struct platform_device *colibri_t20_devices[] __initdata = {
#ifdef CONFIG_RTC_DRV_TEGRA
	&tegra_rtc_device,
#endif
	&tegra_nand_device,

	&tegra_pmu_device,
	&tegra_gart_device,
	&tegra_aes_device,
#ifdef CONFIG_KEYBOARD_GPIO
	&colibri_t20_keys_device,
#endif
	&tegra_wdt_device,
	&tegra_avp_device,
#ifdef CONFIG_TEGRA_CAMERA
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
	&tegra_led_pwm_device,
	&tegra_pwfm1_device,
#ifndef MECS_TELLURIUM
	&tegra_pwfm2_device,
#else
	&tegra_pwfm0_device,
#endif
	&tegra_pwfm3_device,
#ifdef CONFIG_W1_MASTER_TEGRA
	&tegra_w1_device,
#endif
};

static void __init colibri_t20_init(void)
{
	tegra_clk_init_from_table(colibri_t20_clk_init_table);
	colibri_t20_pinmux_init();
	colibri_t20_thermd_alert_init();
	colibri_t20_i2c_init();
	colibri_t20_uart_init();
//
	tegra_ac97_device.dev.platform_data = &colibri_t20_wm97xx_pdata;
//
#ifdef CONFIG_W1_MASTER_TEGRA
	tegra_w1_device.dev.platform_data = &colibri_t20_w1_platform_data;
#endif
	platform_add_devices(colibri_t20_devices,
			     ARRAY_SIZE(colibri_t20_devices));
	tegra_ram_console_debug_init();
	colibri_t20_sdhci_init();
	colibri_t20_regulator_init();

//	tegra_das_device.dev.platform_data = &tegra_das_pdata;
//	tegra_ac97_device.dev.platform_data = &tegra_audio_pdata;
//	tegra_spdif_input_device.name = "spdif";
//	tegra_spdif_input_device.dev.platform_data = &tegra_spdif_audio_pdata;

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

void __init colibri_t20_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	/* we specify zero for special handling due to already reserved
	   fbmem/nvmem (U-Boot 2011.06 compatibility from our V1.x images) */
	tegra_reserve(0, SZ_8M + SZ_1M, SZ_16M);
	tegra_ram_console_debug_reserve(SZ_1M);
}

static const char *colibri_t20_dt_board_compat[] = {
	"toradex,colibri_t20",
	NULL
};

#ifdef CONFIG_ANDROID
MACHINE_START(COLIBRI_T20, "ventana")
#else
MACHINE_START(COLIBRI_T20, "Toradex Colibri T20")
#endif
	.boot_params	= 0x00000100,
	.dt_compat	= colibri_t20_dt_board_compat,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.init_machine	= colibri_t20_init,
	.map_io		= tegra_map_common_io,
	.reserve	= colibri_t20_reserve,
	.timer		= &tegra_timer,
MACHINE_END
