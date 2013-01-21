
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

#include "crm_regs.h"
#include "devices-imx6q.h"
#include "usb.h"

#define WAND_BT_ON		IMX_GPIO_NR(3, 13)
#define WAND_BT_WAKE		IMX_GPIO_NR(3, 14)
#define WAND_BT_HOST_WAKE	IMX_GPIO_NR(3, 15)

#define WAND_RGMII_INT		IMX_GPIO_NR(1, 28)
#define WAND_RGMII_RST		IMX_GPIO_NR(3, 29)

#define WAND_SD1_CD		IMX_GPIO_NR(1, 2)
#define WAND_SD3_CD		IMX_GPIO_NR(3, 9)
#define WAND_SD3_WP		IMX_GPIO_NR(1, 10)

#define WAND_USB_OTG_OC		IMX_GPIO_NR(1, 9)
#define WAND_USB_OTG_PWR	IMX_GPIO_NR(3, 22)
#define WAND_USB_H1_OC		IMX_GPIO_NR(3, 30)

#define WAND_WL_REF_ON		IMX_GPIO_NR(2, 29)
#define WAND_WL_RST_N		IMX_GPIO_NR(5, 2)
#define WAND_WL_REG_ON		IMX_GPIO_NR(1, 26)
#define WAND_WL_HOST_WAKE	IMX_GPIO_NR(1, 29)
#define WAND_WL_WAKE		IMX_GPIO_NR(1, 30)

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
		if (pad_speed[sd] == 50) return 0;
		pad_speed[sd] = 50;
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
		.keep_power_at_suspend	= 1,
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

static const __initconst iomux_v3_cfg_t wand_fec_pads[] = {
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
	msleep(10);
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


/****************************************************************************
 *                                                                          
 * WiFi
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_wifi_pads[] = {
        /* ref_on, enable 32k clock */
        MX6DL_PAD_EIM_EB1__GPIO_2_29,
        /* Wifi reset (resets when low!) */
        MX6DL_PAD_EIM_A25__GPIO_5_2,
        /* reg on, signal to FDC6331L */
        MX6DL_PAD_ENET_RXD1__GPIO_1_26,
        /* host wake */
        MX6DL_PAD_ENET_TXD1__GPIO_1_29,
        /* wl wake - nc */
        MX6DL_PAD_ENET_TXD0__GPIO_1_30,
};

/* ------------------------------------------------------------------------ */

/* assumes SD/MMC pins are set; call after wand_init_sd() */
static __init void wand_init_wifi(void) {
	WAND_SETUP_PADS(wand_wifi_pads);
                
	gpio_request(WAND_WL_RST_N, "wl_rst_n");
	gpio_direction_output(WAND_WL_RST_N, 0);
	msleep(11);
	gpio_set_value(WAND_WL_RST_N, 1);

	gpio_request(WAND_WL_REF_ON, "wl_ref_on");
	gpio_direction_output(WAND_WL_REF_ON, 1);

	gpio_request(WAND_WL_REG_ON, "wl_reg_on");
	gpio_direction_output(WAND_WL_REG_ON, 1);
        
	gpio_request(WAND_WL_WAKE, "wl_wake");
	gpio_direction_output(WAND_WL_WAKE, 1);

	gpio_request(WAND_WL_HOST_WAKE, "wl_host_wake");
	gpio_direction_input(WAND_WL_HOST_WAKE);
}


/****************************************************************************
 *                                                                          
 * Bluetooth
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_bt_pads[] = {
        /* BT_ON, BT_WAKE and BT_HOST_WAKE */
        MX6DL_PAD_EIM_DA13__GPIO_3_13,
        MX6DL_PAD_EIM_DA14__GPIO_3_14,
        MX6DL_PAD_EIM_DA15__GPIO_3_15,

        /* AUD5 channel goes to BT */
        MX6DL_PAD_KEY_COL0__AUDMUX_AUD5_TXC,
        MX6DL_PAD_KEY_ROW0__AUDMUX_AUD5_TXD,
        MX6DL_PAD_KEY_COL1__AUDMUX_AUD5_TXFS,
        MX6DL_PAD_KEY_ROW1__AUDMUX_AUD5_RXD,        
        
        /* Bluetooth is on UART3*/
        MX6DL_PAD_EIM_D23__UART3_CTS,
        MX6DL_PAD_EIM_D24__UART3_TXD,
        MX6DL_PAD_EIM_D25__UART3_RXD,
        MX6DL_PAD_EIM_EB3__UART3_RTS,
};

/* ------------------------------------------------------------------------ */

static const struct imxuart_platform_data wand_bt_uart_data = {
	.flags = IMXUART_HAVE_RTSCTS | IMXUART_SDMA,
	.dma_req_tx = MX6Q_DMA_REQ_UART3_TX,
	.dma_req_rx = MX6Q_DMA_REQ_UART3_RX,
};

/* ------------------------------------------------------------------------ */

/* This assumes wifi is initialized (chip has power) */
static __init void wand_init_bluetooth(void) {
	WAND_SETUP_PADS(wand_bt_pads);
        
	imx6q_add_imx_uart(2, &wand_bt_uart_data);

	gpio_request(WAND_BT_ON, "bt_on");
	gpio_direction_output(WAND_BT_ON, 0);
	msleep(11);
	gpio_set_value(WAND_BT_ON, 1);

	gpio_request(WAND_BT_WAKE, "bt_wake");
	gpio_direction_output(WAND_BT_WAKE, 1);

	gpio_request(WAND_BT_HOST_WAKE, "bt_host_wake");
	gpio_direction_input(WAND_BT_WAKE);
}


/****************************************************************************
 *                                                                          
 * Power and thermal management
 *                                                                          
 ****************************************************************************/

extern bool enable_wait_mode;

static const struct anatop_thermal_platform_data wand_thermal = {
	.name = "anatop_thermal",
};

/* ------------------------------------------------------------------------ */

static void wand_suspend_enter(void) {
	gpio_set_value(WAND_WL_WAKE, 0);
	gpio_set_value(WAND_BT_WAKE, 0);
}

/* ------------------------------------------------------------------------ */

static void wand_suspend_exit(void) {
	gpio_set_value(WAND_WL_WAKE, 1);
	gpio_set_value(WAND_BT_WAKE, 1);
}

/* ------------------------------------------------------------------------ */

static const struct pm_platform_data wand_pm_data = {
	.name		= "imx_pm",
	.suspend_enter	= wand_suspend_enter,
	.suspend_exit	= wand_suspend_exit,
};

/* ------------------------------------------------------------------------ */

static const struct mxc_dvfs_platform_data wand_dvfscore_data = {
	.reg_id			= "cpu_vddgp",
	.clk1_id		= "cpu_clk",
	.clk2_id 		= "gpc_dvfs_clk",
	.gpc_cntr_offset 	= MXC_GPC_CNTR_OFFSET,
	.ccm_cdcr_offset 	= MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset 	= MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset 	= MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask 		= 0x1F800,
	.prediv_offset 		= 11,
	.prediv_val 		= 3,
	.div3ck_mask 		= 0xE0000000,
	.div3ck_offset 		= 29,
	.div3ck_val 		= 2,
	.emac_val 		= 0x08,
	.upthr_val 		= 25,
	.dnthr_val 		= 9,
	.pncthr_val 		= 33,
	.upcnt_val 		= 10,
	.dncnt_val 		= 10,
	.delay_time 		= 80,
};

/* ------------------------------------------------------------------------ */

static __init void wand_init_pm(void) {
	enable_wait_mode = false;
	imx6q_add_anatop_thermal_imx(1, &wand_thermal);
	imx6q_add_pm_imx(0, &wand_pm_data);
	imx6q_add_dvfs_core(&wand_dvfscore_data);
	imx6q_add_busfreq();
}


/****************************************************************************
 *                                                                          
 * Expansion pin header GPIOs
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_external_gpio_pads[] = {
	MX6DL_PAD_EIM_DA11__GPIO_3_11,
	MX6DL_PAD_EIM_D27__GPIO_3_27,
	MX6DL_PAD_EIM_BCLK__GPIO_6_31,
	MX6DL_PAD_ENET_RX_ER__GPIO_1_24,
	MX6DL_PAD_SD3_RST__GPIO_7_8,
	MX6DL_PAD_EIM_D26__GPIO_3_26,
	MX6DL_PAD_EIM_DA8__GPIO_3_8,
	MX6DL_PAD_GPIO_19__GPIO_4_5,
};

/* ------------------------------------------------------------------------ */

static __init void wand_init_external_gpios(void) {
	WAND_SETUP_PADS(wand_external_gpio_pads);

	gpio_request(IMX_GPIO_NR(3, 11), "external_gpio_0");
	gpio_export(IMX_GPIO_NR(3, 11), true);
	gpio_request(IMX_GPIO_NR(3, 27), "external_gpio_1");
	gpio_export(IMX_GPIO_NR(3, 27), true);
	gpio_request(IMX_GPIO_NR(6, 31), "external_gpio_2");
	gpio_export(IMX_GPIO_NR(6, 31), true);
	gpio_request(IMX_GPIO_NR(1, 24), "external_gpio_3");
	gpio_export(IMX_GPIO_NR(1, 24), true);
	gpio_request(IMX_GPIO_NR(7,  8), "external_gpio_4");
	gpio_export(IMX_GPIO_NR(7,  8), true);
	gpio_request(IMX_GPIO_NR(3, 26), "external_gpio_5");
	gpio_export(IMX_GPIO_NR(3, 26), true);
	gpio_request(IMX_GPIO_NR(3, 8), "external_gpio_6");
	gpio_export(IMX_GPIO_NR(3, 8), true);
	gpio_request(IMX_GPIO_NR(4, 5), "external_gpio_7");
	gpio_export(IMX_GPIO_NR(4, 5), true);
}


/****************************************************************************
 *                                                                          
 * SPI - while not used on the Wandboard, the pins are routed out
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_spi_pads[] = {
	MX6DL_PAD_EIM_D16__ECSPI1_SCLK,
	MX6DL_PAD_EIM_D17__ECSPI1_MISO,
	MX6DL_PAD_EIM_D18__ECSPI1_MOSI,
	MX6DL_PAD_EIM_EB2__GPIO_2_30,

	MX6DL_PAD_EIM_CS0__ECSPI2_SCLK,
	MX6DL_PAD_EIM_CS1__ECSPI2_MOSI,
	MX6DL_PAD_EIM_OE__ECSPI2_MISO,
	MX6DL_PAD_EIM_RW__GPIO_2_26,
	MX6DL_PAD_EIM_LBA__GPIO_2_27,
};
/* The choice of using gpios for chipselect is deliberate,
   there can be issues using the dedicated mux modes for cs.
*/

/* ------------------------------------------------------------------------ */

static const int wand_spi1_chipselect[] = { IMX_GPIO_NR(2, 30) };

/* platform device */
static const struct spi_imx_master wand_spi1_data = {
	.chipselect     = wand_spi1_chipselect,
	.num_chipselect = ARRAY_SIZE(wand_spi1_chipselect),
};

/* ------------------------------------------------------------------------ */

static const int wand_spi2_chipselect[] = { IMX_GPIO_NR(2, 26), IMX_GPIO_NR(2, 27) };

static const struct spi_imx_master wand_spi2_data = {
	.chipselect     = wand_spi2_chipselect,
	.num_chipselect = ARRAY_SIZE(wand_spi2_chipselect),
};

/* ------------------------------------------------------------------------ */

static void __init wand_init_spi(void) {
	WAND_SETUP_PADS(wand_spi_pads);
        
	imx6q_add_ecspi(0, &wand_spi1_data);
	imx6q_add_ecspi(1, &wand_spi2_data);
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
	wand_init_wifi();
	wand_init_bluetooth();
	wand_init_pm();
	wand_init_external_gpios();
	wand_init_spi();
}

/* ------------------------------------------------------------------------ */
        
MACHINE_START(WANDBOARD, "Wandboard")
	.boot_params	= MX6_PHYS_OFFSET + 0x100,
	.map_io		= mx6_map_io,
	.init_irq	= mx6_init_irq,
	.init_machine	= wand_board_init,
	.timer		= &wand_timer,
MACHINE_END

