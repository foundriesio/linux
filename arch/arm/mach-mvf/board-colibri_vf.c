/*
 * Copyright 2013 Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/pwm_backlight.h>
#include <linux/leds_pwm.h>
#include <linux/fec.h>
#include <linux/memblock.h>
#include <linux/gpio.h>
#include <linux/etherdevice.h>
#include <linux/regulator/anatop-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <sound/pcm.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/memory.h>
#include <mach/iomux-mvf.h>
#include <mach/spi-mvf.h>
#include <mach/mxc_asrc.h>
#include <mach/mxc.h>
#include <mach/colibri-ts.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "devices-mvf.h"
#include "usb.h"
#include "crm_regs.h"

#define MVF600_SD1_CD  42

#define colibri_vf50_bl_enb	45	/* BL_ON */

static iomux_v3_cfg_t mvf600_pads[] = {
	/* SDHC1: MMC/SD */
	MVF600_PAD14_PTA24__SDHC1_CLK,
	MVF600_PAD15_PTA25__SDHC1_CMD,
	MVF600_PAD16_PTA26__SDHC1_DAT0,
	MVF600_PAD17_PTA27__SDHC1_DAT1,
	MVF600_PAD18_PTA28__SDHC1_DAT2,
	MVF600_PAD19_PTA29__SDHC1_DAT3,
	/* set PTB20 as GPIO for sdhc card detecting */
	MVF600_PAD42_PTB20__SDHC1_SW_CD,

	/* I2C0: I2C_SDA/SCL on SODIMM pin 194/196 (e.g. RTC on carrier board)
	 */
	MVF600_PAD36_PTB14__I2C0_SCL,
	MVF600_PAD37_PTB15__I2C0_SDA,

#if 0 /* optional secondary pinmux */
	/* CAN0 */
	MVF600_PAD36_PTB14__CAN0_RX, /* conflicts with
					MVF600_PAD36_PTB14__I2C0_SCL */
	MVF600_PAD37_PTB15__CAN0_TX, /* conflicts with
					MVF600_PAD37_PTB15__I2C0_SDA */

	/*CAN1*/
	MVF600_PAD38_PTB16__CAN1_RX, /* conflicts with
					MVF600_PAD38_PTB16_GPIO */
	MVF600_PAD39_PTB17__CAN1_TX, /* conflicts with
					MVF600_PAD39_PTB17_GPIO */
#endif

	/* DSPI1: SSP on SODIMM pin 86, 88, 90 and 92 */
	MVF600_PAD84_PTD5__DSPI1_PCS0,
	MVF600_PAD85_PTD6__DSPI1_SIN,
	MVF600_PAD86_PTD7__DSPI1_SOUT,
	MVF600_PAD87_PTD8__DSPI1_SCK,

	/* FEC1: Ethernet */
	MVF600_PAD0_PTA6__RMII_CLKOUT,
	MVF600_PAD54_PTC9__RMII1_MDC,
	MVF600_PAD55_PTC10__RMII1_MDIO,
	MVF600_PAD56_PTC11__RMII1_CRS_DV,
	MVF600_PAD57_PTC12__RMII1_RXD1,
	MVF600_PAD58_PTC13__RMII1_RXD0,
	MVF600_PAD59_PTC14__RMII1_RXER,
	MVF600_PAD60_PTC15__RMII1_TXD1,
	MVF600_PAD61_PTC16__RMII1_TXD0,
	MVF600_PAD62_PTC17__RMII1_TXEN,

	/* ADC */
//ADC0_SE8
//ADC0_SE9
//ADC1_SE8
//ADC1_SE9

	/* DCU0: Display */
	MVF600_PAD105_PTE0_DCU0_HSYNC,
	MVF600_PAD106_PTE1_DCU0_VSYNC,
	MVF600_PAD107_PTE2_DCU0_PCLK,
	MVF600_PAD109_PTE4_DCU0_DE, /* L_BIAS */
	MVF600_PAD110_PTE5_DCU0_R0,
	MVF600_PAD111_PTE6_DCU0_R1,
	MVF600_PAD112_PTE7_DCU0_R2,
	MVF600_PAD113_PTE8_DCU0_R3,
	MVF600_PAD114_PTE9_DCU0_R4,
	MVF600_PAD115_PTE10_DCU0_R5,
	MVF600_PAD116_PTE11_DCU0_R6,
	MVF600_PAD117_PTE12_DCU0_R7,
	MVF600_PAD118_PTE13_DCU0_G0,
	MVF600_PAD119_PTE14_DCU0_G1,
	MVF600_PAD120_PTE15_DCU0_G2,
	MVF600_PAD121_PTE16_DCU0_G3,
	MVF600_PAD122_PTE17_DCU0_G4,
	MVF600_PAD123_PTE18_DCU0_G5,
	MVF600_PAD124_PTE19_DCU0_G6,
	MVF600_PAD125_PTE20_DCU0_G7,
	MVF600_PAD126_PTE21_DCU0_B0,
	MVF600_PAD127_PTE22_DCU0_B1,
	MVF600_PAD128_PTE23_DCU0_B2,
	MVF600_PAD129_PTE24_DCU0_B3,
	MVF600_PAD130_PTE25_DCU0_B4,
	MVF600_PAD131_PTE26_DCU0_B5,
	MVF600_PAD132_PTE27_DCU0_B6,
	MVF600_PAD133_PTE28_DCU0_B7,
	MVF600_PAD45_PTC0_BL_ON,

	/* UART1: UART_C */
	MVF600_PAD26_PTB4_UART1_TX,
	MVF600_PAD27_PTB5_UART1_RX,

	/* UART0: UART_A */
//MVF600_PAD10_PTA20_UART0_DTR,
//MVF600_PAD11_PTA21_UART0_DCD,
//MVF600_PAD20_PTA30_UART0_RI,
//MVF600_PAD21_PTA31_UART0_DSR,
	MVF600_PAD32_PTB10_UART0_TX,
	MVF600_PAD33_PTB11_UART0_RX,
	MVF600_PAD34_PTB12_UART0_RTS,
	MVF600_PAD35_PTB13_UART0_CTS,

	/* UART2: UART_B */
	MVF600_PAD79_PTD0_UART2_TX,
	MVF600_PAD80_PTD1_UART2_RX,
	MVF600_PAD81_PTD2_UART2_RTS,
	MVF600_PAD82_PTD3_UART2_CTS,

	/* USB */
	MVF600_PAD83_PTD4__USBH_PEN,
	MVF600_PAD102_PTC29__USBC_DET, /* multiplexed USB0_VBUS_DET */
	MVF600_PAD108_PTE3__USB_OC,

	/* PWM */
	MVF600_PAD22_PTB0_FTM0CH0, //PWM<A> multiplexed MVF600_PAD52_PTC7_VID7
	MVF600_PAD23_PTB1_FTM0CH1, //PWM<c>
	MVF600_PAD30_PTB8_FTM1CH0, //PWM<B>
	MVF600_PAD31_PTB9_FTM1CH1, //PWM<D> multiplexed MVF600_PAD51_PTC6_VID6

#if 0
	/* NAND */
	MVF600_PAD71_PTD23_NF_IO7,
	MVF600_PAD72_PTD22_NF_IO6,
	MVF600_PAD73_PTD21_NF_IO5,
	MVF600_PAD74_PTD20_NF_IO4,
	MVF600_PAD75_PTD19_NF_IO3,
	MVF600_PAD76_PTD18_NF_IO2,
	MVF600_PAD77_PTD17_NF_IO1,
	MVF600_PAD78_PTD16_NF_IO0,
	MVF600_PAD94_PTB24_NF_WE,
	MVF600_PAD95_PTB25_NF_CE0,
	MVF600_PAD97_PTB27_NF_RE,
	MVF600_PAD99_PTC26_NF_RB,
	MVF600_PAD100_PTC27_NF_ALE,
	MVF600_PAD101_PTC28_NF_CLE,
#endif

//MVF600_PAD2_PTA9_GPIO,	/* carefull also used for JTAG JTDI, may be used
//				   for RMII_CLKOUT */
//MVF600_PAD7_PTA17_GPIO,
//MVF600_PAD38_PTB16_GPIO,	/* carefull also used as SW1_WAKEUP_PIN */
//MVF600_PAD39_PTB17_GPIO,
//MVF600_PAD40_PTB18_GPIO,	/* IOMUXC_CCM_AUD_EXT_CLK_SELECT_INPUT 2
//				   Selecting Pad: PTB18 for Mode: ALT2. */
//MVF600_PAD41_PTB19_GPIO,
//MVF600_PAD43_PTB21_GPIO,	/* CAN_INT */
//MVF600_PAD44_PTB22_GPIO,
//MVF600_PAD63_PTD31_GPIO,
//MVF600_PAD65_PTD29_GPIO,
//MVF600_PAD66_PTD28_GPIO,
//MVF600_PAD67_PTD27_GPIO,
//MVF600_PAD68_PTD26_GPIO,
//MVF600_PAD69_PTD25_GPIO,
//MVF600_PAD70_PTD24_GPIO,
//MVF600_PAD88_PTD9_GPIO,
//MVF600_PAD89_PTD10_GPIO,
//MVF600_PAD90_PTD11_GPIO,
//MVF600_PAD92_PTD13_GPIO,
//MVF600_PAD93_PTB23_GPIO,
//MVF600_PAD96_PTB26_GPIO,
//MVF600_PAD98_PTB28_GPIO,
//MVF600_PAD103_PTC30_GPIO,

//optional secondary pinmux
//MVF600_PAD28_PTB6_VIDHSYNC,
//MVF600_PAD29_PTB7_VIDVSYNC,
//MVF600_PAD46_PTC1_VID1,
//MVF600_PAD47_PTC2_VID2,
//MVF600_PAD48_PTC3_VID3,
//MVF600_PAD49_PTC4_VID4,
//MVF600_PAD50_PTC5_VID5,
//MVF600_PAD51_PTC6_VID6,	/* multiplexed MVF600_PAD31_PTB9_FTM1CH1 */
//MVF600_PAD52_PTC7_VID7,	/* multiplexed MVF600_PAD22_PTB0_FTM0CH0 */
//MVF600_PAD53_PTC8_VID8,
//MVF600_PAD64_PTD30_VID10,
//MVF600_PAD91_PTD12_VID,	/* VIDMCLK? */
//MVF600_PAD134_PTA7_VIDPCLK,	/* IOMUXC_VIDEO_IN0_IPP_IND_PIX_CLK_SELECT_INPUT
//				   1 Selecting Pad: PTA7 for Mode: ALT1. */

//MVF600_PAD104_PTC31_ADC1_SE5,	/* nVDD_FAULT/SENSE */
//MVF600_PAD25_PTB3_ADC1_SE3,	/* nBATT_FAULT/SENSE */

//VADCSE0
//VADCSE1
//VADCSE2
//VADCSE3

//EXT_TAMPER0
//EXT_TAMPER1
//EXT_TAMPER2/EXT_WM0_TAMPER_IN
//EXT_TAMPER3/EXT_WM0_TAMPER_OUT
//EXT_TAMPER4/EXT_WM1_TAMPER_IN
//EXT_TAMPER5/EXT_WM1_TAMPER_OUT

//IOMUXC_VIDEO_IN0_IPP_IND_DE_SELECT_INPUT: PTB5, PTB8 or PTB10 as ALT5
};

static iomux_v3_cfg_t colibri_vf50_pads[] = {
	/* Touchscreen */
	MVF600_PAD4_PTA11,
	MVF600_PAD5_PTA12,
	MVF600_PAD6_PTA16_ADC1_SE0,
	MVF600_PAD8_PTA18_ADC0_SE0,
	MVF600_PAD9_PTA19_ADC0_SE1,
	MVF600_PAD12_PTA22,
	MVF600_PAD13_PTA23,
	MVF600_PAD24_PTB2_ADC1_SE2,
};

static iomux_v3_cfg_t colibri_vf61_pads[] = {
	/* SAI2: AC97/Touchscreen */
	MVF600_PAD4_PTA11_WM9715L_PENDOWN,	/* carefull also used for JTAG
						   JTMS/SWDIO */
	MVF600_PAD6_PTA16_SAI2_TX_BCLK,		/* AC97_BIT_CLK */
	MVF600_PAD8_PTA18_WM9715L_SDATAOUT,	/* AC97_SDATA_OUT, initially
			driven low to avoid wolfson entering test mode */
	MVF600_PAD9_PTA19_WM9715L_SYNC,		/* AC97_SYNC, initially used to
			do wolfson warm reset by toggling it as a GPIO */
	MVF600_PAD12_PTA22_SAI2_RX_DATA,	/* AC97_SDATA_IN */
	MVF600_PAD13_PTA23_WM9715L_RESET,
	MVF600_PAD24_PTB2_WM9715L_GENIRQ,
	MVF600_PAD93_PTB23_SAI0_TX_BCLK,	/* AC97_MCLK */
};

static struct imxuart_platform_data mvf_uart0_pdata = {
//IMXUART_USE_DCEDTE not supported on Vybrid (i.MX only)
//IMXUART_EDMA
//IMXUART_FIFO
//49.4.8 ISO-7816/smartcard support
//49.4.9 Infrared interface (aka IrDA up to 115.2 kbits/s)
//49.8.7.2 Transceiver driver enable using RTS (e.g. for RS485 driver)
	.flags = IMXUART_FIFO | IMXUART_EDMA | IMXUART_HAVE_RTSCTS,
	.dma_req_rx = DMA_MUX03_UART0_RX,
	.dma_req_tx = DMA_MUX03_UART0_TX,
};

static struct imxuart_platform_data mvf_uart1_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART1_RX,
	.dma_req_tx = DMA_MUX03_UART1_TX,
};

static struct imxuart_platform_data mvf_uart2_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA | IMXUART_HAVE_RTSCTS,
	.dma_req_rx = DMA_MUX03_UART2_RX,
	.dma_req_tx = DMA_MUX03_UART2_TX,
};

static inline void mvf_vf700_init_uart(void)
{
	mvf_add_imx_uart(0, &mvf_uart0_pdata);
	mvf_add_imx_uart(1, &mvf_uart1_pdata);
	mvf_add_imx_uart(2, &mvf_uart2_pdata);
}

static int colibri_ts_mux_pen_interrupt(struct platform_device *pdev)
{
	mxc_iomux_v3_setup_pad(MVF600_PAD8_PTA18);
	mxc_iomux_v3_setup_pad(MVF600_PAD9_PTA19);

	dev_dbg(&pdev->dev, "Muxed XP/XM as GPIO\n");

	return 0;
}

static int colibri_ts_mux_adc(struct platform_device *pdev)
{
	mxc_iomux_v3_setup_pad(MVF600_PAD8_PTA18_ADC0_SE0);
	mxc_iomux_v3_setup_pad(MVF600_PAD9_PTA19_ADC0_SE1);

	dev_dbg(&pdev->dev, "Muxed XP/XM for ADC mode\n");

	return 0;
}


static struct colibri_ts_platform_data colibri_ts_pdata = {
	.mux_pen_interrupt = &colibri_ts_mux_pen_interrupt,
	.mux_adc = &colibri_ts_mux_adc,
	.gpio_pen = 8, /* PAD8 */
};

struct platform_device *__init colibri_add_touchdev(
		const struct colibri_ts_platform_data *pdata)
{
	return imx_add_platform_device("colibri-vf50-ts", 0, NULL, 0,
			pdata, sizeof(*pdata));
}

static struct fec_platform_data fec_data __initdata = {
	.phy = PHY_INTERFACE_MODE_RMII,
};

static int mvf_vf600_spi_cs[] = {
	84,
};

static const struct spi_mvf_master mvf_vf600_spi_data __initconst = {
	.bus_num = 1,
	.chipselect = mvf_vf600_spi_cs,
	.num_chipselect = ARRAY_SIZE(mvf_vf600_spi_cs),
	.cs_control = NULL,
};

static struct spi_board_info mvf_spi_board_info[] __initdata = {
	{
		.bus_num	= 1,		/* DSPI1: Colibri SSP */
		.chip_select	= 0,
		.irq		= 0,
		.max_speed_hz	= 50000000,
		.modalias	= "spidev",
		.mode		= SPI_MODE_0,
		.platform_data	= NULL,
	},
};

static void spi_device_init(void)
{
	spi_register_board_info(mvf_spi_board_info,
				ARRAY_SIZE(mvf_spi_board_info));
}

static void vf600_suspend_enter(void)
{
	/* suspend preparation */
}

static void vf600_suspend_exit(void)
{
	/* resmue resore */
}

static const struct pm_platform_data mvf_vf600_pm_data __initconst = {
	.name = "mvf_pm",
	.suspend_enter = vf600_suspend_enter,
	.suspend_exit = vf600_suspend_exit,
};

static int colibri_vf50_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(colibri_vf50_bl_enb, "BL_ON");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(colibri_vf50_bl_enb, 1);
	if (ret < 0)
		gpio_free(colibri_vf50_bl_enb);

	return ret;
};

static void colibri_vf50_backlight_exit(struct device *dev) {
	gpio_set_value(colibri_vf50_bl_enb, 0);
	gpio_free(colibri_vf50_bl_enb);
}

static int colibri_vf50_backlight_notify(struct device *dev, int brightness)
{
	struct platform_pwm_backlight_data *pdata = dev->platform_data;

	gpio_set_value(colibri_vf50_bl_enb, !!brightness);

	/* Unified TFT interface displays (e.g. EDT ET070080DH6) LEDCTRL pin
	   with inverted behaviour (e.g. 0V brightest vs. 3.3V darkest)
	   Note: brightness polarity display model specific */
	if (brightness)	return pdata->max_brightness - brightness;
	else return brightness;
}

static struct platform_pwm_backlight_data colibri_vf50_backlight_data = {
	.pwm_id		= 1, /* PWM<A> (FTM0CH0) */
	.max_brightness	= 255,
	.dft_brightness	= 127,
	.pwm_period_ns	= 1000000, /* 1 kHz */
	.init		= colibri_vf50_backlight_init,
	.exit		= colibri_vf50_backlight_exit,
	.notify		= colibri_vf50_backlight_notify,
};

static struct mvf_dcu_platform_data mvf_dcu_pdata = {
	.mode_str	= "640x480",
	.default_bpp	= 16,
};

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	if (!mi->nr_banks)
		arm_add_memory(PHYS_OFFSET, SZ_128M);
}

/*
 * Not defined the cd/wp so far, set it always present for debug */
static const struct esdhc_platform_data mvfa5_sd1_data __initconst = {
	.cd_gpio = MVF600_SD1_CD,
	.wp_gpio = -1,
};

static struct imxi2c_platform_data mvf600_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		/* M41T0M6 real time clock on Iris carrier board */
		I2C_BOARD_INFO("rtc-ds1307", 0x68),
			.type = "m41t00",
	},
};

static struct mxc_nand_platform_data mvf_data __initdata = {
	.width = 1,
};

#if 0
/* PWM LEDs */
static struct led_pwm tegra_leds_pwm[] = {
	{
		.name		= "PWM<A>",
		.pwm_id		= 1,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
	{
		.name		= "PWM<B>",
		.pwm_id		= 1,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
	{
		.name		= "PWM<C>",
		.pwm_id		= 2,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
	{
		.name		= "PWM<D>",
		.pwm_id		= 3,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
};

static struct led_pwm_platform_data tegra_leds_pwm_data = {
	.num_leds	= ARRAY_SIZE(tegra_leds_pwm),
	.leds		= tegra_leds_pwm,
};
#endif

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 3,
};

static void __init mvf_twr_init_usb(void)
{
	imx_otg_base = MVF_IO_ADDRESS(MVF_USBC0_BASE_ADDR);
	/*mvf_set_otghost_vbus_func(mvf_twr_usbotg_vbus);*/
	gpio_request(83, "USBH_PEN");
	gpio_direction_output(83, 0);
#ifdef CONFIG_USB_EHCI_ARC
	mvf_usb_dr2_init();
#endif
#ifdef CONFIG_USB_GADGET_ARC
	mvf_usb_dr_init();
#endif
}

static void __init mvf_init_adc(void)
{
	mvf_add_adc(0);
	mvf_add_adc(1);
}

/*!
 * Board specific initialization.
 */
static void __init mvf_board_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mvf600_pads,
					ARRAY_SIZE(mvf600_pads));
	mvf_vf700_init_uart();

#ifdef CONFIG_FEC
	mvf_init_fec(fec_data);
#endif

	mvf_add_snvs_rtc();

	mvf_init_adc();

	mvf_add_pm_imx(0, &mvf_vf600_pm_data);

	mvf700_add_caam();

	mvf_add_sdhci_esdhc_imx(1, &mvfa5_sd1_data);

	mvf_add_imx_i2c(0, &mvf600_i2c_data);
	i2c_register_board_info(0, mxc_i2c0_board_info,
			ARRAY_SIZE(mxc_i2c0_board_info));

//	mvf_add_dspi(0, &mvf_vf600_spi_data);
//	spi_device_init();

	mvfa5_add_dcu(0, &mvf_dcu_pdata);
	mvf_add_mxc_pwm(0);
	mvf_add_mxc_pwm_backlight(0, &colibri_vf50_backlight_data);

	mvf_add_wdt(0);

	mvf_twr_init_usb();

	mvf_add_nand(&mvf_data);

#if 0
	mvf_add_mxc_pwm(0);
//	mvf_add_mxc_pwm(1);
//	mvf_add_mxc_pwm(2);
//	mvf_add_mxc_pwm(3);
	mvf_add_pwm_leds(&tegra_leds_pwm_data);
#endif

	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	mvf_add_asrc(&imx_asrc_data);
}

static void __init colibri_vf50_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(colibri_vf50_pads,
					ARRAY_SIZE(colibri_vf50_pads));

	mvf_board_init();

	colibri_add_touchdev(&colibri_ts_pdata);
}

static void __init colibri_vf61_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(colibri_vf61_pads,
					ARRAY_SIZE(colibri_vf61_pads));

	mvf_board_init();

	mvfa5_add_sai(2, NULL);
}

static void __init mvf_timer_init(void)
{
	struct clk *uart_clk;
	uart_clk = clk_get_sys("mvf-uart.0", NULL);
	early_console_setup(MVF_UART0_BASE_ADDR, uart_clk);

	mvf_clocks_init(32768, 24000000, 0, 0);
}

static struct sys_timer mxc_timer = {
	.init   = mvf_timer_init,
};

/*
 * initialize __mach_desc_ data structure.
 */
MACHINE_START(COLIBRI_VF50, "Toradex Colibri VF50 Module")
	.boot_params = MVF_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.init_irq = mvf_init_irq,
	.init_machine = colibri_vf50_init,
	.map_io = mvf_map_io,
	.timer = &mxc_timer,
MACHINE_END

MACHINE_START(COLIBRI_VF61, "Toradex Colibri VF61 Module")
	.boot_params = MVF_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.init_irq = mvf_init_irq,
	.init_machine = colibri_vf61_init,
	.map_io = mvf_map_io,
	.timer = &mxc_timer,
MACHINE_END
