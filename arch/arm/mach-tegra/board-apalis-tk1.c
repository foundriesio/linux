/*
 * arch/arm/mach-tegra/board-apalis-tk1.c
 *
 * Copyright (c) 2016, Toradex AG.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c/i2c-hid.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/spi/rm31080a_ts.h>
#include <linux/maxim_sti.h>
#include <linux/memblock.h>
#include <linux/spi/spi-tegra.h>
#include <linux/nfc/pn544.h>
#include <linux/rfkill-gpio.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/regulator/consumer.h>
#include <linux/smb349-charger.h>
#include <linux/max17048_battery.h>
#include <linux/leds.h>
#include <linux/i2c/at24.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/platform_data/serial-tegra.h>
#include <linux/edp.h>
#include <linux/usb/tegra_usb_phy.h>
#include <linux/mfd/palmas.h>
#include <linux/clk/tegra.h>
#include <media/tegra_dtv.h>
#include <linux/clocksource.h>
#include <linux/irqchip.h>
#include <linux/irqchip/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/tegra_fiq_debugger.h>
#include <linux/platform_data/tegra_usb_modem_power.h>
#include <linux/platform_data/tegra_ahci.h>
#include <linux/irqchip/tegra.h>
#include <sound/max98090.h>
#include <linux/pci.h>

#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t12.h>
#include <mach/io_dpd.h>
#include <mach/i2s.h>
#include <mach/isomgr.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/dc.h>
#include <mach/tegra_usb_pad_ctrl.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/gpio-tegra.h>
#include <mach/xusb.h>

#include "board.h"
#include "board-apalis-tk1.h"
#include "board-common.h"
#include "board-touch-raydium.h"
#include "board-touch-maxim_sti.h"
#include "clock.h"
#include "common.h"
#include "devices.h"
#include "gpio-names.h"
#include "iomap.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "tegra-of-dev-auxdata.h"

static struct i2c_board_info apalis_tk1_sgtl5000_board_info = {
	/* SGTL5000 audio codec */
	I2C_BOARD_INFO("sgtl5000", 0x0a),
};

static __initdata struct tegra_clk_init_table apalis_tk1_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x", "pll_p",	48000000,	false},
	{ "pwm",	"pll_p",	48000000,	false},
	{ "pll_a",	"pll_p_out1",	282240000,	false},
	{ "pll_a_out0",	"pll_a",	12288000,	false},
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "d_audio",	"pll_a_out0",	12288000,	false},
	{ "dam0",	"clk_m",	12000000,	false},
	{ "dam1",	"clk_m",	12000000,	false},
	{ "dam2",	"clk_m",	12000000,	false},
	{ "audio2",	"i2s2_sync",	0,		false},
	{ "vi_sensor",	"pll_p",	150000000,	false},
	{ "vi_sensor2",	"pll_p",	150000000,	false},
	{ "cilab",	"pll_p",	150000000,	false},
	{ "cilcd",	"pll_p",	150000000,	false},
	{ "cile",	"pll_p",	150000000,	false},
	{ "i2c1",	"pll_p",	3200000,	false},
	{ "i2c2",	"pll_p",	3200000,	false},
	{ "i2c3",	"pll_p",	3200000,	false},
	{ "i2c4",	"pll_p",	3200000,	false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ "sbc1",	"pll_p",	25000000,	false},
	{ "sbc2",	"pll_p",	25000000,	false},
	{ "sbc3",	"pll_p",	25000000,	false},
	{ "sbc4",	"pll_p",	25000000,	false},
	{ "sbc5",	"pll_p",	25000000,	false},
	{ "sbc6",	"pll_p",	25000000,	false},
	{ "uarta",	"pll_p",	408000000,	false},
	{ "uartb",	"pll_p",	408000000,	false},
	{ "uartc",	"pll_p",	408000000,	false},
	{ "uartd",	"pll_p",	408000000,	false},
	{ NULL,		NULL,		0,		0},
};

static void apalis_tk1_i2c_init(void)
{
	i2c_register_board_info(4, &apalis_tk1_sgtl5000_board_info, 1);
}

static struct tegra_serial_platform_data apalis_tk1_uarta_pdata = {
	.dma_req_selector = 8,
	.modem_interrupt = false,
};

static struct tegra_asoc_platform_data apalis_tk1_audio_pdata_sgtl5000 = {
	.gpio_hp_det		= -1,
	.gpio_ldo1_en		= -1,
	.gpio_spkr_en		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
	.gpio_hp_mute		= -1,
	.gpio_codec1		= -1,
	.gpio_codec2		= -1,
	.gpio_codec3		= -1,
	.i2s_param[HIFI_CODEC] = {
		.audio_port_id	= 1, /* index of below registered
					tegra_i2s_device plus one if HDA codec
					is activated as well */
		.is_i2s_master	= 1, /* meaning TK1 SoC is I2S master */
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.channels	= 2,
		.bit_clk	= 1536000,
	},
	.i2s_param[BT_SCO] = {
		.audio_port_id = -1,
	},
	.i2s_param[BASEBAND] = {
		.audio_port_id = -1,
	},
};

static struct platform_device apalis_tk1_audio_device_sgtl5000 = {
	.name = "tegra-snd-apalis-tk1-sgtl5000",
	.id = 0,
	.dev = {
		.platform_data = &apalis_tk1_audio_pdata_sgtl5000,
	},
};

static void __init apalis_tk1_uart_init(void)
{
	tegra_uarta_device.dev.platform_data = &apalis_tk1_uarta_pdata;
	if (!is_tegra_debug_uartport_hs()) {
		int debug_port_id = uart_console_debug_init(0);
		if (debug_port_id < 0)
			return;

#ifdef CONFIG_TEGRA_FIQ_DEBUGGER
#if !defined(CONFIG_TRUSTED_FOUNDATIONS) && defined(CONFIG_ARCH_TEGRA_12x_SOC) \
		&& defined(CONFIG_FIQ_DEBUGGER)
	tegra_serial_debug_init(TEGRA_UARTA_BASE, INT_WDT_AVP, NULL, -1, -1);
	platform_device_register(uart_console_debug_device);
#else
	tegra_serial_debug_init(TEGRA_UARTA_BASE, INT_WDT_CPU, NULL, -1, -1);
#endif
#else
		platform_device_register(uart_console_debug_device);
#endif
	} else {
		tegra_uarta_device.dev.platform_data = &apalis_tk1_uarta_pdata;
		platform_device_register(&tegra_uarta_device);
	}
}

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

static struct platform_device *apalis_tk1_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
#if defined(CONFIG_TEGRA_WAKEUP_MONITOR)
	&tegratab_tegra_wakeup_monitor_device,
#endif
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_WATCHDOG)
	&tegra_wdt0_device,
#endif
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE) && !defined(CONFIG_USE_OF)
	&tegra12_se_device,
#endif
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&spdif_dit_device,
	&tegra_hda_device,
	&tegra_offload_device,
	&tegra30_avp_audio_device,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
	&tegra_hier_ictlr_device,
};

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq			= 0,
		.vbus_gpio			= -1,
		.charging_supported		= false,
		.remote_wakeup_supported	= false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay	= 0,
		.elastic_limit		= 16,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_setup		= 8,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio			= -1,
		.hot_plug			= false,
		.remote_wakeup_supported	= true,
		.power_off_on_suspend		= true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay	= 0,
		.elastic_limit		= 16,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_setup		= 15,
		.xcvr_lsfslew		= 0,
		.xcvr_lsrslew		= 3,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
		.vbus_oc_map		= 0x4,
		.xcvr_hsslew_lsb	= 2,
	},
};

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio			= -1,
		.hot_plug			= false,
		.remote_wakeup_supported	= true,
		.power_off_on_suspend		= true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay	= 0,
		.elastic_limit		= 16,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_setup		= 8,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
		.vbus_oc_map		= 0x5,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio			= -1,
		.hot_plug			= false,
		.remote_wakeup_supported	= true,
		.power_off_on_suspend		= true,
	},
	.u_cfg.utmi = {
	.hssync_start_delay		= 0,
		.elastic_limit		= 16,
		.idle_wait_delay	= 17,
		.term_range_adj		= 6,
		.xcvr_setup		= 8,
		.xcvr_lsfslew		= 2,
		.xcvr_lsrslew		= 2,
		.xcvr_setup_offset	= 0,
		.xcvr_use_fuses		= 1,
		.vbus_oc_map		= 0x5,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

static void apalis_tk1_usb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();
/* TBD
	tegra_ehci1_utmi_pdata.u_data.host.turn_off_vbus_on_lp0 = true; */

	tegra_udc_pdata.id_det_type = TEGRA_USB_ID;
	tegra_ehci1_utmi_pdata.id_det_type = TEGRA_USB_ID;

	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB)) {
		tegra_otg_pdata.is_xhci = false;
		tegra_udc_pdata.u_data.dev.is_xhci = false;
	} else {
		tegra_otg_pdata.is_xhci = true;
		tegra_udc_pdata.u_data.dev.is_xhci = true;
	}
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);
	/* Setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;

	if (!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB)) {
		tegra_ehci2_device.dev.platform_data = &tegra_ehci2_utmi_pdata;
		platform_device_register(&tegra_ehci2_device);
	}

	if (!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB)) {
		tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
		platform_device_register(&tegra_ehci3_device);
	}
}

static struct tegra_xusb_platform_data xusb_pdata = {
	.portmap = TEGRA_XUSB_SS_P0 | TEGRA_XUSB_USB2_P0 | TEGRA_XUSB_SS_P1 |
		   TEGRA_XUSB_USB2_P1 | TEGRA_XUSB_USB2_P2,
};

#ifdef CONFIG_TEGRA_XUSB_PLATFORM
static void apalis_tk1_xusb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	xusb_pdata.lane_owner = (u8) tegra_get_lane_owner_info();

	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB))
		xusb_pdata.portmap &= ~(TEGRA_XUSB_USB2_P0);
	if (!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB))
		xusb_pdata.portmap &= ~(TEGRA_XUSB_USB2_P2 |
					TEGRA_XUSB_USB2_P1 | TEGRA_XUSB_SS_P0);
	xusb_pdata.portmap &= ~(TEGRA_XUSB_SS_P1);

	if (usb_port_owner_info & HSIC1_PORT_OWNER_XUSB)
		xusb_pdata.portmap |= TEGRA_XUSB_HSIC_P0;

	if (usb_port_owner_info & HSIC2_PORT_OWNER_XUSB)
		xusb_pdata.portmap |= TEGRA_XUSB_HSIC_P1;
}
#endif

#ifdef CONFIG_USE_OF
static struct of_dev_auxdata apalis_tk1_auxdata_lookup[] __initdata = {
	T124_SPI_OF_DEV_AUXDATA,
	OF_DEV_AUXDATA("nvidia,tegra124-apbdma", 0x60020000, "tegra-apbdma",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-se", 0x70012000, "tegra12-se", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-host1x", TEGRA_HOST1X_BASE, "host1x",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-gk20a", TEGRA_GK20A_BAR0_BASE,
		       "gk20a.0", NULL),
#ifdef CONFIG_ARCH_TEGRA_VIC
	OF_DEV_AUXDATA("nvidia,tegra124-vic", TEGRA_VIC_BASE, "vic03.0", NULL),
#endif
	OF_DEV_AUXDATA("nvidia,tegra124-msenc", TEGRA_MSENC_BASE, "msenc",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-vi", TEGRA_VI_BASE, "vi.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-isp", TEGRA_ISP_BASE, "isp.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-isp", TEGRA_ISPB_BASE, "isp.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-tsec", TEGRA_TSEC_BASE, "tsec", NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006000, "serial-tegra.0",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006040, "serial-tegra.1",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-hsuart", 0x70006200, "serial-tegra.2",
		       NULL),
	T124_I2C_OF_DEV_AUXDATA,
	OF_DEV_AUXDATA("nvidia,tegra124-xhci", 0x70090000, "tegra-xhci",
		       &xusb_pdata),
	OF_DEV_AUXDATA("nvidia,tegra124-dc", TEGRA_DISPLAY_BASE, "tegradc.0",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-dc", TEGRA_DISPLAY2_BASE, "tegradc.1",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-nvavp", 0x60001000, "nvavp",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-pwm", 0x7000a000, "tegra-pwm", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-dfll", 0x70110000, "tegra_cl_dvfs",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra132-dfll", 0x70040084, "tegra_cl_dvfs",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-efuse", TEGRA_FUSE_BASE, "tegra-fuse",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-camera", 0, "pcl-generic",
		       NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-ahci-sata", 0x70027000, "tegra-sata.0",
		       NULL),
	{}
};
#endif

static void __init edp_init(void)
{
	apalis_tk1_edp_init();
}

static void __init tegra_apalis_tk1_early_init(void)
{
	tegra_clk_init_from_table(apalis_tk1_clk_init_table);
	tegra_clk_verify_parents();
	tegra_soc_device_init("apalis-tk1");
}

static struct tegra_dtv_platform_data apalis_tk1_dtv_pdata = {
	.dma_req_selector = 11,
};

static void __init apalis_tk1_dtv_init(void)
{
	tegra_dtv_device.dev.platform_data = &apalis_tk1_dtv_pdata;
	platform_device_register(&tegra_dtv_device);
}

static struct tegra_io_dpd pexbias_io = {
	.name			= "PEX_BIAS",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 4,
};

static struct tegra_io_dpd pexclk1_io = {
	.name			= "PEX_CLK1",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 5,
};

static struct tegra_io_dpd pexclk2_io = {
	.name			= "PEX_CLK2",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 6,
};

static struct tegra_suspend_platform_data apalis_tk1_suspend_data = {
	.cpu_timer			= 500,
	.cpu_off_timer			= 300,
	.suspend_mode			= TEGRA_SUSPEND_LP0,
	.core_timer			= 0x157e,
	.core_off_timer			= 10,
	.corereq_high			= true,
	.sysclkreq_high			= true,
	.cpu_lp2_min_residency		= 1000,
	.min_residency_vmin_fmin	= 1000,
	.min_residency_ncpu_fast	= 8000,
	.min_residency_ncpu_slow	= 5000,
	.min_residency_mclk_stop	= 5000,
	.min_residency_crail		= 20000,
};

static void __init tegra_apalis_tk1_late_init(void)
{
	apalis_tk1_display_init();
	apalis_tk1_uart_init();
	apalis_tk1_usb_init();
#ifdef CONFIG_TEGRA_XUSB_PLATFORM
	apalis_tk1_xusb_init();
#endif
	apalis_tk1_i2c_init();
	platform_add_devices(apalis_tk1_devices,
			     ARRAY_SIZE(apalis_tk1_devices));
	platform_device_register(&apalis_tk1_audio_device_sgtl5000);
	tegra_io_dpd_init();
	apalis_tk1_sdhci_init();

	apalis_tk1_regulator_init();

	apalis_tk1_dtv_init();
	tegra_init_suspend(&apalis_tk1_suspend_data);

	apalis_tk1_emc_init();

	edp_init();
	isomgr_init();
	apalis_tk1_panel_init();

	/* put PEX pads into DPD mode to save additional power */
	tegra_io_dpd_enable(&pexbias_io);
	tegra_io_dpd_enable(&pexclk1_io);
	tegra_io_dpd_enable(&pexclk2_io);

#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif

	apalis_tk1_sensors_init();
	apalis_tk1_soctherm_init();
}

static void __init tegra_apalis_tk1_init_early(void)
{
	apalis_tk1_rail_alignment_init();
	tegra12x_init_early();
}

static void __init tegra_apalis_tk1_dt_init(void)
{
	tegra_apalis_tk1_early_init();
#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	carveout_linear_set(&tegra_generic_cma_dev);
	carveout_linear_set(&tegra_vpr_cma_dev);
#endif
#ifdef CONFIG_USE_OF
	apalis_tk1_camera_auxdata(apalis_tk1_auxdata_lookup);
	of_platform_populate(NULL,
			     of_default_bus_match_table,
			     apalis_tk1_auxdata_lookup, &platform_bus);
#endif

#define PEX_PERST_N     TEGRA_GPIO_PDD1 /* Apalis GPIO7 */
#define RESET_MOCI_N    TEGRA_GPIO_PU4

	/* Reset PLX PEX 8605 PCIe Switch plus PCIe devices on Apalis Evaluation
	   Board */
	gpio_request(PEX_PERST_N, "PEX_PERST_N");
	gpio_request(RESET_MOCI_N, "RESET_MOCI_N");
	gpio_direction_output(PEX_PERST_N, 0);
	gpio_direction_output(RESET_MOCI_N, 0);
	/* Must be asserted for 100 ms after power and clocks are stable */
	mdelay(100);
	gpio_set_value(PEX_PERST_N, 1);
	/* Err_5: PEX_REFCLK_OUTpx/nx Clock Outputs is not Guaranteed Until
	   900 us After PEX_PERST# De-assertion */
	mdelay(1);
	gpio_set_value(RESET_MOCI_N, 1);

#if 0
#define LAN_RESET_N TEGRA_GPIO_PS2

	/* Reset I210 Gigabit Ethernet Controller */
	gpio_request(LAN_RESET_N, "LAN_RESET_N");
	gpio_direction_output(LAN_RESET_N, 0);
	mdelay(100);
	gpio_set_value(LAN_RESET_N, 1);
#endif

	tegra_apalis_tk1_late_init();
}

/*
 * The Apalis evaluation board needs to set the link speed to 2.5 GT/s (GEN1).
 * The default link speed setting is 5 GT/s (GEN2). 0x98 is the Link Control 2
 * PCIe Capability Register of the PEX8605 PCIe switch.
 * With the default speed setting of 5 GT/s (GEN2) the switch does not bring up
 * any of its down stream links. Limiting it to GEN1 makes them down stream
 * links to show up and work however as GEN1 only.
 */
static void quirk_apalis_plx_gen1(struct pci_dev *dev)
{
	pci_write_config_dword(dev, 0x98, 0x01);
	mdelay(50);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PLX, 0x8605, quirk_apalis_plx_gen1);

static void __init tegra_apalis_tk1_reserve(void)
{
#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	ulong tmp;
#endif /* CONFIG_TEGRA_HDMI_PRIMARY */

#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM) || \
		defined(CONFIG_TEGRA_NO_CARVEOUT)
	ulong carveout_size = 0;
	ulong fb2_size = SZ_16M;
#else
	ulong carveout_size = SZ_1G;
	ulong fb2_size = SZ_4M;
#endif
	ulong fb1_size = SZ_16M + SZ_2M;
	ulong vpr_size = 186 * SZ_1M;

#ifdef CONFIG_FRAMEBUFFER_CONSOLE
	/* support FBcon on 4K monitors */
	fb2_size = SZ_64M + SZ_8M;	/* 4096*2160*4*2 = 70778880 bytes */
#endif /* CONFIG_FRAMEBUFFER_CONSOLE */

#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	tmp = fb1_size;
	fb1_size = fb2_size;
	fb2_size = tmp;
#endif /* CONFIG_TEGRA_HDMI_PRIMARY */

	tegra_reserve4(carveout_size, fb1_size, fb2_size, vpr_size);
}

static const char *const apalis_tk1_dt_board_compat[] = {
	"toradex,apalis-tk1",
	NULL
};

DT_MACHINE_START(APALIS_TK1, "apalis-tk1")
	.atag_offset	= 0x100,
	.smp		= smp_ops(tegra_smp_ops),
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_apalis_tk1_reserve,
	.init_early	= tegra_apalis_tk1_init_early,
	.init_irq	= irqchip_init,
	.init_time	= clocksource_of_init,
	.init_machine	= tegra_apalis_tk1_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= apalis_tk1_dt_board_compat,
	.init_late      = tegra_init_late
MACHINE_END
