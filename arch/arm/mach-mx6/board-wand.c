
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/phy.h>

#include <mach/common.h>
#include <mach/devices-common.h>
#include <mach/gpio.h>
#include <mach/iomux-mx6dl.h>
#include <mach/iomux-v3.h>
#include <mach/mx6.h>

#include "devices-imx6q.h"
#include "usb.h"

#define WAND_RGMII_INT		IMX_GPIO_NR(1, 28)
#define WAND_RGMII_RST		IMX_GPIO_NR(3, 29)

#define WAND_SD1_CD		IMX_GPIO_NR(1, 2)
#define WAND_SD3_CD		IMX_GPIO_NR(3, 9)
#define WAND_SD3_WP		IMX_GPIO_NR(1, 10)

#define WAND_USB_OTG_OC		IMX_GPIO_NR(1, 9)
#define WAND_USB_OTG_PWR	IMX_GPIO_NR(3, 22)
#define WAND_USB_H1_OC		IMX_GPIO_NR(3, 30)

/* Syntactic sugar for pad configuration */
#define WAND_SETUP_PADS(p) \
        mxc_iomux_v3_setup_multiple_pads((p), ARRAY_SIZE(p))

/* See arch/arm/plat-mxc/include/mach/iomux-mx6dl.h for definitions */

/****************************************************************************
 *                                                                          
 * DMA controller init
 *                                                                          
 ****************************************************************************/

static __init void wand_init_dma(void) {
        imx6q_add_dma();        
}


/****************************************************************************
 *                                                                          
 * SD init
 *
 * SD1 is routed to EDM connector (external SD on wand baseboard)
 * SD2 is WiFi
 * SD3 is boot SD on the module
 *                                                                          
 ****************************************************************************/

#define WAND_SD3_PADS 6

static const iomux_v3_cfg_t wand_sd_pads[3][3][WAND_SD3_PADS] = {
	{	{
		MX6DL_PAD_SD1_CLK__USDHC1_CLK_50MHZ_40OHM,
		MX6DL_PAD_SD1_CMD__USDHC1_CMD_50MHZ_40OHM,
		MX6DL_PAD_SD1_DAT0__USDHC1_DAT0_50MHZ_40OHM,
		MX6DL_PAD_SD1_DAT1__USDHC1_DAT1_50MHZ_40OHM,
		MX6DL_PAD_SD1_DAT2__USDHC1_DAT2_50MHZ_40OHM,
		MX6DL_PAD_SD1_DAT3__USDHC1_DAT3_50MHZ_40OHM,
		}, {
		MX6DL_PAD_SD1_CLK__USDHC1_CLK_100MHZ_40OHM,
		MX6DL_PAD_SD1_CMD__USDHC1_CMD_100MHZ_40OHM,
		MX6DL_PAD_SD1_DAT0__USDHC1_DAT0_100MHZ_40OHM,
		MX6DL_PAD_SD1_DAT1__USDHC1_DAT1_100MHZ_40OHM,
		MX6DL_PAD_SD1_DAT2__USDHC1_DAT2_100MHZ_40OHM,
		MX6DL_PAD_SD1_DAT3__USDHC1_DAT3_100MHZ_40OHM,
		}, {
		MX6DL_PAD_SD1_CLK__USDHC1_CLK_200MHZ_40OHM,
		MX6DL_PAD_SD1_CMD__USDHC1_CMD_200MHZ_40OHM,
		MX6DL_PAD_SD1_DAT0__USDHC1_DAT0_200MHZ_40OHM,
		MX6DL_PAD_SD1_DAT1__USDHC1_DAT1_200MHZ_40OHM,
		MX6DL_PAD_SD1_DAT2__USDHC1_DAT2_200MHZ_40OHM,
		MX6DL_PAD_SD1_DAT3__USDHC1_DAT3_200MHZ_40OHM,
                }
        }, {	{
		MX6DL_PAD_SD2_CLK__USDHC2_CLK_50MHZ,
		MX6DL_PAD_SD2_CMD__USDHC2_CMD_50MHZ,
		MX6DL_PAD_SD2_DAT0__USDHC2_DAT0_50MHZ,
		MX6DL_PAD_SD2_DAT1__USDHC2_DAT1_50MHZ,
		MX6DL_PAD_SD2_DAT2__USDHC2_DAT2_50MHZ,
		MX6DL_PAD_SD2_DAT3__USDHC2_DAT3_50MHZ,
		}, {
		MX6DL_PAD_SD2_CLK__USDHC2_CLK_100MHZ,
		MX6DL_PAD_SD2_CMD__USDHC2_CMD_100MHZ,
		MX6DL_PAD_SD2_DAT0__USDHC2_DAT0_100MHZ,
		MX6DL_PAD_SD2_DAT1__USDHC2_DAT1_100MHZ,
		MX6DL_PAD_SD2_DAT2__USDHC2_DAT2_100MHZ,
		MX6DL_PAD_SD2_DAT3__USDHC2_DAT3_100MHZ,
		}, {
		MX6DL_PAD_SD2_CLK__USDHC2_CLK_200MHZ,
		MX6DL_PAD_SD2_CMD__USDHC2_CMD_200MHZ,
		MX6DL_PAD_SD2_DAT0__USDHC2_DAT0_200MHZ,
		MX6DL_PAD_SD2_DAT1__USDHC2_DAT1_200MHZ,
		MX6DL_PAD_SD2_DAT2__USDHC2_DAT2_200MHZ,
		MX6DL_PAD_SD2_DAT3__USDHC2_DAT3_200MHZ,
		}
        }, {	{
		MX6DL_PAD_SD3_CLK__USDHC3_CLK_50MHZ,
		MX6DL_PAD_SD3_CMD__USDHC3_CMD_50MHZ,
		MX6DL_PAD_SD3_DAT0__USDHC3_DAT0_50MHZ,
		MX6DL_PAD_SD3_DAT1__USDHC3_DAT1_50MHZ,
		MX6DL_PAD_SD3_DAT2__USDHC3_DAT2_50MHZ,
		MX6DL_PAD_SD3_DAT3__USDHC3_DAT3_50MHZ,
	        }, {
		MX6DL_PAD_SD3_CLK__USDHC3_CLK_100MHZ,
		MX6DL_PAD_SD3_CMD__USDHC3_CMD_100MHZ,
		MX6DL_PAD_SD3_DAT0__USDHC3_DAT0_100MHZ,
		MX6DL_PAD_SD3_DAT1__USDHC3_DAT1_100MHZ,
		MX6DL_PAD_SD3_DAT2__USDHC3_DAT2_100MHZ,
		MX6DL_PAD_SD3_DAT3__USDHC3_DAT3_100MHZ,
	        }, {
		MX6DL_PAD_SD3_CLK__USDHC3_CLK_200MHZ,
		MX6DL_PAD_SD3_CMD__USDHC3_CMD_200MHZ,
		MX6DL_PAD_SD3_DAT0__USDHC3_DAT0_200MHZ,
		MX6DL_PAD_SD3_DAT1__USDHC3_DAT1_200MHZ,
		MX6DL_PAD_SD3_DAT2__USDHC3_DAT2_200MHZ,
		MX6DL_PAD_SD3_DAT3__USDHC3_DAT3_200MHZ,
		}                
        }
};

/* ------------------------------------------------------------------------ */

static int wand_sd_speed_change(unsigned int sd, int clock) {
	static int pad_speed[3] = { 200, 200, 200 };

	if (clock > 100000000) {                
		if (pad_speed[sd] == 200) return 0;
		pad_speed[sd] = 200;
		return WAND_SETUP_PADS(wand_sd_pads[sd][2]);
	} else if (clock > 52000000) {
		if (pad_speed[sd] == 100) return 0;
		pad_speed[sd] = 100;
		return WAND_SETUP_PADS(wand_sd_pads[sd][1]); 
	} else {
		if (pad_speed[sd] == 25) return 0;
		pad_speed[sd] = 25;
		return WAND_SETUP_PADS(wand_sd_pads[sd][0]);
	}
}

/* ------------------------------------------------------------------------ */

static const struct esdhc_platform_data wand_sd_data[3] = {
	{
		.cd_gpio		= WAND_SD1_CD,
		.wp_gpio		=-EINVAL,
		.keep_power_at_suspend	= 1,
	        .support_8bit		= 0,
		.platform_pad_change	= wand_sd_speed_change,
	}, {
		.cd_gpio		=-EINVAL,
		.wp_gpio		=-EINVAL,
		.platform_pad_change	= wand_sd_speed_change,
	}, {
		.cd_gpio		= WAND_SD3_CD,
		.wp_gpio		= WAND_SD3_WP,
		.keep_power_at_suspend	= 1,
		.support_18v		= 1,
		.support_8bit		= 0,
		.delay_line		= 0,
		.platform_pad_change	= wand_sd_speed_change,
	}
};

/* ------------------------------------------------------------------------ */

static void wand_init_sd(void) {
	int i;
	/* Card Detect for SD1 & SD3, respectively */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_2__GPIO_1_2); 
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA9__GPIO_3_9);

	/* Add mmc devices in reverse order, so mmc0 always is boot sd (SD3) */
	for (i=2; i>=0; i--) {
		WAND_SETUP_PADS(wand_sd_pads[i][0]);
                imx6q_add_sdhci_usdhc_imx(i, &wand_sd_data[i]);
	}
}


/****************************************************************************
 *                                                                          
 * I2C
 *                                                                          
 ****************************************************************************/

static const iomux_v3_cfg_t wand_i2c_pads[3][2] __initdata = {
	{        
	MX6DL_PAD_EIM_D21__I2C1_SCL,
	MX6DL_PAD_EIM_D28__I2C1_SDA,
	}, {
	MX6DL_PAD_KEY_COL3__I2C2_SCL,
	MX6DL_PAD_KEY_ROW3__I2C2_SDA,
	}, {
	MX6DL_PAD_GPIO_5__I2C3_SCL,
	MX6DL_PAD_GPIO_16__I2C3_SDA
	}
};

/* ------------------------------------------------------------------------ */

static struct imxi2c_platform_data wand_i2c_data[] = {
	{ .bitrate	= 100000, },
	{ .bitrate	= 400000, },
	{ .bitrate	= 400000, },
};

/* ------------------------------------------------------------------------ */

static void __init wand_init_i2c(void) {
        int i;
	for (i=0; i<3; i++) {
		WAND_SETUP_PADS(wand_i2c_pads[i]);
		imx6q_add_imx_i2c(i, &wand_i2c_data[i]);
        }
}


/****************************************************************************
 *                                                                          
 * Initialize debug console (UART1)
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_uart_pads[] = {
	/* UART1 (debug console) */
        MX6DL_PAD_CSI0_DAT10__UART1_TXD,
        MX6DL_PAD_CSI0_DAT11__UART1_RXD,
        MX6DL_PAD_EIM_D19__UART1_CTS,
        MX6DL_PAD_EIM_D20__UART1_RTS,
};

/* ------------------------------------------------------------------------ */
 
static __init void wand_init_uart(void) {
        WAND_SETUP_PADS(wand_uart_pads);

	imx6q_add_imx_uart(0, NULL);
}


/****************************************************************************
 *                                                                          
 * Initialize sound (SSI, ASRC, AUD3 channel and S/PDIF)
 *                                                                          
 ****************************************************************************/

static const iomux_v3_cfg_t wand_audio_pads[] = {
        MX6DL_PAD_CSI0_DAT4__AUDMUX_AUD3_TXC,
        MX6DL_PAD_CSI0_DAT5__AUDMUX_AUD3_TXD,
        MX6DL_PAD_CSI0_DAT6__AUDMUX_AUD3_TXFS,
        MX6DL_PAD_CSI0_DAT7__AUDMUX_AUD3_RXD,
        MX6DL_PAD_GPIO_0__CCM_CLKO,
};

/* ------------------------------------------------------------------------ */

extern struct mxc_audio_platform_data wand_audio_channel_data;

/* This function is called as a callback from the audio channel data struct */
static int wand_audio_clock_enable(void) {
	struct clk *clko;
	struct clk *new_parent;
	int rate;

	clko = clk_get(NULL, "clko_clk");
	if (IS_ERR(clko)) return PTR_ERR(clko);
	
        new_parent = clk_get(NULL, "ahb");
	if (!IS_ERR(new_parent)) {
		clk_set_parent(clko, new_parent);
		clk_put(new_parent);
	}
        
	rate = clk_round_rate(clko, 16000000);
	if (rate < 8000000 || rate > 27000000) {
		pr_err("SGTL5000: mclk freq %d out of range!\n", rate);
		clk_put(clko);
		return -1;
	}

        wand_audio_channel_data.sysclk = rate;
	clk_set_rate(clko, rate);
	clk_enable(clko);
        
	return 0;        
}

/* ------------------------------------------------------------------------ */

/* This struct is added by the baseboard when initializing the codec */
struct mxc_audio_platform_data wand_audio_channel_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3, /* audio channel: 3=AUD3. TODO: EDM */
	.init = wand_audio_clock_enable,
	.hp_gpio = -1,
};
EXPORT_SYMBOL_GPL(wand_audio_channel_data); /* TODO: edm naming? */

/* ------------------------------------------------------------------------ */

static int wand_set_spdif_clk_rate(struct clk *clk, unsigned long rate) {
	unsigned long rate_actual;
	rate_actual = clk_round_rate(clk, rate);
	clk_set_rate(clk, rate_actual);
	return 0;
}

/* ------------------------------------------------------------------------ */

static struct mxc_spdif_platform_data wand_spdif = {
	.spdif_tx		= 1,	/* enable tx */
	.spdif_rx		= 1,	/* enable rx */
	.spdif_clk_44100	= 1,    /* tx clk from spdif0_clk_root */
	.spdif_clk_48000	= 1,    /* tx clk from spdif0_clk_root */
	.spdif_div_44100	= 23,
	.spdif_div_48000	= 37,
	.spdif_div_32000	= 37,
	.spdif_rx_clk		= 0,    /* rx clk from spdif stream */
	.spdif_clk_set_rate	= wand_set_spdif_clk_rate,
	.spdif_clk		= NULL, /* spdif bus clk */
};

/* ------------------------------------------------------------------------ */

static struct imx_ssi_platform_data wand_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

/* ------------------------------------------------------------------------ */

static struct imx_asrc_platform_data wand_asrc_data = {
	.channel_bits	= 4,
	.clk_map_ver	= 2,
};

/* ------------------------------------------------------------------------ */

void __init wand_init_audio(void) {
        WAND_SETUP_PADS(wand_audio_pads);
        
        /* Sample rate converter is added together with audio */
        wand_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
        wand_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&wand_asrc_data);

	imx6q_add_imx_ssi(1, &wand_ssi_pdata);
	/* Enable SPDIF */

	mxc_iomux_v3_setup_pad(MX6DL_PAD_ENET_RXD0__SPDIF_OUT1);

	wand_spdif.spdif_core_clk = clk_get_sys("mxc_spdif.0", NULL);
	clk_put(wand_spdif.spdif_core_clk);
	imx6q_add_spdif(&wand_spdif);                
	imx6q_add_spdif_dai();
	imx6q_add_spdif_audio_device();
}


/*****************************************************************************
 *                                                                           
 * Init FEC and AR8031 PHY
 *                                                                            
 *****************************************************************************/

static const iomux_v3_cfg_t wand_fec_pads[] = {
        MX6DL_PAD_ENET_MDIO__ENET_MDIO,
        MX6DL_PAD_ENET_MDC__ENET_MDC,
        
        MX6DL_PAD_ENET_REF_CLK__ENET_TX_CLK,
        
	MX6DL_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6DL_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6DL_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6DL_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6DL_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6DL_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	MX6DL_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6DL_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6DL_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6DL_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6DL_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6DL_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
                
        MX6DL_PAD_ENET_TX_EN__GPIO_1_28,
        MX6DL_PAD_EIM_D29__GPIO_3_29,
};

/* ------------------------------------------------------------------------ */

static int wand_fec_phy_init(struct phy_device *phydev) {
	unsigned short val;

	/* Enable AR8031 125MHz clk */
	phy_write(phydev, 0x0d, 0x0007); /* Set device address to 7*/
	phy_write(phydev, 0x00, 0x8000); /* Apply by soft reset */
	udelay(500); 
        
	phy_write(phydev, 0x0e, 0x8016); /* set mmd reg */
	phy_write(phydev, 0x0d, 0x4007); /* apply */

	val = phy_read(phydev, 0xe);
	val &= 0xffe3;
	val |= 0x18;
	phy_write(phydev, 0xe, val);
	phy_write(phydev, 0x0d, 0x4007); /* Post data */        

	/* Introduce random tx clock delay. Why is this needed? */
	phy_write(phydev, 0x1d, 0x5);
	val = phy_read(phydev, 0x1e);
	val |= 0x0100;
	phy_write(phydev, 0x1e, val);

        phy_print_status(phydev);

	return 0;
}

/* ------------------------------------------------------------------------ */

static int wand_fec_power_hibernate(struct phy_device *phydev) { return 0; }

/* ------------------------------------------------------------------------ */

static struct fec_platform_data wand_fec_data = {
	.init			= wand_fec_phy_init,
	.power_hibernate	= wand_fec_power_hibernate,
	.phy			= PHY_INTERFACE_MODE_RGMII,
	.phy_noscan_mask	= ~2, /* phy is on adress 1 */
};

/* ------------------------------------------------------------------------ */

static __init void wand_init_ethernet(void) {
	WAND_SETUP_PADS(wand_fec_pads);
        
	gpio_request(WAND_RGMII_RST, "rgmii reset");
	gpio_direction_output(WAND_RGMII_RST, 0);
#ifdef CONFIG_FEC_1588
	mxc_iomux_set_gpr_register(1, 21, 1, 1);
#endif
	mdelay(6);
	gpio_set_value(WAND_RGMII_RST, 1);
	imx6_init_fec(wand_fec_data);
}


/****************************************************************************
 *                                                                          
 * USB
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_usb_pads[] = {
        MX6DL_PAD_GPIO_9__GPIO_1_9,
        MX6DL_PAD_GPIO_1__USBOTG_ID,
        MX6DL_PAD_EIM_D22__GPIO_3_22,
        MX6DL_PAD_EIM_D30__GPIO_3_30
};

/* ------------------------------------------------------------------------ */

static void wand_usbotg_vbus(bool on) {
        gpio_set_value_cansleep(WAND_USB_OTG_PWR, on);
}

/* ------------------------------------------------------------------------ */

static __init void wand_init_usb(void) {
        WAND_SETUP_PADS(wand_usb_pads);
        
        gpio_request(WAND_USB_OTG_OC, "otg oc");
	gpio_direction_input(WAND_USB_OTG_OC);

        gpio_request(WAND_USB_OTG_PWR, "otg pwr");
        gpio_direction_output(WAND_USB_OTG_PWR, 1);

	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	mxc_iomux_set_gpr_register(1, 13, 1, 0);

	mx6_set_otghost_vbus_func(wand_usbotg_vbus);

        gpio_request(WAND_USB_H1_OC, "usbh1 oc");
	gpio_direction_input(WAND_USB_H1_OC);
}


/****************************************************************************
 *                                                                          
 * IPU
 *                                                                          
 ****************************************************************************/

static struct imx_ipuv3_platform_data wand_ipu_data[] = {
	{
		.rev		= 4,
		.csi_clk[0]	= "ccm_clk0",
	}, {
		.rev		= 4,
		.csi_clk[0]	= "ccm_clk0",
	},
};

/* ------------------------------------------------------------------------ */

static __init void wand_init_ipu(void) {
	imx6q_add_ipuv3(0, &wand_ipu_data[0]);
}


/****************************************************************************
 *                                                                          
 * HDMI
 *                                                                          
 ****************************************************************************/

static struct ipuv3_fb_platform_data wand_hdmi_fb[] = {
	{ /* hdmi framebuffer */
		.disp_dev		= "hdmi",
		.interface_pix_fmt	= IPU_PIX_FMT_RGB32,
		.mode_str		= "1920x1080@60",
		.default_bpp		= 32,
		.int_clk		= false,
	}
};

/* ------------------------------------------------------------------------ */

static void wand_hdmi_init(int ipu_id, int disp_id) {
	if ((unsigned)ipu_id > 1) ipu_id = 0;
	if ((unsigned)disp_id > 1) disp_id = 0;

	mxc_iomux_set_gpr_register(3, 2, 2, 2*ipu_id + disp_id);
}

/* ------------------------------------------------------------------------ */

static struct fsl_mxc_hdmi_platform_data wand_hdmi_data = {
	.init = wand_hdmi_init,
};

/* ------------------------------------------------------------------------ */

static struct fsl_mxc_hdmi_core_platform_data wand_hdmi_core_data = {
	.ipu_id		= 0,
	.disp_id	= 1,
};

/* ------------------------------------------------------------------------ */

static const struct i2c_board_info wand_hdmi_i2c_info = {
	I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
};

/* ------------------------------------------------------------------------ */

static void wand_init_hdmi(void) {
	i2c_register_board_info(0, &wand_hdmi_i2c_info, 1);
	imx6q_add_mxc_hdmi_core(&wand_hdmi_core_data);
	imx6q_add_mxc_hdmi(&wand_hdmi_data);
	imx6q_add_ipuv3fb(0, wand_hdmi_fb);
        
        /* Enable HDMI audio */
	imx6q_add_hdmi_soc();
	imx6q_add_hdmi_soc_dai();        
	mxc_iomux_set_gpr_register(0, 0, 1, 1);
}


/*****************************************************************************
 *                                                                           
 * Init clocks and early boot console                                      
 *                                                                            
 *****************************************************************************/

extern void __iomem *twd_base;

static void __init wand_init_timer(void) {
	struct clk *uart_clk;
#ifdef CONFIG_LOCAL_TIMERS
	twd_base = ioremap(LOCAL_TWD_ADDR, SZ_256);
#endif
	mx6_clocks_init(32768, 24000000, 0, 0);

	uart_clk = clk_get_sys("imx-uart.0", NULL);
	early_console_setup(UART1_BASE_ADDR, uart_clk);
}

/* ------------------------------------------------------------------------ */

static struct sys_timer wand_timer = {
	.init = wand_init_timer,
};


/*****************************************************************************
 *                                                                           
 * BOARD INIT                                                                
 *                                                                            
 *****************************************************************************/

static void __init wand_board_init(void) {
	wand_init_dma();
	wand_init_uart();
	wand_init_sd();
	wand_init_i2c();
	wand_init_audio();
	wand_init_ethernet();
	wand_init_usb();
	wand_init_ipu();
	wand_init_hdmi();
}

/* ------------------------------------------------------------------------ */
        
MACHINE_START(WANDBOARD, "Wandboard")
	.boot_params	= MX6_PHYS_OFFSET + 0x100,
	.map_io		= mx6_map_io,
	.init_irq	= mx6_init_irq,
	.init_machine	= wand_board_init,
	.timer		= &wand_timer,
MACHINE_END

