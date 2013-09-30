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
#include <linux/smsc911x.h>
#include <linux/spi/spi.h>
#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
#include <linux/spi/flash.h>
#else
#include <linux/mtd/physmap.h>
#endif
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
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
#include <mach/mxc_dvfs.h>
#include <mach/memory.h>
#include <mach/iomux-mvf.h>
#include <mach/imx-uart.h>
#include <mach/spi-mvf.h>
#include <mach/viv_gpu.h>
#include <mach/ahci_sata.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_hdmi.h>
#include <mach/mxc_asrc.h>
#include <mach/mipi_dsi.h>
#include <mach/mipi_csi2.h>
#include <mach/fsl_l2_switch.h>
#include <mach/mxc.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "devices-mvf.h"
#include "usb.h"
#include "crm_regs.h"

#define MVF600_SD1_CD  42

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

	/* I2C0: I2C_SDA/SCL on SODIMM pin 194/196 (e.g. RTC on carrier board) */
	MVF600_PAD36_PTB14__I2C0_SCL,
	MVF600_PAD37_PTB15__I2C0_SDA,

#if 0
	/*CAN1*/
	MVF600_PAD38_PTB16__CAN1_RX,
	MVF600_PAD39_PTB17__CAN1_TX,

	/* DSPI1: SSP on SODIMM pin 86, 88, 90 and 92 */
	MVF600_PAD84_PTD5__DSPI1_PCS0,
	MVF600_PAD85_PTD6__DSPI1_SIN,
	MVF600_PAD86_PTD7__DSPI1_SOUT,
	MVF600_PAD87_PTD8__DSPI1_SCK,
#endif

#if defined(CONFIG_FEC1) || defined(CONFIG_FSL_L2_SWITCH)
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
#endif

	/* SAI2: AC97 */
#if 0 // Those pins are disabled on Colibri VP50 since touchscreen uses them...
	MVF600_PAD6_PTA16_SAI2_TX_BCLK,
	MVF600_PAD8_PTA18_SAI2_TX_DATA,
	MVF600_PAD9_PTA19_SAI2_TX_SYNC,
	MVF600_PAD12_PTA22_SAI2_RX_DATA,
	MVF600_PAD13_PTA23_SAI2_RX_SYNC,
//	MVF600_PAD5_PTA12_EXT_AUDIO_MCLK,
//	MVF600_PAD24_PTB2 WM9715L GENIRQ
#endif
	/* Touchscreen */
	MVF600_PAD4_PTA11,
	MVF600_PAD5_PTA12,
	MVF600_PAD6_PTA16_ADC1_SE0,
	MVF600_PAD24_PTB2_ADC1_SE2,
	MVF600_PAD8_PTA18_ADC0_SE0,
	MVF600_PAD9_PTA19_ADC0_SE1,
	MVF600_PAD12_PTA22,
	MVF600_PAD13_PTA23,

	/* DCU0: Display */
	MVF600_PAD105_PTE0_DCU0_HSYNC,
	MVF600_PAD106_PTE1_DCU0_VSYNC,
	MVF600_PAD107_PTE2_DCU0_PCLK,
	MVF600_PAD109_PTE4_DCU0_DE,
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

	/* UART1: UART_C */
	MVF600_PAD26_PTB4_UART1_TX,
	MVF600_PAD27_PTB5_UART1_RX,

	/* UART0: UART_A */
	MVF600_PAD32_PTB10_UART0_TX,
	MVF600_PAD33_PTB11_UART0_RX,

#if 0
	/* UART2: UART_B */
	MVF600_PAD79_PTD0_UART2_TX,
	MVF600_PAD80_PTD1_UART2_RX,

	/* USB */
	MVF600_PAD83_PTD4__USBH_PEN,
	MVF600_PAD102_PTC29__USBC_DET,
	MVF600_PAD108_PTE3__USB_OC,
#endif

	/*
	 * PTB6 & PTB7 are commented out as they conflict with uart2
	 * which is the MQX default console (e.g for printf)
	*/
	/* MVF600_PAD28_PTB6_FTM0CH6, */
	/* MVF600_PAD29_PTB7_FTM0CH7, */

	MVF600_PAD22_PTB0_FTM0CH0, //PWM<A> multiplexed
	MVF600_PAD23_PTB1_FTM0CH1, //PWM<c>
	MVF600_PAD30_PTB8_FTM1CH0, //PWM<B>
	MVF600_PAD31_PTB9_FTM1CH1, //PWM<D>

#if 0
	/* Touch Screen */
	MVF600_PAD4_PTA11_TS_IRQ,

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
};

static struct imxuart_platform_data mvf_uart0_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART0_RX,
	.dma_req_tx = DMA_MUX03_UART0_TX,
};

static struct imxuart_platform_data mvf_uart1_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART1_RX,
	.dma_req_tx = DMA_MUX03_UART1_TX,
};

static struct imxuart_platform_data mvf_uart2_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART2_RX,
	.dma_req_tx = DMA_MUX03_UART2_TX,
};

static inline void mvf_vf700_init_uart(void)
{
	mvf_add_imx_uart(0, &mvf_uart0_pdata);
	mvf_add_imx_uart(1, &mvf_uart1_pdata);
	mvf_add_imx_uart(2, &mvf_uart2_pdata);
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

static struct mvf_dcu_platform_data mvf_dcu_pdata = {
	.mode_str	= "640x480",
	.default_bpp	= 24,
};

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
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

/* PWM LEDs */
static struct led_pwm tegra_leds_pwm[] = {
	{
		.name		= "PWM<A>",
		.pwm_id		= 1,
		.max_brightness	= 255,
		.pwm_period_ns	= 19600,
	},
#if 0
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
#endif
};

static struct led_pwm_platform_data tegra_leds_pwm_data = {
	.num_leds	= ARRAY_SIZE(tegra_leds_pwm),
	.leds		= tegra_leds_pwm,
};

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 3,
};

static void __init mvf_twr_init_usb(void)
{
	imx_otg_base = MVF_IO_ADDRESS(MVF_USBC0_BASE_ADDR);
	/*mvf_set_otghost_vbus_func(mvf_twr_usbotg_vbus);*/
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
	mvf_add_adc_touch(0);
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

	mvf_add_wdt(0);

	mvf_twr_init_usb();

	mvf_add_nand(&mvf_data);

	mvf_add_mxc_pwm(0);
//	mvf_add_mxc_pwm(1);
//	mvf_add_mxc_pwm(2);
//	mvf_add_mxc_pwm(3);
	mvf_add_pwm_leds(&tegra_leds_pwm_data);

	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	mvf_add_asrc(&imx_asrc_data);
}

static void __init mvf_timer_init(void)
{
#if 1
	struct clk *uart_clk;
	uart_clk = clk_get_sys("mvf-uart.0", NULL);
	early_console_setup(MVF_UART0_BASE_ADDR, uart_clk);
#endif
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
	.map_io = mvf_map_io,
	.init_irq = mvf_init_irq,
	.init_machine = mvf_board_init,
	.timer = &mxc_timer,
MACHINE_END
