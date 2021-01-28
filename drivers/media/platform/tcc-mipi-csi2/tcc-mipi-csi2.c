// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <linux/of_graph.h>
#include <video/tcc/vioc_ddicfg.h>

#include "tcc-mipi-csi2-reg.h"
#include "tcc-mipi-cfg-reg.h"
#include "tcc-mipi-ckc-reg.h"
#include "tcc-mipi-csi2.h"

#define LOG_TAG		TCC_MIPI_CSI2_DRIVER_NAME

#define loge(dev_ptr, fmt, ...) \
	dev_err(dev_ptr, "[ERROR][%s] %s - " \
		fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(dev_ptr, fmt, ...) \
	dev_warn(dev_ptr, "[WARN][%s] %s - " \
		fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(dev_ptr, fmt, ...) \
	dev_info(dev_ptr, "[INFO][%s] %s - " fmt, \
		LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(dev_ptr, fmt, ...) \
	dev_dbg(dev_ptr, "[DEBUG][%s] %s - " \
		fmt, LOG_TAG, __func__, ##__VA_ARGS__)

/* To support legacy SVM application */
#define DEFAULT_WIDTH		(1280 * 4)
#define DEFAULT_HEIGHT		(720)

#define DEFAULT_CSIS_FREQ 300000000UL

struct isp_state {
	struct v4l2_mbus_framefmt fmt;

	unsigned int data_format;

	/* output */
	unsigned int pixel_mode;
};

struct tcc_mipi_csi2_state {
	struct platform_device *pdev;
	struct v4l2_subdev sd;

	void __iomem *csi_base;
	void __iomem *ckc_base;
	void __iomem *cfg_base;
	void __iomem *ddicfg_base;

	int irq;

	struct clk *clock;
	u32 clk_frequency;

	unsigned int mipi_chmux[CSI_CFG_MIPI_CHMUX_MAX];
	unsigned int isp_bypass[CSI_CFG_ISP_BYPASS_MAX];

	unsigned int input_ch_num;
	unsigned int pixel_mode;
	unsigned int interleave_mode;
	unsigned int data_lane_num;
	unsigned int hssettle;

	struct isp_state isp_info[MAX_VC];

	struct mutex lock;
	unsigned int use_cnt;
};

static u32 code_to_csi_dt(u32 mbus_code)
{
	u32 dt = 0;

	switch (mbus_code) {
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		dt = DATA_FORMAT_YUV422_8BIT;
		break;
	case MEDIA_BUS_FMT_RGB565_1X16:
		dt = DATA_FORMAT_RGB565;
		break;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		dt = DATA_FORMAT_RGB666;
		break;
	case MEDIA_BUS_FMT_RBG888_1X24:
	case MEDIA_BUS_FMT_BGR888_1X24:
	case MEDIA_BUS_FMT_GBR888_1X24:
	case MEDIA_BUS_FMT_RGB888_1X24:
		dt = DATA_FORMAT_RGB888;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		/*
		 * internal isp does not support RAW8
		 * so, padding 2 bits is done using ISP_FMT_CFG register
		 */
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		dt = DATA_FORMAT_RAW10;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		dt = DATA_FORMAT_RAW12;
		break;
	default:
		dt = DATA_FORMAT_YUV422_8BIT;
	}

	return dt;
}

struct v4l2_dv_timings tcc_mipi_csi2_dv_timings = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.width = DEFAULT_WIDTH,
		.height = DEFAULT_HEIGHT,
		.interlaced = V4L2_DV_PROGRESSIVE,
		.polarities = 0,/* V4L2_DV_HSYNC_POS_POL */
	},
};

static u32 tcc_mipi_csi2_codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

struct v4l2_mbus_config mipi_csi2_mbus_config = {
	.type	= V4L2_MBUS_PARALLEL,
	.flags	=
		V4L2_MBUS_DATA_ACTIVE_LOW	|
		V4L2_MBUS_PCLK_SAMPLE_FALLING	|
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
		V4L2_MBUS_HSYNC_ACTIVE_LOW	|
		V4L2_MBUS_MASTER,
};
/*
 * Helper fuctions for reflection
 */
static inline struct tcc_mipi_csi2_state *sd_to_state(struct v4l2_subdev *sd)
{
	return v4l2_get_subdevdata(sd);
}


static void MIPI_CSIS_Set_DPHY_B_Control(struct tcc_mipi_csi2_state *state,
		unsigned int high_part,
		unsigned int low_part)
{
	unsigned int val_l = low_part;
	unsigned int val_h = high_part;
	void __iomem *reg_l = state->csi_base + DPHY_BCTRL_L;
	void __iomem *reg_h = state->csi_base + DPHY_BCTRL_H;

	__raw_writel(val_l, reg_l);
	__raw_writel(val_h, reg_h);
}

static void MIPI_CSIS_Set_DPHY_S_Control(struct tcc_mipi_csi2_state *state,
		unsigned int high_part,
		unsigned int low_part)
{
	unsigned int val_l = low_part;
	unsigned int val_h = high_part;
	void __iomem *reg_l = state->csi_base + DPHY_SCTRL_L;
	void __iomem *reg_h = state->csi_base + DPHY_SCTRL_H;

	__raw_writel(val_l, reg_l);
	__raw_writel(val_h, reg_h);
}

static void MIPI_CSIS_Set_DPHY_Common_Control(
		struct tcc_mipi_csi2_state *state,
		unsigned int HSsettle,
		unsigned int S_ClkSettleCtl,
		unsigned int S_BYTE_CLK_enable,
		unsigned int S_DpDn_Swap_Clk,
		unsigned int S_DpDn_Swap_Dat,
		unsigned int Enable_dat,
		unsigned int Enable_clk)
{
	unsigned int val = 0;
	void __iomem *reg = state->csi_base + DPHY_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(DCCTRL_HSSETTLE_MASK |
		DCCTRL_S_CLKSETTLECTL_MASK |
		DCCTRL_S_BYTE_CLK_ENABLE_MASK |
		DCCTRL_S_DPDN_SWAP_CLK_MASK |
		DCCTRL_S_DPDN_SWAP_DAT_MASK |
		DCCTRL_ENABLE_DAT_MASK |
		DCCTRL_ENABLE_CLK_MASK);

	val |= (HSsettle << DCCTRL_HSSETTLE_SHIFT |
		S_ClkSettleCtl << DCCTRL_S_CLKSETTLECTL_SHIFT |
		S_BYTE_CLK_enable << DCCTRL_S_BYTE_CLK_ENABLE_SHIFT |
		S_DpDn_Swap_Clk << DCCTRL_S_DPDN_SWAP_CLK_SHIFT |
		S_DpDn_Swap_Dat << DCCTRL_S_DPDN_SWAP_DAT_SHIFT |
		((1 << Enable_dat) - 1) << DCCTRL_ENABLE_DAT_SHIFT |
		Enable_clk << DCCTRL_ENABLE_CLK_SHIFT);

	__raw_writel(val, reg);
}

static void MIPI_CSIS_Set_ISP_Configuration(struct tcc_mipi_csi2_state *state,
		unsigned int ch,
		unsigned int Pixel_mode,
		unsigned int Parallel,
		unsigned int RGB_SWAP,
		unsigned int DataFormat,
		unsigned int Virtual_channel)
{
	unsigned int val = 0;
	void __iomem *reg = state->csi_base + ISP_CONFIG_CH0;
	unsigned int reg_offset = ISP_CONFIG_CH1 - ISP_CONFIG_CH0;

	val = __raw_readl(reg);

	val &= ~(ICON_PIXEL_MODE_MASK |
		ICON_PARALLEL_MASK |
		ICON_RGB_SWAP_MASK |
		ICON_DATAFORMAT_MASK |
		ICON_VIRTUAL_CHANNEL_MASK);

	val |= (Pixel_mode << ICON_PIXEL_MODE_SHIFT |
		Parallel << ICON_PARALLEL_SHIFT |
		RGB_SWAP << ICON_RGB_SWAP_SHIFT |
		DataFormat << ICON_DATAFORMAT_SHIFT |
		Virtual_channel << ICON_VIRTUAL_CHANNEL_SHIFT);

	__raw_writel(val, (reg + (reg_offset * ch)));
}

static void MIPI_CSIS_Set_ISP_Resolution(struct tcc_mipi_csi2_state *state,
		unsigned int ch,
		unsigned int width,
		unsigned int height)
{
	unsigned int val = 0;
	void __iomem *reg = state->csi_base + ISP_RESOL_CH0;
	unsigned int reg_offset = ISP_RESOL_CH1 - ISP_RESOL_CH0;

	val = __raw_readl(reg);

	val &= ~(IRES_VRESOL_MASK | IRES_HRESOL_MASK);

	val |= (width << IRES_HRESOL_SHIFT | height << IRES_VRESOL_SHIFT);

	__raw_writel(val, (reg + (reg_offset * ch)));
}

static void MIPI_CSIS_Set_CSIS_Common_Control(
		struct tcc_mipi_csi2_state *state,
		unsigned int Update_shadow,
		unsigned int Deskew_level,
		unsigned int Deskew_enable,
		unsigned int Interleave_mode,
		unsigned int Lane_number,
		unsigned int Update_shadow_ctrl,
		unsigned int Sw_reset,
		unsigned int Csi_en)
{
	unsigned int val = 0;
	void __iomem *reg = state->csi_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_UPDATE_SHADOW_MASK |
		CCTRL_DESKEW_LEVEL_MASK |
		CCTRL_DESKEW_ENABLE_MASK |
		CCTRL_INTERLEAVE_MODE_MASK |
		CCTRL_LANE_NUMBER_MASK |
		CCTRL_UPDATE_SHADOW_CTRL_MASK |
		CCTRL_SW_RESET_MASK |
		CCTRL_CSI_EN_MASK);

	val |= ((((1 << Update_shadow) - 1) << CCTRL_UPDATE_SHADOW_SHIFT) |
		(Deskew_level << CCTRL_DESKEW_LEVEL_SHIFT) |
		(Deskew_enable << CCTRL_DESKEW_ENABLE_SHIFT) |
		(Interleave_mode << CCTRL_INTERLEAVE_MODE_SHIFT) |
		((Lane_number - 1) << CCTRL_LANE_NUMBER_SHIFT) |
		(Update_shadow_ctrl << CCTRL_UPDATE_SHADOW_CTRL_SHIFT) |
		(Sw_reset << CCTRL_SW_RESET_SHIFT) |
		(Csi_en << CCTRL_CSI_EN_SHIFT));

	__raw_writel(val, reg);
}

static void MIPI_CSIS_Set_CSIS_Reset(struct tcc_mipi_csi2_state *state,
		unsigned int reset)
{
	unsigned int val = 0, count = 0;
	void __iomem *reg = state->csi_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_SW_RESET_MASK);

	if (reset)
		val |= (1 << CCTRL_SW_RESET_SHIFT);

	__raw_writel(val, reg);

	while (__raw_readl(reg) & CCTRL_SW_RESET_MASK) {
		if (count > 50) {
			loge(&(state->pdev->dev), "fail - MIPI_CSI2 reset\n");
			break;
		}
		mdelay(1);
		count++;
	}
}

static void MIPI_CSIS_Set_Enable(struct tcc_mipi_csi2_state *state,
		unsigned int enable)
{
	unsigned int val = 0;
	void __iomem *reg = state->csi_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_CSI_EN_MASK);

	if (enable)
		val |= (1 << CCTRL_CSI_EN_SHIFT);

	__raw_writel(val, reg);
}

static void MIPI_CSIS_Set_CSIS_Clock_Control(
		struct tcc_mipi_csi2_state *state,
		unsigned int Clkgate_trail,
		unsigned int Clkgate_en)
{
	unsigned int val = 0;
	void __iomem *reg = state->csi_base + CSIS_CLK_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_CLKGATE_TRAIL_MASK |
		CCTRL_CLKGATE_EN_MASK);

	val |= (Clkgate_trail << CCTRL_CLKGATE_TRAIL_SHIFT |
		Clkgate_en << CCTRL_CLKGATE_EN_SHIFT);

	__raw_writel(val, reg);
}

static void MIPI_CSIS_Set_CSIS_Interrupt_Mask(
		struct tcc_mipi_csi2_state *state,
		unsigned int page,
		unsigned int mask,
		unsigned int set)
{
	unsigned int val = 0;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0)
		reg = state->csi_base + CSIS_INT_MSK0;
	else
		reg = state->csi_base + CSIS_INT_MSK1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = (__raw_readl(reg) & ~(mask));

	if (set == 0) /* Interrupt enable*/
		val |= mask;

	__raw_writel(val, reg);
}

static unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Mask(
		struct tcc_mipi_csi2_state *state,
		unsigned int page)
{
	unsigned int val = 0;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0)
		reg = state->csi_base + CSIS_INT_MSK0;
	else
		reg = state->csi_base + CSIS_INT_MSK1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = __raw_readl(reg);

	return val;
}

static void MIPI_CSIS_Set_CSIS_Interrupt_Src(
		struct tcc_mipi_csi2_state *state,
		unsigned int page, unsigned int mask)
{
	unsigned int val = 0;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0)
		reg = state->csi_base + CSIS_INT_SRC0;
	else
		reg = state->csi_base + CSIS_INT_SRC1;

	val = __raw_readl(reg);

	val |= mask;

	__raw_writel(val, reg);
}

static unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Src(
		struct tcc_mipi_csi2_state *state,
		unsigned int page)
{
	unsigned int val = 0;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0)
		reg = state->csi_base + CSIS_INT_SRC0;
	else
		reg = state->csi_base + CSIS_INT_SRC1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = __raw_readl(reg);

	return val;
}

#if defined(CONFIG_ARCH_TCC805X)
static void MIPI_WRAP_Set_PLL_Reset(
		struct tcc_mipi_csi2_state *state,
		unsigned int onOff)
{
	unsigned int val = 0;
	void __iomem *reg = 0;

	reg = state->ckc_base + PLLPMS;

	val = __raw_readl(reg);

	if (onOff)
		val &= ~(PLLPMS_RESETB_MASK);
	else
		val |= PLLPMS_RESETB_MASK;

	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_PLL_DIV(
		struct tcc_mipi_csi2_state *state,
		unsigned int onOff, unsigned int pdiv)
{
	unsigned int val = 0;
	void __iomem *reg = 0;

	reg = state->ckc_base + CLKDIVC;

	val = __raw_readl(reg);

	val &= ~(CLKDIVC_PE_MASK | CLKDIVC_PDIV_MASK);
	val |= (((onOff) << CLKDIVC_PE_SHIFT) |
		((pdiv) << CLKDIVC_PDIV_SHIFT));

	__raw_writel(val, reg);
}

static int MIPI_WRAP_Set_PLL_PMS(
		struct tcc_mipi_csi2_state *state,
		unsigned int p, unsigned int m, unsigned int s)
{
	unsigned int val = 0;
	void __iomem *reg = 0;

	reg = state->ckc_base + PLLPMS;

	val = __raw_readl(reg);

	val &= ~(PLLPMS_RESETB_MASK |
		PLLPMS_S_MASK |
		PLLPMS_M_MASK |
		PLLPMS_P_MASK);

	val |= (((p) << PLLPMS_P_SHIFT) |
		((m) << PLLPMS_M_SHIFT) |
		((s) << PLLPMS_S_SHIFT));

	__raw_writel(val, reg);

	usleep_range(1000, 2000);

	val |= ((1) << PLLPMS_RESETB_SHIFT);

	__raw_writel(val, reg);

	usleep_range(1000, 2000);

	val = __raw_readl(reg);

	if (val & PLLPMS_LOCK_MASK)
		return 1;
	else
		return -1;
}

static unsigned int MIPI_WRAP_Set_CKC(struct tcc_mipi_csi2_state *state)
{
	unsigned int ret = 0;
	unsigned int val = 0;
	void __iomem *reg = 0;
	int pll_div = 5, pll_p = 2, pll_m = 200, pll_s = 2;
	int sel_pclk = CLKCTRL_SEL_PLL_DIRECT;
	int sel_bclk = CLKCTRL_SEL_PLL_DIVIDER;

	/*
	 * set source clock to XIN
	 * (for core reset case(SD805XA1-365))
	 */
	reg = state->ckc_base + CLKCTRL0;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_SEL_MASK);
	val |= (CLKCTRL_SEL_XIN << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

	val = 0;
	reg = state->ckc_base + CLKCTRL1;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_SEL_MASK);
	val |= (CLKCTRL_SEL_XIN << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

	/*
	 * XIN is 24Mhz
	 * set pixel clock 600MHz
	 * set bus clock 100MHz
	 */
	MIPI_WRAP_Set_PLL_Reset(state, 1);

	MIPI_WRAP_Set_PLL_DIV(state, ON, pll_div);
	ret = MIPI_WRAP_Set_PLL_PMS(state, pll_p, pll_m, pll_s);

	logi(&(state->pdev->dev), "DIV(%d), PMS(%d %d %d)\n",
		pll_div, pll_p, pll_m, pll_s);

	if (ret < 0) {
		loge(&(state->pdev->dev), "FAIL - MIPI WRAP PLL SETTING\n");
		goto ERR;
	}

	/*
	 * set source clock to PLL
	 */
	logi(&(state->pdev->dev), "BUSCLK SRC(%d), PIXELCLK SRC(%d)\n",
		sel_bclk, sel_pclk);

	reg = state->ckc_base + CLKCTRL0;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_CHGRQ_MASK | CLKCTRL_SEL_MASK);
	val |= (sel_bclk << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

	reg = state->ckc_base + CLKCTRL1;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_CHGRQ_MASK | CLKCTRL_SEL_MASK);
	val |= (sel_pclk << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

ERR:
	return ret;
}

static void MIPI_WRAP_Set_Reset_DPHY(struct tcc_mipi_csi2_state *state,
		unsigned int reset)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + CSI0_CFG + (state->pdev->id * 0x4);

	val = (__raw_readl(reg) & ~(CSI_CFG_S_RESETN_MASK));

	// 0x1 : release, 0x0 : reset
	if (!reset)
		val |= (0x1 << CSI_CFG_S_RESETN_SHIFT);
	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_Reset_GEN(struct tcc_mipi_csi2_state *state,
		unsigned int reset)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + CSI0_CFG + (state->pdev->id * 0x4);

	val = (__raw_readl(reg) &
		~(CSI_CFG_GEN_PX_RST_MASK | CSI_CFG_GEN_APB_RST_MASK));
	if (reset) {
		val |= ((0x1 << CSI_CFG_GEN_PX_RST_SHIFT) |
			(0x1 << CSI_CFG_GEN_APB_RST_SHIFT));
	}

	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_VSync_Polarity(struct tcc_mipi_csi2_state *state,
	unsigned int ch,
	unsigned int pol)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + CSI0_CFG + (state->pdev->id * 0x4);

	val = (__raw_readl(reg) & ~(CSI_CFG_VSYNC_INV0_MASK << ch));

	val |= (pol << (CSI_CFG_VSYNC_INV0_SHIFT + ch));

	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_Output_Mux(struct tcc_mipi_csi2_state *state,
		unsigned int mux, unsigned int sel)
{
	unsigned int val, csi = 0;
	void __iomem *reg = 0;

	csi = ((mux > 3) ? (1) : (0));
	mux %= 4;

	reg = state->cfg_base + CSI0_CFG + (csi * 0x4);

	val = (__raw_readl(reg) & ~(CSI_CFG_MIPI_CHMUX_0_MASK << mux));
	val |= (sel << (CSI_CFG_MIPI_CHMUX_0_SHIFT + mux));
	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_ISP_BYPASS(struct tcc_mipi_csi2_state *state,
	unsigned int ch, unsigned int bypass)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + ISP_BYPASS;

	val = (__raw_readl(reg) & ~(ISP_BYPASS_ISP0_BYP_MASK << ch));
	val |= (bypass << (ISP_BYPASS_ISP0_BYP_SHIFT + ch));
	__raw_writel(val, reg);

}

static void MIPI_WRAP_Set_MIPI_Output_RAW12(struct tcc_mipi_csi2_state *state,
	unsigned int ch, unsigned int fmt)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + ISP_FMT_CFG;

	val = (__raw_readl(reg) & ~(ISP_FMT_CFG_ISP0_FMT_MASK << (ch * 4)));
	val |= (fmt << (ISP_FMT_CFG_ISP0_FMT_SHIFT + (ch * 4)));
	__raw_writel(val, reg);

}

static void MIPI_WRAP_Set_ISP_APBADDR(struct tcc_mipi_csi2_state *state)
{
	void __iomem *reg = 0;

	/* TCS: CV8050C-658 */
	reg = state->cfg_base + ISP_APBADDR_CFG0;
	__raw_writel(0x16371637, reg);
	reg = state->cfg_base + ISP_APBADDR_CFG1;
	__raw_writel(0x16371637, reg);
}
#endif

static int tcc_mipi_csi2_set_interface(struct tcc_mipi_csi2_state *state,
		unsigned int onOff)
{
	unsigned int idx = 0;
	struct isp_state *info;

	if (onOff) {
#if defined(CONFIG_ARCH_TCC803X)
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(state->ddicfg_base, 0);

		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(state->ddicfg_base, 0);
#elif defined CONFIG_ARCH_TCC805X
		MIPI_WRAP_Set_Reset_DPHY(state, OFF);
		MIPI_WRAP_Set_Reset_GEN(state, OFF);

		for (idx = 0; idx < CSI_CFG_MIPI_CHMUX_MAX; idx++) {
			MIPI_WRAP_Set_Output_Mux(state,
				idx, state->mipi_chmux[idx]);
		}

		for (idx = 0; idx < CSI_CFG_ISP_BYPASS_MAX; idx++) {
			MIPI_WRAP_Set_ISP_BYPASS(state,
				idx, state->isp_bypass[idx]);
		}
		MIPI_WRAP_Set_ISP_APBADDR(state);
#endif
		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(state, ON);

		/*
		 * Set D-PHY control(Master, Slave)
		 * Refer to 7.2.3(B_DPHYCTL) in D-PHY datasheet
		 * 500 means ULPS EXIT counter value.
		 */
		MIPI_CSIS_Set_DPHY_B_Control(state, 0x00000000, 0x000001f4);

		/*
		 * Set D-PHY control(Slave)
		 * Refer to 7.2.5(S_DPHYCTL) in D-PHY datasheet
		 */
		MIPI_CSIS_Set_DPHY_S_Control(state, 0x92, 0xfd008000);

		/*
		 * Set D-PHY Common control
		 */
		logi(&(state->pdev->dev), "hssettle value : 0x%x\n",
			state->hssettle);

		MIPI_CSIS_Set_DPHY_Common_Control(state,
			state->hssettle,
			OFF,
			ON,
			OFF,
			OFF,
			state->data_lane_num,
			ON);

		for (idx = 0; idx < state->input_ch_num ; idx++) {
			info = &state->isp_info[idx];
			MIPI_CSIS_Set_ISP_Configuration(state,
				idx,
				info->pixel_mode,
				OFF,
				OFF,
				info->data_format,
				idx);
			MIPI_CSIS_Set_ISP_Resolution(state,
				idx,
				info->fmt.width,
				info->fmt.height);
#ifdef CONFIG_ARCH_TCC805X
			if ((info->data_format >= DATA_FORMAT_RAW10) &&
			    (info->data_format <= DATA_FORMAT_RAW12)) {
				MIPI_WRAP_Set_VSync_Polarity(state, idx, 1);
				MIPI_WRAP_Set_MIPI_Output_RAW12(state, idx, 3);
			}

			if ((info->data_format) == DATA_FORMAT_RAW8)
				MIPI_WRAP_Set_MIPI_Output_RAW12(state, idx, 1);
#endif
		}

		MIPI_CSIS_Set_CSIS_Clock_Control(state, 0x0, 0xf);

		MIPI_CSIS_Set_CSIS_Common_Control(state,
			state->input_ch_num,
			0x0,
			ON,
			state->interleave_mode,
			state->data_lane_num,
			OFF,
			OFF,
			ON);
	} else {
		MIPI_CSIS_Set_Enable(state, OFF);

		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(state, ON);

#if defined(CONFIG_ARCH_TCC803X)
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(state->ddicfg_base, 1);

		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(state->ddicfg_base, 1);
#endif
	}

	return 0;
}

static int tcc_mipi_csi2_set_interrupt(struct tcc_mipi_csi2_state *state,
		unsigned int onOff)
{
	if (onOff) {
		/*
		 * clear interrupt
		 */
		MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 0,
			CIM_MSK_FrameStart_MASK | CIM_MSK_FrameEnd_MASK |
			CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK |
			CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK |
			CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK |
			CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK);

		MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 1,
			CIM_MSK_LINE_END_MASK);

		/*
		 * unmask interrupt
		 */
		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 0,
			CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK |
			CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK |
			CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK |
			CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK,
			0);
		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 1,
			CIM_MSK_LINE_END_MASK,
			0);
	} else {
		/*
		 * mask interrupt
		 */
		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 0,
			CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK |
			CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK |
			CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK |
			CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK,
			1);

		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 1,
			CIM_MSK_LINE_END_MASK,
			1);
	}

	return 0;
}

static irqreturn_t tcc_mipi_csi2_irq_handler(int irq, void *client_data)
{
	struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *) client_data;
	unsigned int intr_status0 = 0, intr_status1 = 0,
		     intr_mask0 = 0, intr_mask1 = 0;
	unsigned int idx = 0;

	intr_status0 = MIPI_CSIS_Get_CSIS_Interrupt_Src(state, 0);
	intr_status1 = MIPI_CSIS_Get_CSIS_Interrupt_Src(state, 1);

	intr_mask0 = MIPI_CSIS_Get_CSIS_Interrupt_Mask(state, 0);
	intr_mask1 = MIPI_CSIS_Get_CSIS_Interrupt_Mask(state, 1);

	intr_status0 &= intr_mask0;
	intr_status1 &= intr_mask1;

	/* interruptsource register 0 */
	if (intr_status0 & CIM_MSK_FrameStart_MASK) {
		for (idx = 0; idx < 4; idx++) {
			if (intr_status0 &
			  ((1 << idx) << CIM_MSK_FrameStart_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]FrameStart packet is received\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_FrameEnd_MASK) {
		for (idx = 0; idx < 4; idx++) {
			if (intr_status0 &
			  ((1 << idx) << CIM_MSK_FrameEnd_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]FrameEnd packet is received\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_SOT_HS_MASK) {
		for (idx = 0; idx < 4; idx++) {
			if (intr_status0 &
			  ((1 << idx) << CIM_MSK_ERR_SOT_HS_SHIFT))
				loge(&(state->pdev->dev),
					"[Lane%d]Start of transmission error\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_LOST_FS_MASK) {
		for (idx = 0; idx < 4; idx++) {
			if (intr_status0 &
			  ((1 << idx) << CIM_MSK_ERR_LOST_FS_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]Lost of Frame Start packet\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_LOST_FE_MASK) {
		for (idx = 0; idx < 4; idx++) {
			if (intr_status0 &
			  ((1 << idx) << CIM_MSK_ERR_LOST_FE_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]Lost of Frame End packet\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_OVER_MASK)
		loge(&(state->pdev->dev), "Image FIFO overflow interrupt\n");

	if (intr_status0 & CIM_MSK_ERR_WRONG_CFG_MASK)
		loge(&(state->pdev->dev), "Wrong configuration\n");

	if (intr_status0 & CIM_MSK_ERR_ECC_MASK)
		loge(&(state->pdev->dev), "ECC error\n");

	if (intr_status0 & CIM_MSK_ERR_CRC_MASK)
		loge(&(state->pdev->dev), "CRC error\n");

	if (intr_status0 & CIM_MSK_ERR_ID_MASK)
		loge(&(state->pdev->dev), "Unknown ID error\n");

	/* interruptsource register 1 */
	if (intr_status1 & CIM_MSK_LINE_END_MASK) {
		for (idx = 0; idx < 4; idx++) {
			if (intr_status1 &
			  ((1 << idx) << CIM_MSK_LINE_END_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d] End of specific line\n",
					idx);
		}
	}

	// clear interrupt
	MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 0, intr_status0);
	MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 1, intr_status1);

	return IRQ_HANDLED;
}

static int tcc_mipi_csi2_parse_dt(struct platform_device *pdev,
		struct tcc_mipi_csi2_state *state)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *ep_node = NULL;
	struct v4l2_fwnode_endpoint endpoint;
#if defined(CONFIG_ARCH_TCC803X)
	struct device_node *ddicfg_node;
#endif
	struct device *dev = &pdev->dev;
	struct resource *mem_res;
#if defined(CONFIG_ARCH_TCC805X)
	char prop_name[32] = {0, };
#endif
	int ret = 0, index = 0;

	/*
	 * Get MIPI CSI-2 base address
	 */
	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csi");
	logi(&(state->pdev->dev), "mem_res = %px\n", mem_res);
	state->csi_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR((const void *)state->csi_base))
		return PTR_ERR((const void *)state->csi_base);

	logi(&(state->pdev->dev), "csi base addr is %px\n", state->csi_base);

#if defined(CONFIG_ARCH_TCC805X)
	/*
	 * Get CKC base address
	 */
	state->ckc_base = (void __iomem *)of_iomap(node, 2);
	if (IS_ERR((const void *)state->ckc_base))
		return PTR_ERR((const void *)state->ckc_base);

	logi(&(state->pdev->dev), "ckc base addr is %px\n", state->ckc_base);

	/*
	 * Get CFG base address
	 */
	state->cfg_base = (void __iomem *)of_iomap(node, 3);
	if (IS_ERR((const void *)state->cfg_base))
		return PTR_ERR((const void *)state->cfg_base);

	logi(&(state->pdev->dev), "cfg base addr is %px\n", state->cfg_base);

	/*
	 * Get mipi_chmux_X selection
	 */
	for (index = 0; index < CSI_CFG_MIPI_CHMUX_MAX; index++) {
		sprintf(prop_name, "mipi-chmux-%d", index);

		of_property_read_u32_index(node,
			prop_name, 0, &(state->mipi_chmux[index]));
	}

	/*
	 * Get ISPX_BYP selection
	 */
	for (index = 0; index < CSI_CFG_ISP_BYPASS_MAX; index++) {
		sprintf(prop_name, "isp%d-bypass", index);

		of_property_read_u32_index(node,
			prop_name, 0, &(state->isp_bypass[index]));
	}
#endif
	/*
	 * get interrupt number
	 */
	state->irq = platform_get_irq(pdev, 0);
	if (state->irq < 0) {
		loge(&(state->pdev->dev), "fail - get irq\n");
		ret = -ENODEV;
		goto err;
	}

	logi(&(state->pdev->dev), "csi irq number is %d\n", state->irq);

#if defined(CONFIG_ARCH_TCC803X)
	// ddi config
	ddicfg_node =
		of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
	if (ddicfg_node == NULL) {
		loge(&(state->pdev->dev), "cann't find DDI Config node\n");
		ret = -ENODEV;
		goto err;
	} else {
		state->ddicfg_base =
			(void __iomem *)of_iomap(ddicfg_node, 0);
		logi(&(state->pdev->dev), "ddicfg addr: %p\n",
			state->ddicfg_base);
	}
#endif
	/*
	 * First endpoint is input port
	 */
	ep_node = of_graph_get_next_endpoint(node, NULL);
	if (!ep_node) {
		loge(&(state->pdev->dev), "No port node at %pOF\n", node->name);
		return -EINVAL;
	}

	/* Get input MIPI CSI2 bus info */
	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep_node),
					&endpoint);
	if (ret)
		goto err;

	state->data_lane_num = endpoint.bus.mipi_csi2.num_data_lanes;

	/* Get additional bus info  */
	of_property_read_u32(ep_node, "num-channel",
					&state->input_ch_num);
	of_property_read_u32(ep_node, "interleave-mode",
					&state->interleave_mode);
	of_property_read_u32(ep_node, "hs-settle",
					&state->hssettle);

	logi(&(state->pdev->dev),
		"data_lane_num %d\n", state->data_lane_num);
	logi(&(state->pdev->dev),
		"input_ch_num %d\n", state->input_ch_num);
	logi(&(state->pdev->dev),
		"interleave_mode %d\n", state->interleave_mode);
	logi(&(state->pdev->dev),
		"hs-settle %d\n", state->hssettle);

	ep_node = of_graph_get_next_endpoint(node, ep_node);
	for (index = 0; ep_node != NULL;
		ep_node = of_graph_get_next_endpoint(node, ep_node), index++) {
		of_property_read_u32(ep_node, "pixel-mode",
					&state->isp_info[index].pixel_mode);
		logi(&(state->pdev->dev), "ch %d pixel_mode is %d\n",
			index, state->isp_info[index].pixel_mode);
	}


err:
	of_node_put(node);

	return ret;
}

#if defined(CONFIG_ARCH_TCC803X)
static void tcc_mipi_csi2_clk_put(struct tcc_mipi_csi2_state *state)
{
	if (!IS_ERR(state->clock)) {
		clk_unprepare(state->clock);
		clk_put(state->clock);
		state->clock = ERR_PTR(-EINVAL);
	}
}

static int tcc_mipi_csi2_clk_get(struct tcc_mipi_csi2_state *state)
{
	int ret;
	struct device *dev = &state->pdev->dev;

	struct device_node *node = dev->of_node;

	if (of_property_read_u32(node,
		"clock-frequency",
		&state->clk_frequency)) {
		state->clk_frequency = DEFAULT_CSIS_FREQ;
		goto err;
	}

	state->clock = clk_get(dev, "mipi-csi-clk");
	if (IS_ERR(state->clock)) {
		loge(&(state->pdev->dev), "Failed to clk_get\n");
		ret = PTR_ERR(state->clock);
		goto err;
	}

	ret = clk_prepare(state->clock);
	if (ret < 0) {
		loge(&(state->pdev->dev), "Failed to clk_prepare\n");

		clk_put(state->clock);
		state->clock = ERR_PTR(-EINVAL);
		goto err;
	}

	return ret;

err:
	loge(&(state->pdev->dev), "fail - get clock\n");
	return ret;
}
#endif

static void tcc_mipi_csi2_init_format(struct tcc_mipi_csi2_state *state)
{
	int i = 0;

	for (i = 0; i < MAX_VC; i++) {
		state->isp_info[i].fmt.width = DEFAULT_WIDTH;
		state->isp_info[i].fmt.height = DEFAULT_HEIGHT;
		state->isp_info[i].fmt.code = MEDIA_BUS_FMT_YUYV8_1X16;
		state->isp_info[i].fmt.field = V4L2_FIELD_NONE;
		state->isp_info[i].fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
		state->isp_info[i].data_format =
			code_to_csi_dt(state->isp_info[i].fmt.code);
	}
}

int tcc_mipi_csi2_enable(struct tcc_mipi_csi2_state *state, unsigned int enable)
{
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

	ret = tcc_mipi_csi2_set_interface(state, enable);
	if (ret < 0) {
		loge(&(state->pdev->dev),
			"fail - tcc_mipi_csi2_set_interface\n");
		goto err;
	}

	ret = tcc_mipi_csi2_set_interrupt(state, enable);
	if (ret < 0) {
		loge(&(state->pdev->dev),
			"fail - tcc_mipi_csi2_set_interrupt\n");
		goto err;
	}

err:
	logi(&(state->pdev->dev), "%s out\n", __func__);
	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_mipi_csi2_s_power(struct v4l2_subdev *sd, int on)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	logi(&(state->pdev->dev), "call\n");

	return ret;
}
static int tcc_mipi_csi2_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	logi(&(state->pdev->dev), "call\n");

	mutex_lock(&state->lock);

	if ((state->use_cnt == 0) && (enable == 1))
		tcc_mipi_csi2_enable(state, enable);
	else if ((state->use_cnt == 1) && (enable == 0))
		tcc_mipi_csi2_enable(state, enable);

	if (enable)
		state->use_cnt++;
	else
		state->use_cnt--;

	logi(&(state->pdev->dev), "use_cnt is %d\n", state->use_cnt);

	mutex_unlock(&state->lock);


	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_mipi_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	return ret;
}

static int tcc_mipi_csi2_g_mbus_config(struct v4l2_subdev *sd,
				       struct v4l2_mbus_config *cfg)
{
	memcpy((void *)cfg, (const void *)&mipi_csi2_mbus_config, sizeof(*cfg));

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int tcc_mipi_csi2_enum_mbus_code(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_mbus_code_enum *code)
{
	if ((code->pad != 0) ||
	    (ARRAY_SIZE(tcc_mipi_csi2_codes) <= code->index))
		return -EINVAL;

	code->code = tcc_mipi_csi2_codes[code->index];

	return 0;
}

static int tcc_mipi_csi2_g_dv_timings(struct v4l2_subdev *sd,
				      struct v4l2_dv_timings *timings)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	logi(&(state->pdev->dev), "%s call\n", __func__);

	mutex_lock(&state->lock);

	memcpy((void *)timings,
		(const void *)&tcc_mipi_csi2_dv_timings,
		sizeof(*timings));

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_get_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *format)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int ret	= 0;
	int i = 0;

	logi(&(state->pdev->dev), "call\n");

	mutex_lock(&state->lock);

	for (i = 0; i < MAX_VC; i++) {
		memcpy((void *)&format->format,
			(const void *)&state->isp_info[0].fmt,
			sizeof(struct v4l2_mbus_framefmt));
	}
	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *format)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int ret	= 0;
	int i = 0;

	logi(&(state->pdev->dev), "call\n");

	mutex_lock(&state->lock);

	for (i = 0; i < MAX_VC; i++) {
		memcpy((void *)&state->isp_info[i].fmt,
			(const void *)&format->format,
			sizeof(struct v4l2_mbus_framefmt));

		state->isp_info[i].data_format =
			code_to_csi_dt(state->isp_info[i].fmt.code);
	}

	tcc_mipi_csi2_dv_timings.bt.width = format->format.width;
	tcc_mipi_csi2_dv_timings.bt.height = format->format.height;

	mutex_unlock(&state->lock);

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops tcc_mipi_csi2_core_ops = {
	.s_power		= tcc_mipi_csi2_s_power,
	.init			= tcc_mipi_csi2_init,
};

static const struct v4l2_subdev_pad_ops tcc_mipi_csi2_pad_ops = {
	.enum_mbus_code		= tcc_mipi_csi2_enum_mbus_code,
	.get_fmt		= tcc_mipi_csi2_get_fmt,
	.set_fmt		= tcc_mipi_csi2_set_fmt,
};

static const struct v4l2_subdev_video_ops tcc_mipi_csi2_video_ops = {
	.s_stream		= tcc_mipi_csi2_s_stream,
	.g_mbus_config		= tcc_mipi_csi2_g_mbus_config,
	.g_dv_timings		= tcc_mipi_csi2_g_dv_timings,
};

static const struct v4l2_subdev_ops tcc_mipi_csi2_ops = {
	.core			= &tcc_mipi_csi2_core_ops,
	.video			= &tcc_mipi_csi2_video_ops,
	.pad			= &tcc_mipi_csi2_pad_ops,
};

static const struct of_device_id tcc_mipi_csi2_of_match[];

static int tcc_mipi_csi2_probe(struct platform_device *pdev)
{
	struct tcc_mipi_csi2_state *state;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	int ret = 0;

	logi(&(pdev->dev), "%s in\n", __func__);

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (WARN_ON(state == NULL)) {
		ret = -ENOMEM;
		goto err;
	}

	mutex_init(&state->lock);
	platform_set_drvdata(pdev, state);

	of_id = of_match_node(tcc_mipi_csi2_of_match, dev->of_node);
	if (WARN_ON(of_id == NULL)) {
		ret = -EINVAL;
		goto err;
	}

	pdev->id = of_alias_get_id(pdev->dev.of_node, "mipi-csi2-");
	state->pdev = pdev;

	/*
	 * Parse device tree
	 *
	 * In the case of 803x platform, ddicfg base is needed to reset DPHY.
	 */
	ret = tcc_mipi_csi2_parse_dt(pdev, state);
	if (ret < 0)
		goto err;

	/*
	 * set clock
	 */
#if defined(CONFIG_ARCH_TCC803X)
	ret = tcc_mipi_csi2_clk_get(state);
	if (ret < 0)
		goto err;

	ret = clk_set_rate(state->clock, state->clk_frequency);
	if (ret < 0)
		goto e_clkput;

	ret = clk_enable(state->clock);
	if (ret < 0)
		goto e_clkput;

	logi(&(state->pdev->dev), "csi clock is %d Hz\n", state->clk_frequency);
#else
	if (!(MIPI_WRAP_Set_CKC(state))) {
		loge(&(state->pdev->dev), "fail - mipi wrap clock setting\n");
		ret = -ENODEV;
		goto err;
	}
#endif
	/*
	 * request irq
	 */
	ret = devm_request_irq(dev, state->irq, tcc_mipi_csi2_irq_handler,
			0, dev_name(dev), state);
	if (ret) {
		loge(&(state->pdev->dev), "fail - Interrupt request\n");
		goto e_clkdis;
	}

	v4l2_subdev_init(&(state->sd), &tcc_mipi_csi2_ops);
	state->sd.owner = pdev->dev.driver->owner;
	state->sd.dev = &pdev->dev;
	v4l2_set_subdevdata(&(state->sd), state);
	/* initialize name */
	sprintf(state->sd.name, "mipi_csi2_%d", pdev->id);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&(state->sd));
	if (ret) {
		loge(&(state->pdev->dev),
				"Failed to register subdevice\n");
		goto e_clkdis;
	}
	logi(&(state->pdev->dev),
		"%s is registered as a v4l2 sub device.\n",
		state->sd.name);

	tcc_mipi_csi2_init_format(state);

#ifdef CONFIG_VIDEO_TCCVIN
	/* To support legacy SVM application */
	tcc_mipi_csi2_init(&(state->sd), ON);
#endif
	logi(&(state->pdev->dev), "Success proving MIPI-CSI2-%d\n", pdev->id);

	goto end;

e_clkdis:
#if defined(CONFIG_ARCH_TCC803X)
	clk_disable(state->clock);
e_clkput:
	tcc_mipi_csi2_clk_put(state);
#endif
err:
end:
	logi(&(state->pdev->dev), "%s out\n", __func__);

	return ret;
}

static int tcc_mipi_csi2_remove(struct platform_device *pdev)
{
	struct tcc_mipi_csi2_state *state = platform_get_drvdata(pdev);

	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

#ifdef CONFIG_VIDEO_TCCVIN
	/* To support legacy SVM application */
	tcc_mipi_csi2_init(&(state->sd), OFF);
#endif

#if defined(CONFIG_ARCH_TCC803X)
	clk_disable(state->clock);
	tcc_mipi_csi2_clk_put(state);
#else

#endif
	logi(&(state->pdev->dev), "%s out\n", __func__);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_mipi_csi2_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tcc_mipi_csi2_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

#if defined(CONFIG_ARCH_TCC803X)
	clk_disable(state->clock);
	tcc_mipi_csi2_clk_put(state);
#else

#endif

	return ret;
}

static int tcc_mipi_csi2_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tcc_mipi_csi2_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

#if defined(CONFIG_ARCH_TCC803X)
	ret = tcc_mipi_csi2_clk_get(state);
	if (ret < 0)
		goto err;

	ret = clk_set_rate(state->clock, state->clk_frequency);
	if (ret < 0)
		goto e_clkput;

	ret = clk_enable(state->clock);
	if (ret < 0)
		goto e_clkput;

	logi(&(state->pdev->dev), "csi clock is %d Hz\n", state->clk_frequency);
#else
	if (!(MIPI_WRAP_Set_CKC(state))) {
		loge(&(state->pdev->dev), "fail - mipi wrap clock setting\n");
		ret = -ENODEV;
		goto err;
	}
#endif

	return ret;

e_clkdis:
#if defined(CONFIG_ARCH_TCC803X)
	clk_disable(state->clock);
e_clkput:
	tcc_mipi_csi2_clk_put(state);
#endif
err:
	return ret;
}
#endif

static const struct dev_pm_ops tcc_mipi_csi2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_mipi_csi2_suspend, tcc_mipi_csi2_resume)
};

static const struct of_device_id tcc_mipi_csi2_of_match[] = {
	{
		.compatible	= "telechips,tcc803x-mipi-csi2",
	},
	{
		.compatible	= "telechips,tcc805x-mipi-csi2",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tcc_mipi_csi2_of_match);

static struct platform_driver tcc_mipi_csi2_driver = {
	.probe = tcc_mipi_csi2_probe,
	.remove = tcc_mipi_csi2_remove,
	.driver = {
		.name = TCC_MIPI_CSI2_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= tcc_mipi_csi2_of_match,
		.pm = &tcc_mipi_csi2_pm_ops,
	},
};
module_platform_driver(tcc_mipi_csi2_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC MIPI-CSI2 receiver driver");
MODULE_LICENSE("GPL");
