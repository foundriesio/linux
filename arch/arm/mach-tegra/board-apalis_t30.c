/*
 * arch/arm/mach-tegra/board-apalis_t30.c
 *
 * Copyright (c) 2013-2015 Toradex, Inc.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <linux/can/platform/mcp251x.h>
#include <linux/clk.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/input/fusion_F0710A.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#include <linux/lm95245.h>
#include <linux/mfd/stmpe.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>
#include <linux/tegra_uart.h>

#include <mach/io_dpd.h>
#include <mach/pci.h>
#include <mach/sdhci.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/tegra_fiq_debugger.h>
#include <mach/thermal.h>
#include <mach/usb_phy.h>
#include <mach/w1.h>

#include <media/ov5640.h>
#include <media/soc_camera.h>
#include <media/tegra_v4l2_camera.h>

#include "board-apalis_t30.h"
#include "board.h"
#include "clock.h"
#include "devices.h"
#include "gpio-names.h"
#include "pm.h"
#include "wakeups-t3.h"

/* Audio */

/* HDA see sound/pci/hda/hda_intel.c */

/* I2S */

static struct tegra_asoc_platform_data apalis_t30_audio_sgtl5000_pdata = {
	.gpio_ext_mic_en	= -1,
	.gpio_hp_det		= -1,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_spkr_en		= -1,
	.i2s_param[BASEBAND] = {
		.audio_port_id	= -1,
	},
	.i2s_param[BT_SCO] = {
		.audio_port_id	= -1,
	},
	.i2s_param[HIFI_CODEC] = {
		.audio_port_id	= 1, /* index of below registered
					tegra_i2s_device plus one if HDA codec
					is activated as well */
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.is_i2s_master	= 1, /* meaning T30 SoC is I2S master */
		.sample_size	= 16,
	},
};

static struct platform_device apalis_t30_audio_sgtl5000_device = {
	.dev = {
		.platform_data = &apalis_t30_audio_sgtl5000_pdata,
	},
	.id	= 0,
	.name	= "tegra-snd-apalis_t30-sgtl5000",
};

/* Camera */

#ifdef CONFIG_TEGRA_CAMERA
static struct platform_device tegra_camera = {
	.id	= -1,
	.name	= "tegra_camera",
};
#endif /* CONFIG_TEGRA_CAMERA */

#if defined(CONFIG_VIDEO_TEGRA) || defined(CONFIG_VIDEO_TEGRA_MODULE)
static void tegra_camera_disable(struct nvhost_device *ndev)
{
}

static int tegra_camera_enable(struct nvhost_device *ndev)
{
	return 0;
}

#if defined(CONFIG_VIDEO_ADV7180) || defined(CONFIG_VIDEO_ADV7180_MODULE)
static struct i2c_board_info camera_i2c_adv7180 = {
	I2C_BOARD_INFO("adv7180", 0x21),
};

static struct tegra_camera_platform_data adv7180_platform_data = {
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.port			= TEGRA_CAMERA_PORT_VIP,
	.internal_sync		= false,
	.vip_h_active_start	= 0x8F,
	.vip_v_active_start	= 0x12,
};

static struct soc_camera_link iclink_adv7180 = {
	.board_info	= &camera_i2c_adv7180,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &adv7180_platform_data,
};

static struct platform_device soc_camera_adv7180 = {
	.dev = {
		.platform_data = &iclink_adv7180,
	},
	.id	= 0,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_VIDEO_ADV7180 | CONFIG_VIDEO_ADV7180_MODULE */

#if defined(CONFIG_VIDEO_ADV7280) || defined(CONFIG_VIDEO_ADV7280_MODULE)
static struct i2c_board_info camera_i2c_adv7280 = {
	I2C_BOARD_INFO("adv7280", 0x21),
};

static struct tegra_camera_platform_data adv7280_platform_data = {
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.internal_sync		= false,
	.port			= TEGRA_CAMERA_PORT_VIP,
	.vip_h_active_start	= 0x44,
	.vip_v_active_start	= 0x27,
};

static struct soc_camera_link iclink_adv7280 = {
	.board_info	= &camera_i2c_adv7280,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &adv7280_platform_data,
};

static struct platform_device soc_camera_adv7280 = {
	.dev = {
		.platform_data = &iclink_adv7280,
	},
	.id	= 1,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_VIDEO_ADV7280 | CONFIG_VIDEO_ADV7280_MODULE */

#if defined(CONFIG_SOC_CAMERA_AS0260) || \
		defined(CONFIG_SOC_CAMERA_AS0260_MODULE)
static struct i2c_board_info camera_i2c_as0260soc = {
	I2C_BOARD_INFO("as0260soc", 0x48),
};

static struct tegra_camera_platform_data as0260soc_platform_data = {
	.continuous_clk		= true,
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.internal_sync		= false,
	.lanes			= 2,
	.port			= TEGRA_CAMERA_PORT_CSI_A,
//	.port			= TEGRA_CAMERA_PORT_CSI_B,
//	.port			= TEGRA_CAMERA_PORT_VIP,
	.vip_h_active_start	= 0,
//	.vip_h_active_start	= 8F,
	.vip_v_active_start	= 0,
//	.vip_v_active_start	= 12,
};

static struct soc_camera_link iclink_as0260soc = {
	.board_info	= &camera_i2c_as0260soc,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &as0260soc_platform_data,
};

static struct platform_device soc_camera_as0260soc = {
	.dev = {
		.platform_data = &iclink_as0260soc,
	},
	.id	= 2,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_SOC_CAMERA_AS0260 | CONFIG_SOC_CAMERA_AS0260_MODULE */

#if defined(CONFIG_SOC_CAMERA_MAX9526) || \
		defined(CONFIG_SOC_CAMERA_MAX9526_MODULE)
static struct i2c_board_info camera_i2c_max9526 = {
	I2C_BOARD_INFO("max9526", 0x20),
};

static struct tegra_camera_platform_data max9526_platform_data = {
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.internal_sync		= false,
	.port			= TEGRA_CAMERA_PORT_VIP,
	.vip_h_active_start	= 0x8F,
	.vip_v_active_start	= 0x12,
};

static struct soc_camera_link iclink_max9526 = {
	.board_info	= &camera_i2c_max9526,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &max9526_platform_data,
};

static struct platform_device soc_camera_max9526 = {
	.dev = {
		.platform_data = &iclink_max9526,
	},
	.id	= 3,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_SOC_CAMERA_MAX9526 | CONFIG_SOC_CAMERA_MAX9526_MODULE */

#if defined(CONFIG_SOC_CAMERA_OV5640) || \
		defined(CONFIG_SOC_CAMERA_OV5640_MODULE)

static int apalis_t30_ov5640_power(struct device *dev, int enable)
{
	return 0;
}

static struct i2c_board_info apalis_t30_ov5640_camera_i2c_device = {
	I2C_BOARD_INFO("ov5640", 0x3C),
};

static struct tegra_camera_platform_data ov5640_platform_data = {
	.continuous_capture	= 1,
	.continuous_clk		= 0,
	.flip_v			= 0,
	.flip_h			= 0,
	.lanes			= 2,
	.port			= TEGRA_CAMERA_PORT_CSI_A,
	.vi_freq		= 24000000,
};

static struct soc_camera_link ov5640_iclink = {
	.board_info	= &apalis_t30_ov5640_camera_i2c_device,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.module_name	= "ov5640",
	.power		= apalis_t30_ov5640_power,
	.priv		= &ov5640_platform_data,
};

static struct platform_device apalis_t30_ov5640_soc_camera_device = {
	.dev = {
		.platform_data = &ov5640_iclink,
	},
	.id	= 4,
	.name	= "soc-camera-pdrv",
};
#endif /* ONFIG_SOC_CAMERA_OV5640 | CONFIG_SOC_CAMERA_OV5640_MODULE */

#if defined(CONFIG_SOC_CAMERA_OV7670SOC) || \
		defined(CONFIG_SOC_CAMERA_OV7670SOC_MODULE)
static struct i2c_board_info camera_i2c_ov7670soc = {
	I2C_BOARD_INFO("ov7670soc", 0x21),
};

static struct tegra_camera_platform_data ov7670_platform_data = {
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.internal_sync		= false,
	.port			= TEGRA_CAMERA_PORT_VIP,
	.vip_h_active_start	= 0x8F,
	.vip_v_active_start	= 0x12,
};

static struct soc_camera_link iclink_ov7670soc = {
	.board_info	= &camera_i2c_ov7670soc,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &ov7670_platform_data,
};

static struct platform_device soc_camera_ov7670soc = {
	.dev = {
		.platform_data = &iclink_ov7670soc,
	},
	.id	= 5,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_SOC_CAMERA_OV7670SOC | CONFIG_SOC_CAMERA_OV7670SOC_MODULE */

#if defined(CONFIG_SOC_CAMERA_TVP5150) || \
		defined(CONFIG_SOC_CAMERA_TVP5150_MODULE)
static struct i2c_board_info camera_i2c_tvp5150soc = {
	I2C_BOARD_INFO("tvp5150soc", 0x5d),
};

static struct tegra_camera_platform_data tvp5150soc_platform_data = {
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.internal_sync		= false,
	.port			= TEGRA_CAMERA_PORT_VIP,
	.vip_h_active_start	= 0x8F,
	.vip_v_active_start	= 0x12,
};

static struct soc_camera_link iclink_tvp5150soc = {
	.board_info	= &camera_i2c_tvp5150soc,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &tvp5150soc_platform_data,
};

static struct platform_device soc_camera_tvp5150soc = {
	.dev = {
		.platform_data = &iclink_tvp5150soc,
	},
	.id	= 6,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_SOC_CAMERA_TVP5150 | CONFIG_SOC_CAMERA_TVP5150_MODULE */
#if defined(CONFIG_SOC_CAMERA_S2D13P04) || \
		defined(CONFIG_SOC_CAMERA_S2D13P04_MODULE)
static struct i2c_board_info camera_i2c_s2d13p04 = {
	I2C_BOARD_INFO("s2d13p04", 0x37),
};

static struct tegra_camera_platform_data s2d13p04_platform_data = {
	.disable_camera		= tegra_camera_disable,
	.enable_camera		= tegra_camera_enable,
	.flip_h			= 0,
	.flip_v			= 0,
	.internal_sync		= false,
	.port			= TEGRA_CAMERA_PORT_VIP,
	.vip_h_active_start	= 0x66,
	.vip_v_active_start	= 0x21,
};

static struct soc_camera_link iclink_s2d13p04 = {
	.board_info	= &camera_i2c_s2d13p04,
	.bus_id		= -1, /* This must match the .id of tegra_vi01_device */
	.i2c_adapter_id	= 2,
	.priv		= &s2d13p04_platform_data,
};

static struct platform_device soc_camera_s2d13p04 = {
	.dev = {
		.platform_data = &iclink_s2d13p04,
	},
	.id	= 7,
	.name	= "soc-camera-pdrv",
};
#endif /* CONFIG_SOC_CAMERA_S2D13P04 | CONFIG_SOC_CAMERA_S2D13P04_MODULE */
#endif /* CONFIG_VIDEO_TEGRA | CONFIG_VIDEO_TEGRA_MODULE */

/* CAN */

#if defined(CONFIG_CAN_MCP251X) || defined(CONFIG_CAN_MCP251X_MODULE)
static struct tegra_spi_device_controller_data mcp251x_controller_data = {
	.cs_hold_clk_count	= 1,	/* at least 50 ns */
	.cs_setup_clk_count	= 1,	/* at least 50 ns */
	.is_hw_based_cs		= 1,
};

static struct mcp251x_platform_data can_pdata = {
	.oscillator_frequency	= 16000000,
	.power_enable		= NULL,
	.transceiver_enable	= NULL
};

static struct spi_board_info can_board_info[] = {
	{
		.bus_num		= 1,		/* SPI2: CAN1 */
		.chip_select		= 0,
		.controller_data	= &mcp251x_controller_data,
		.max_speed_hz		= 10000000,
		.modalias		= "mcp2515",
		.platform_data		= &can_pdata,
	},
	{
		.bus_num		= 3,		/* SPI4: CAN2 */
		.chip_select		= 1,
		.controller_data	= &mcp251x_controller_data,
		.max_speed_hz		= 10000000,
		.modalias		= "mcp2515",
		.platform_data		= &can_pdata,
	},
};

static void __init apalis_t30_mcp2515_can_init(void)
{
	can_board_info[0].irq = gpio_to_irq(CAN1_INT);
	can_board_info[1].irq = gpio_to_irq(CAN2_INT);
	spi_register_board_info(can_board_info, ARRAY_SIZE(can_board_info));
}
#else /* CONFIG_CAN_MCP251X | CONFIG_CAN_MCP251X_MODULE */
#define apalis_t30_mcp2515_can_init() do {} while (0)
#endif /* CONFIG_CAN_MCP251X | CONFIG_CAN_MCP251X_MODULE */

/* CEC */

//TODO

/* Clocks */
static struct tegra_clk_init_table apalis_t30_clk_init_table[] __initdata = {
	/* name			parent		rate		enabled */
	{"apbif",		"clk_m",	12000000,	false},
	{"audio0",		"i2s0_sync",	0,		false},
	{"audio1",		"i2s1_sync",	0,		false},
	{"audio2",		"i2s2_sync",	0,		false},
	{"audio3",		"i2s3_sync",	0,		false},
	{"audio4",		"i2s4_sync",	0,		false},
	{"blink",		"clk_32k",	32768,		true},
	/* required for vi_sensor ? */
	{"csus",		"clk_m",	0,		true},
	{"d_audio",		"clk_m",	12000000,	false},
	{"dam0",		"clk_m",	12000000,	false},
	{"dam1",		"clk_m",	12000000,	false},
	{"dam2",		"clk_m",	12000000,	false},
	{"hda",			"pll_p",	108000000,	false},
	{"hda2codec_2x",	"pll_p",	48000000,	false},
	{"i2c1",		"pll_p",	3200000,	false},
	{"i2c2",		"pll_p",	3200000,	false},
	{"i2c3",		"pll_p",	3200000,	false},
	{"i2c4",		"pll_p",	3200000,	false},
	{"i2c5",		"pll_p",	3200000,	false},
	{"i2s0",		"pll_a_out0",	0,		false},
	{"i2s1",		"pll_a_out0",	0,		false},
	{"i2s2",		"pll_a_out0",	0,		false},
	{"i2s3",		"pll_a_out0",	0,		false},
	{"i2s4",		"pll_a_out0",	0,		false},
	{"pll_a",		NULL,		564480000,	true},
	{"pll_m",		NULL,		0,		false},
	{"pwm",			"pll_p",	3187500,	false},
	{"spdif_out",		"pll_a_out0",	0,		false},
	{"vi",			"pll_p",	0,		false},
	{"vi_sensor",		"pll_p",	24000000,	true},
	{NULL,			NULL,		0,		0},
};

/* GPIO */

static struct gpio apalis_t30_gpios[] = {
	{APALIS_GPIO1,		GPIOF_IN,		"GPIO1 X1-1"},
	{APALIS_GPIO2,		GPIOF_IN,		"GPIO2 X1-3"},
	{APALIS_GPIO3,		GPIOF_IN,		"GPIO3 X1-5"},
	{APALIS_GPIO4,		GPIOF_IN,		"GPIO4 X1-7"},
#ifndef POWER_GPIO
	{APALIS_GPIO5,		GPIOF_IN,		"GPIO5 X1-9"},
#endif
#ifndef FORCE_OFF_GPIO
	{APALIS_GPIO6,		GPIOF_IN,		"GPIO6 X1-11"},
#endif
	/* GPIO7 is used by PCIe driver on Evaluation board */
/*	{APALIS_GPIO7,		GPIOF_IN,		"GPIO7 X1-13"}, */
	{APALIS_GPIO8,		GPIOF_IN,		"GPIO8 X1-15, FAN"},
#ifdef IXORA
	{LED4_GREEN,		GPIOF_OUT_INIT_LOW,	"Ixora LED4_GREEN"},
	{LED4_RED,		GPIOF_OUT_INIT_LOW,	"Ixora LED4_RED"},
	{LED5_GREEN,		GPIOF_OUT_INIT_LOW,	"Ixora LED5_GREEN"},
	{LED5_RED,		GPIOF_OUT_INIT_LOW,	"Ixora LED5_RED"},
#endif /* IXORA */
	{LVDS_MODE,		GPIOF_IN,		"LVDS: Single/Dual Ch"},
	{LVDS_6B_8B_N,		GPIOF_IN,		"LVDS: 18/24 Bit Mode"},
	{LVDS_OE,		GPIOF_IN,		"LVDS: Output Enable"},
	{LVDS_PDWN_N,		GPIOF_IN,		"LVDS: Power Down"},
	{LVDS_R_F_N,		GPIOF_IN,		"LVDS: Clock Polarity"},
	{LVDS_MAP,		GPIOF_IN,		"LVDS: Colour Mapping"},
	{LVDS_RS,		GPIOF_IN,		"LVDS: Swing Mode"},
	{LVDS_DDR_N,		GPIOF_IN,		"LVDS: DDRclk Disable"},
#ifdef IXORA
	{PCIE1_WDISABLE_N,	GPIOF_OUT_INIT_HIGH,	"PCIE1_WDISABLE_N"},
	{SW3,			GPIOF_IN,		"Ixora SW3"},
	{UART2_3_RS232_FOFF_N,	GPIOF_OUT_INIT_HIGH,	"UART2_3_RS232_FOFF_N"},
#endif /* IXORA */
};

static void apalis_t30_gpio_init(void)
{
	int i = 0;
	int length = sizeof(apalis_t30_gpios) / sizeof(struct gpio);
	int err = 0;

	for (i = 0; i < length; i++) {
		err = gpio_request_one(apalis_t30_gpios[i].gpio,
				       apalis_t30_gpios[i].flags,
				       apalis_t30_gpios[i].label);

		if (err) {
			pr_warn("gpio_request(%s) failed, err = %d",
				apalis_t30_gpios[i].label, err);
		} else {
			gpio_export(apalis_t30_gpios[i].gpio, true);
		}
	}
}

/*
 * Fusion touch screen GPIOs (using Toradex display/touch adapter)
 * Apalis GPIO 5, MXM-11, Ixora X27-17, pen down interrupt
 * Apalis GPIO 6, MXM-13, Ixora X27-18, reset
 * gpio_request muxes the GPIO function automatically, we only have to make
 * sure input/output muxing is done and the GPIO is freed here.
 */
static int pinmux_fusion_pins(void);

static struct fusion_f0710a_init_data apalis_fusion_pdata = {
	.gpio_int		= APALIS_GPIO5,	/* MXM-11, Pen down interrupt */
	.gpio_reset		= APALIS_GPIO6,	/* MXM-13, Reset interrupt */
	.pinmux_fusion_pins	= &pinmux_fusion_pins,
};

static int pinmux_fusion_pins(void)
{
	gpio_free(apalis_fusion_pdata.gpio_int);
	gpio_free(apalis_fusion_pdata.gpio_reset);
	apalis_fusion_pdata.pinmux_fusion_pins = NULL;

	return 0;
}

/* I2C */

/* Make sure that the pinmuxing enable the 'open drain' feature for pins used
   for I2C */

/* GEN1_I2C: I2C1_SDA/SCL on MXM3 pin 209/211 (e.g. RTC on carrier board) */
static struct i2c_board_info apalis_t30_i2c_bus1_board_info[] __initdata = {
	{
		/* M41T0M6 real time clock on carrier board */
		I2C_BOARD_INFO("rtc-ds1307", 0x68),
			.type = "m41t00",
	},
	{
		/* TouchRevolution Fusion 7 and 10 multi-touch controller */
		I2C_BOARD_INFO("fusion_F0710A", 0x10),
			.platform_data = &apalis_fusion_pdata,
	},
};

static struct tegra_i2c_platform_data apalis_t30_i2c1_platform_data = {
	.adapter_nr	= 0,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {400000, 0},
	.bus_count	= 1,
	.scl_gpio	= {I2C1_SCL, 0},
	.sda_gpio	= {I2C1_SDA, 0},
	.slave_addr	= 0x00FC,
};

/* GEN2_I2C: unused */

/* DDC: I2C2_SDA/SCL on MXM3 pin 205/207 (e.g. display EDID) */
static struct tegra_i2c_platform_data apalis_t30_i2c4_platform_data = {
	.adapter_nr	= 3,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {10000, 10000},
	.bus_count	= 1,
	.scl_gpio	= {I2C2_SCL, 0},
	.sda_gpio	= {I2C2_SDA, 0},
	.slave_addr	= 0x00FC,
};

/* CAM_I2C: I2C3_SDA/SCL on MXM3 pin 201/203 (e.g. camera sensor on carrier
   board) */
static struct tegra_i2c_platform_data apalis_t30_i2c3_platform_data = {
	.adapter_nr	= 2,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {400000, 0},
	.bus_count	= 1,
	.scl_gpio	= {I2C3_SCL, 0},
	.sda_gpio	= {I2C3_SDA, 0},
	.slave_addr	= 0x00FC,
};

/* PWR_I2C: power I2C to audio codec, PMIC, temperature sensor and touch screen
	    controller */

/* STMPE811 touch screen controller */
static struct stmpe_ts_platform_data stmpe811_ts_data = {
	.adc_freq		= 1, /* 3.25 MHz ADC clock speed */
	.ave_ctrl		= 3, /* 8 sample average control */
	.fraction_z		= 7, /* 7 length fractional part in z */
	.i_drive		= 1, /* 50 mA typical 80 mA max touchscreen
					drivers current limit value */
	.mod_12b		= 1, /* 12-bit ADC */
	.ref_sel		= 0, /* internal ADC reference */
	.sample_time		= 4, /* ADC converstion time: 80 clocks */
	.settling		= 3, /* 1 ms panel driver settling time */
	.touch_det_delay	= 5, /* 5 ms touch detect interrupt delay */
};

/* STMPE811 ADC controller */
static struct stmpe_adc_platform_data stmpe811_adc_data = {
	.adc_freq		= 1, /* 3.25 MHz ADC clock speed */
	.mod_12b		= 1, /* 12-bit ADC */
	.ref_sel		= 0, /* internal ADC reference */
	.sample_time		= 4, /* ADC converstion time: 80 clocks */
};

static struct stmpe_platform_data stmpe811_data = {
	.adc		= &stmpe811_adc_data,
	.blocks		= STMPE_BLOCK_TOUCHSCREEN | STMPE_BLOCK_ADC,
	.id		= 1,
	.irq_base	= STMPE811_IRQ_BASE,
	.irq_trigger	= IRQF_TRIGGER_FALLING,
	.ts		= &stmpe811_ts_data,
};

static void lm95245_probe_callback(struct device *dev);

static struct lm95245_platform_data apalis_t30_lm95245_pdata = {
	.enable_os_pin	= true,
	.probe_callback	= lm95245_probe_callback,
};

static struct i2c_board_info apalis_t30_i2c_bus5_board_info[] __initdata = {
	{
		/* SGTL5000 audio codec */
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
	{
		/* STMPE811 touch screen controller */
		I2C_BOARD_INFO("stmpe", 0x41),
			.flags = I2C_CLIENT_WAKE,
			.platform_data = &stmpe811_data,
			.type = "stmpe811",
	},
	{
		/* LM95245 temperature sensor
		   Note: OVERT_N directly connected to PMIC PWRDN */
		I2C_BOARD_INFO("lm95245", 0x4c),
			.platform_data = &apalis_t30_lm95245_pdata,
	},
};

static struct tegra_i2c_platform_data apalis_t30_i2c5_platform_data = {
	.adapter_nr	= 4,
	.arb_recovery	= arb_lost_recovery,
	.bus_clk_rate	= {400000, 0},
	.bus_count	= 1,
	.scl_gpio	= {PWR_I2C_SCL, 0},
	.sda_gpio	= {PWR_I2C_SDA, 0},
};

static void __init apalis_t30_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &apalis_t30_i2c1_platform_data;
	tegra_i2c_device3.dev.platform_data = &apalis_t30_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &apalis_t30_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &apalis_t30_i2c5_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device5);

	i2c_register_board_info(0, apalis_t30_i2c_bus1_board_info,
				ARRAY_SIZE(apalis_t30_i2c_bus1_board_info));

	/* enable touch interrupt GPIO */
	gpio_request(TOUCH_PEN_INT, "TOUCH_PEN_INT");
	gpio_direction_input(TOUCH_PEN_INT);

	apalis_t30_i2c_bus5_board_info[1].irq = gpio_to_irq(TOUCH_PEN_INT);
	i2c_register_board_info(4, apalis_t30_i2c_bus5_board_info,
				ARRAY_SIZE(apalis_t30_i2c_bus5_board_info));
}

/* IrDA */

//TODO

/* Keys
 * Note: wake-up-key active-low due to EvalBoard v1.1a having 4.7 K pull-up on
 *       MXM3 pin 37 aka WAKE1_MICO
 * Note2: wake keys need to be supported by hardware, see wakeups-t3.h
 */

#ifdef CONFIG_KEYBOARD_GPIO
#define GPIO_KEY(_id, _gpio, _lowactive, _iswake)	\
	{						\
		.active_low = _lowactive,		\
		.code = _id,				\
		.debounce_interval = 10,		\
		.desc = #_id,				\
		.gpio = TEGRA_GPIO_##_gpio,		\
		.type = EV_KEY,				\
		.wakeup = _iswake,			\
	}

static struct gpio_keys_button apalis_t30_keys[] = {
#ifdef POWER_GPIO
	GPIO_KEY(KEY_POWER, PS6, 1, 0),		/* MXM3 pin 11 aka GPIO5, Ixora
						   X27-17/EvalBoard X2-6 */
#endif
	GPIO_KEY(KEY_WAKEUP, PV1, 1, 1),	/* MXM3 pin 37 aka WAKE1_MICO,
						Ixora X27-3/EvalBoard X2-24 */
};

static struct gpio_keys_platform_data apalis_t30_keys_platform_data = {
	.buttons	= apalis_t30_keys,
	.nbuttons	= ARRAY_SIZE(apalis_t30_keys),
};

static struct platform_device apalis_t30_keys_device = {
	.dev = {
		.platform_data = &apalis_t30_keys_platform_data,
	},
	.id	= 0,
	.name	= "gpio-keys",
};
#endif /* CONFIG_KEYBOARD_GPIO */

/* MMC/SD */

/* To limit the 8-bit MMC slot to 3.3 volt only operation (e.g. no UHS) */
int g_sdmmc3_uhs = 0;

static int __init enable_mmc_uhs(char *s)
{
	if (!(*s) || !strcmp(s, "1"))
		g_sdmmc3_uhs = 1;

	return 0;
}
__setup("mmc_uhs=", enable_mmc_uhs);

static struct tegra_sdhci_platform_data apalis_t30_emmc_platform_data = {
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

#ifndef IXORA
static struct tegra_sdhci_platform_data apalis_t30_mmccard_platform_data = {
	.cd_gpio	= MMC1_CD_N,
	.ddr_clk_limit	= 52000000,
	.is_8bit	= 1,
	.power_gpio	= -1,
	.tap_delay	= 0x0f,
	.wp_gpio	= -1,
	.no_1v8		= 1,
};
#endif /* !IXORA */

static struct tegra_sdhci_platform_data apalis_t30_sdcard_platform_data = {
	.cd_gpio	= SD1_CD_N,
	.ddr_clk_limit	= 52000000,
	.is_8bit	= 0,
	.power_gpio	= -1,
	.tap_delay	= 0x0f,
	.wp_gpio	= -1,
	.no_1v8		= 1,
};

static void __init apalis_t30_sdhci_init(void)
{
	/* register eMMC first */
	tegra_sdhci_device4.dev.platform_data =
			&apalis_t30_emmc_platform_data;
	platform_device_register(&tegra_sdhci_device4);

#ifndef IXORA
	if (g_sdmmc3_uhs)
		apalis_t30_mmccard_platform_data.no_1v8 = 0;
	tegra_sdhci_device3.dev.platform_data =
			&apalis_t30_mmccard_platform_data;
	platform_device_register(&tegra_sdhci_device3);
#endif /* !IXORA */

	tegra_sdhci_device1.dev.platform_data =
			&apalis_t30_sdcard_platform_data;
	platform_device_register(&tegra_sdhci_device1);
}

/* PCIe */

/* The Apalis evaluation board needs to set the link speed to 2.5 GT/s (GEN1).
   The default link speed setting is 5 GT/s (GEN2). 0x98 is the Link Control 2
   PCIe Capability Register of the PEX8605 PCIe switch. The switch supports
   link speed auto negotiation, but falsely sets the link speed to 5 GT/s. */
static void quirk_apalis_plx_gen1(struct pci_dev *dev)
{
	pci_write_config_dword(dev, 0x98, 0x01);
	mdelay(50);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PLX, 0x8605, quirk_apalis_plx_gen1);

static struct tegra_pci_platform_data apalis_t30_pci_platform_data = {
	.gpio			= 0,
	.port_status[0]		= 1,
	.port_status[1]		= 1,
	.port_status[2]		= 1,
	.use_dock_detect	= 0,
};

static void apalis_t30_pci_init(void)
{
	tegra_pci_device.dev.platform_data = &apalis_t30_pci_platform_data;
	platform_device_register(&tegra_pci_device);
}

/* PWM LEDs */
static struct led_pwm tegra_leds_pwm[] = {
	{
		.max_brightness	= 255,
		.name		= "PWM3",
		.pwm_id		= 1,
		.pwm_period_ns	= 19600,
	},
	{
		.max_brightness	= 255,
		.name		= "PWM2",
		.pwm_id		= 2,
		.pwm_period_ns	= 19600,
	},
	{
		.max_brightness	= 255,
		.name		= "PWM1",
		.pwm_id		= 3,
		.pwm_period_ns	= 19600,
	},
};

static struct led_pwm_platform_data tegra_leds_pwm_data = {
	.leds		= tegra_leds_pwm,
	.num_leds	= ARRAY_SIZE(tegra_leds_pwm),
};

static struct platform_device tegra_led_pwm_device = {
	.dev = {
		.platform_data = &tegra_leds_pwm_data,
	},
	.id	= -1,
	.name	= "leds_pwm",
};

/* RTC */

#ifdef CONFIG_RTC_DRV_TEGRA
static struct resource tegra_rtc_resources[] = {
	[0] = {
		.end	= TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_RTC_BASE,
	},
	[1] = {
		.end	= INT_RTC,
		.flags	= IORESOURCE_IRQ,
		.start	= INT_RTC,
	},
};

static struct platform_device tegra_rtc_device = {
	.id		= -1,
	.name		= "tegra_rtc",
	.num_resources	= ARRAY_SIZE(tegra_rtc_resources),
	.resource	= tegra_rtc_resources,
};
#endif /* CONFIG_RTC_DRV_TEGRA */

/* SATA */

#ifdef CONFIG_SATA_AHCI_TEGRA
static struct gpio_led apalis_gpio_leds[] = {
	[0] = {
		.active_low             = 1,
		.default_trigger        = "ide-disk",
		.gpio                   = SATA1_ACT_N,
		.name                   = "SATA1_ACT_N",
		.retain_state_suspended = 0,
	},
};

static struct gpio_led_platform_data apalis_gpio_led_data = {
	.leds		= apalis_gpio_leds,
	.num_leds	= ARRAY_SIZE(apalis_gpio_leds),
};

static struct platform_device apalis_led_gpio_device = {
	.dev = {
		.platform_data = &apalis_gpio_led_data,
	},
	.name = "leds-gpio",
};

static void apalis_t30_sata_init(void)
{
	platform_device_register(&tegra_sata_device);
	platform_device_register(&apalis_led_gpio_device);
}
#else
static void apalis_t30_sata_init(void) { }
#endif

/* SPI */

#if defined(CONFIG_SPI_TEGRA) && defined(CONFIG_SPI_SPIDEV)
static struct tegra_spi_device_controller_data spidev_controller_data = {
	.cs_hold_clk_count	= 1,
	.cs_setup_clk_count	= 1,
	.is_hw_based_cs		= 1,
};

static struct spi_board_info tegra_spi_devices[] __initdata = {
	{
		.bus_num		= 0,		/* SPI1: Apalis SPI1 */
		.chip_select		= 0,
		.controller_data	= &spidev_controller_data,
		.irq			= 0,
		.max_speed_hz		= 50000000,
		.modalias		= "spidev",
		.mode			= SPI_MODE_0,
		.platform_data		= NULL,
	},
	{
		.bus_num		= 4,		/* SPI5: Apalis SPI2 */
		.chip_select		= 2,
		.controller_data	= &spidev_controller_data,
		.irq			= 0,
		.max_speed_hz		= 50000000,
		.modalias		= "spidev",
		.mode			= SPI_MODE_0,
		.platform_data		= NULL,
	},
};

static void __init apalis_t30_register_spidev(void)
{
	spi_register_board_info(tegra_spi_devices,
				ARRAY_SIZE(tegra_spi_devices));
}
#else /* CONFIG_SPI_TEGRA && CONFIG_SPI_SPIDEV */
#define apalis_t30_register_spidev() do {} while (0)
#endif /* CONFIG_SPI_TEGRA && CONFIG_SPI_SPIDEV */

static struct platform_device *apalis_t30_spi_devices[] __initdata = {
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device4,
	&tegra_spi_device5,
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

static struct tegra_spi_platform_data apalis_t30_spi_pdata = {
	.is_clkon_always	= false,
	.is_dma_based		= true,
	.max_dma_buffer		= 16 * 1024,
	.max_rate		= 100000000,
};

static void __init apalis_t30_spi_init(void)
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
	apalis_t30_spi_pdata.parent_clk_list = spi_parent_clk;
	apalis_t30_spi_pdata.parent_clk_count = ARRAY_SIZE(spi_parent_clk);
	tegra_spi_device1.dev.platform_data = &apalis_t30_spi_pdata;
	platform_add_devices(apalis_t30_spi_devices,
			     ARRAY_SIZE(apalis_t30_spi_devices));
}

/* Thermal throttling */

static void *apalis_t30_alert_data;
static void (*apalis_t30_alert_func)(void *);
static int apalis_t30_low_edge;
static int apalis_t30_low_hysteresis;
static int apalis_t30_low_limit;
static struct device *lm95245_device;
static int thermd_alert_irq_disabled;
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
	if (!apalis_t30_low_edge && temp <= apalis_t30_low_limit) {
		apalis_t30_alert_func(apalis_t30_alert_data);
		apalis_t30_low_edge = 1;
	} else if (apalis_t30_low_edge && temp > apalis_t30_low_limit +
						 apalis_t30_low_hysteresis) {
		apalis_t30_low_edge = 0;
	}

	/* Avoid unbalanced enable for IRQ 367 */
	if (thermd_alert_irq_disabled) {
		apalis_t30_alert_func(apalis_t30_alert_data);
		thermd_alert_irq_disabled = 0;
		enable_irq(gpio_to_irq(THERMD_ALERT_N));
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
static int lm95245_set_limits(void *_data, long lo_limit_milli,
			      long hi_limit_milli)
{
	struct device *lm95245_device = _data;

	apalis_t30_low_limit = lo_limit_milli;
	if (lm95245_device)
		lm95245_set_remote_os_limit(lm95245_device, hi_limit_milli);

	return 0;
}

static int lm95245_set_alert(void *_data, void (*alert_func)(void *),
			     void *alert_data)
{
	lm95245_device = _data;
	apalis_t30_alert_func = alert_func;
	apalis_t30_alert_data = alert_data;

	return 0;
}

static int lm95245_set_shutdown_temp(void *_data, long shutdown_temp)
{
	struct device *lm95245_device = _data;

	if (lm95245_device)
		lm95245_set_remote_critical_limit(lm95245_device,
						  shutdown_temp);

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

	if (request_irq(gpio_to_irq(THERMD_ALERT_N), thermd_alert_irq,
			IRQF_TRIGGER_LOW, "THERMD_ALERT_N", NULL))
		pr_err("%s: unable to register THERMD_ALERT_N interrupt\n",
		       __func__);

	/* initialise the local temp limit */
	if (dev)
		lm95245_set_local_shared_os__critical_limit(dev, TCRIT_LOCAL);
}

static void apalis_t30_thermd_alert_init(void)
{
	apalis_t30_low_edge = 0;
	apalis_t30_low_hysteresis = 3000;
	apalis_t30_low_limit = 0;
	lm95245_device = NULL;
	thermd_alert_irq_disabled = 0;

	gpio_request(THERMD_ALERT_N, "THERMD_ALERT_N");
	gpio_direction_input(THERMD_ALERT_N);

	thermd_alert_workqueue = create_singlethread_workqueue("THERMD_ALERT_N"
							      );

	INIT_WORK(&thermd_alert_work, thermd_alert_work_func);
}

/* UART */

static struct platform_device *apalis_t30_uart_devices[] __initdata = {
	&tegra_uarta_device, /* Apalis UART1 */
	&tegra_uartd_device, /* Apalis UART2 */
	&tegra_uartb_device, /* Apalis UART3 */
	&tegra_uartc_device, /* Apalis UART4 */
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[2] = {.name = "pll_m"},
#endif
};

static struct tegra_uart_platform_data apalis_t30_uart_pdata;

static void __init uart_debug_init(void)
{
	int debug_port_id;

	debug_port_id = get_tegra_uart_debug_port_id();
	if (debug_port_id < 0)
		debug_port_id = 0;

	switch (debug_port_id) {
	case 0:
		/* UARTA is the debug port. */
		pr_info("Selecting UARTA as the debug console\n");
		apalis_t30_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		break;

	case 1:
		/* UARTB is the debug port. */
		pr_info("Selecting UARTB as the debug console\n");
		apalis_t30_uart_devices[2] = &debug_uartb_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartb");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		break;

	case 2:
		/* UARTC is the debug port. */
		pr_info("Selecting UARTC as the debug console\n");
		apalis_t30_uart_devices[3] = &debug_uartc_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartc");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		break;

	case 3:
		/* UARTD is the debug port. */
		pr_info("Selecting UARTD as the debug console\n");
		apalis_t30_uart_devices[1] = &debug_uartd_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartd");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
		break;

	default:
		pr_info("The debug console id %d is invalid, Assuming UARTA",
			debug_port_id);
		apalis_t30_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		break;
	}
}

static void __init apalis_t30_uart_init(void)
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
	apalis_t30_uart_pdata.parent_clk_list = uart_parent_clk;
	apalis_t30_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uarta_device.dev.platform_data = &apalis_t30_uart_pdata;
	tegra_uartb_device.dev.platform_data = &apalis_t30_uart_pdata;
	tegra_uartc_device.dev.platform_data = &apalis_t30_uart_pdata;
	tegra_uartd_device.dev.platform_data = &apalis_t30_uart_pdata;

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

	platform_add_devices(apalis_t30_uart_devices,
			     ARRAY_SIZE(apalis_t30_uart_devices));
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
		.power_off_on_suspend		= true,
		.remote_wakeup_supported	= true,
		.vbus_gpio			= USBO1_EN,
		.vbus_gpio_inverted		= 0,
		.vbus_reg			= NULL,
	},
};

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
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
		.xcvr_setup		= 15,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
	.u_data.host = {
		.hot_plug			= true,
		.power_off_on_suspend		= true,
		.remote_wakeup_supported	= true,
		.vbus_gpio			= USBH_EN,
		.vbus_gpio_inverted		= 0,
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
		.power_off_on_suspend		= true,
		.remote_wakeup_supported	= true,
		/* Uses same USBH_EN as EHCI2 */
		.vbus_gpio			= -1,
		.vbus_gpio_inverted		= 0,
		.vbus_reg			= NULL,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

static void apalis_t30_usb_init(void)
{
	/* OTG should be the first to be registered
	   EHCI instance 0: USB1_DP/N -> USBO1_DP/N */
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	/* setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	platform_device_register(&tegra_udc_device);

	/* EHCI instance 1: USB2_DP/N -> USBH2_DP/N */
	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_utmi_pdata;
	platform_device_register(&tegra_ehci2_device);

	/* EHCI instance 2: USB3_DP/N -> USBH3_DP/N */
	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
}

/* W1, aka OWR, aka OneWire */

#ifdef CONFIG_W1_MASTER_TEGRA
struct tegra_w1_timings apalis_t30_w1_timings = {
		.psclk		= 0x50,
		.rdsclk		= 0x7,
		.tlow0		= 0x3c,
		.tlow1		= 1,
		.tpdh		= 0x1e,
		.tpdl		= 0x78,
		.trdv		= 0xf,
		.trelease	= 0xf,
		.trsth		= 0x1df,
		.trstl		= 0x1df,
		.tslot		= 0x77,
		.tsu		= 1,
};

struct tegra_w1_platform_data apalis_t30_w1_platform_data = {
	.clk_id		= "tegra_w1",
	.timings	= &apalis_t30_w1_timings,
};
#endif /* CONFIG_W1_MASTER_TEGRA */

static struct platform_device *apalis_t30_devices[] __initdata = {
	&tegra_pmu_device,
#if defined(CONFIG_RTC_DRV_TEGRA)
	&tegra_rtc_device,
#endif
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
#ifdef CONFIG_KEYBOARD_GPIO
	&apalis_t30_keys_device,
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
	&apalis_t30_audio_sgtl5000_device,
	&tegra_hda_device,
	&tegra_cec_device,
	&tegra_led_pwm_device,
	&tegra_pwfm1_device,
	&tegra_pwfm2_device,
	&tegra_pwfm3_device,
#ifdef CONFIG_W1_MASTER_TEGRA
	&tegra_w1_device,
#endif
};

static void __init apalis_t30_init(void)
{
	tegra_thermal_init(&thermal_data, throttle_list,
			   ARRAY_SIZE(throttle_list));
	tegra_clk_init_from_table(apalis_t30_clk_init_table);
	apalis_t30_pinmux_init();
	apalis_t30_thermd_alert_init();
	apalis_t30_i2c_init();
	apalis_t30_spi_init();
	apalis_t30_usb_init();
#ifdef CONFIG_TEGRA_EDP_LIMITS
	apalis_t30_edp_init();
#endif
	apalis_t30_uart_init();
#ifdef CONFIG_W1_MASTER_TEGRA
	tegra_w1_device.dev.platform_data = &apalis_t30_w1_platform_data;
#endif
	platform_add_devices(apalis_t30_devices,
			     ARRAY_SIZE(apalis_t30_devices));
	tegra_ram_console_debug_init();
	tegra_io_dpd_init();
	apalis_t30_sdhci_init();
	apalis_t30_regulator_init();
	apalis_t30_suspend_init();
	apalis_t30_panel_init();
	apalis_t30_sata_init();
	apalis_t30_emc_init();
	apalis_t30_register_spidev();

#if defined(CONFIG_VIDEO_TEGRA) || defined(CONFIG_VIDEO_TEGRA_MODULE)
#if defined(CONFIG_VIDEO_ADV7180) || defined(CONFIG_VIDEO_ADV7180_MODULE)
	platform_device_register(&soc_camera_adv7180);
#endif
#if defined(CONFIG_VIDEO_ADV7280) || defined(CONFIG_VIDEO_ADV7280_MODULE)
	platform_device_register(&soc_camera_adv7280);
#endif
#if defined(CONFIG_SOC_CAMERA_AS0260) || \
		defined(CONFIG_SOC_CAMERA_AS0260_MODULE)
	platform_device_register(&soc_camera_as0260soc);
#endif
#if defined(CONFIG_SOC_CAMERA_MAX9526) || \
		defined(CONFIG_SOC_CAMERA_MAX9526_MODULE)
	platform_device_register(&soc_camera_max9526);
#endif
#if defined(CONFIG_SOC_CAMERA_OV5640) || \
		defined(CONFIG_SOC_CAMERA_OV5640_MODULE)
	platform_device_register(&apalis_t30_ov5640_soc_camera_device);
#endif
#if defined(CONFIG_SOC_CAMERA_OV7670SOC) || \
		defined(CONFIG_SOC_CAMERA_OV7670SOC_MODULE)
	platform_device_register(&soc_camera_ov7670soc);
#endif
#if defined(CONFIG_SOC_CAMERA_TVP5150) || \
		defined(CONFIG_SOC_CAMERA_TVP5150_MODULE)
	platform_device_register(&soc_camera_tvp5150soc);
#endif
#if defined(CONFIG_SOC_CAMERA_S2D13P04) || \
		defined(CONFIG_SOC_CAMERA_S2D13P04_MODULE)
	platform_device_register(&soc_camera_s2d13p04);
#endif
#endif /* CONFIG_VIDEO_TEGRA | CONFIG_VIDEO_TEGRA_MODULE */

	tegra_release_bootloader_fb();
	apalis_t30_pci_init();
#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif
	tegra_serial_debug_init(TEGRA_UARTA_BASE, INT_WDT_CPU, NULL, -1, -1);
	apalis_t30_mcp2515_can_init();
	apalis_t30_gpio_init();
}

static void __init apalis_t30_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	/* Support 1920X1080 32bpp,double buffered on HDMI*/
	tegra_reserve(0, SZ_8M + SZ_1M, SZ_16M);
#else
	tegra_reserve(SZ_128M, SZ_8M, SZ_8M);
#endif
	tegra_ram_console_debug_reserve(SZ_1M);
}

static const char *apalis_t30_dt_board_compat[] = {
	"toradex,apalis_t30",
	NULL
};

MACHINE_START(APALIS_T30, "Toradex Apalis T30")
	.boot_params	= 0x80000100,
	.dt_compat	= apalis_t30_dt_board_compat,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.init_machine	= apalis_t30_init,
	.map_io		= tegra_map_common_io,
	.reserve	= apalis_t30_reserve,
	.timer		= &tegra_timer,
MACHINE_END
