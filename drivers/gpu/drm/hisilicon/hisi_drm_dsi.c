/*
 * Hisilicon Terminal SoCs drm driver
 *
 * Copyright (c) 2014-2015 Hisilicon Limited.
 * Author: Xinwei Kong <kong.kongxinwei@hisilicon.com> for hisilicon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <video/videomode.h>
#include <linux/of_graph.h>

#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_encoder_slave.h>
#include <drm/drm_atomic_helper.h>

#include "hisi_dsi_reg.h"

#define MAX_TX_ESC_CLK		   (10)
#define DSI_24BITS_1               (5)
#define DSI_NON_BURST_SYNC_PULSES  (0)
#define DSI_BURST_MODE    DSI_NON_BURST_SYNC_PULSES

#define ROUND(x, y) ((x) / (y) + ((x) % (y) * 10 / (y) >= 5 ? 1 : 0))

#define DEFAULT_MIPI_CLK_RATE   19200000
#define DEFAULT_MIPI_CLK_PERIOD_PS (1000000000 / (DEFAULT_MIPI_CLK_RATE / 1000))

#define R(x) ((u32)((((u64)(x) * (u64)1000 * (u64)dsi->vm.pixelclock) / \
	      phy->lane_byte_clk_kHz)))

#define to_hisi_dsi_encoder(encoder) \
	container_of(encoder, struct hisi_dsi, encoder)

#define to_hisi_dsi_host(host) \
	container_of(host, struct hisi_dsi, host)

struct mipi_phy_register {
	u32 clk_t_lpx;
	u32 clk_t_hs_prepare;
	u32 clk_t_hs_zero;
	u32 clk_t_hs_trial;
	u32 clk_t_wakeup;
	u32 data_t_lpx;
	u32 data_t_hs_prepare;
	u32 data_t_hs_zero;
	u32 data_t_hs_trial;
	u32 data_t_ta_go;
	u32 data_t_ta_get;
	u32 data_t_wakeup;
	u32 hstx_ckg_sel;
	u32 pll_fbd_div5f;
	u32 pll_fbd_div1f;
	u32 pll_fbd_2p;
	u32 pll_enbwt;
	u32 pll_fbd_p;
	u32 pll_fbd_s;
	u32 pll_pre_div1p;
	u32 pll_pre_p;
	u32 pll_vco_750M;
	u32 pll_lpf_rs;
	u32 pll_lpf_cs;
	u32 clklp2hs_time;
	u32 clkhs2lp_time;
	u32 lp2hs_time;
	u32 hs2lp_time;
	u32 clk_to_data_delay;
	u32 data_to_clk_delay;
	u32 lane_byte_clk_kHz;
	u32 clk_division;
	u32 burst_mode;
};

struct hisi_dsi {
	struct platform_device *pdev;
	struct device_node *device_node;
	struct drm_encoder encoder;
	struct drm_bridge  bridge;
	struct drm_bridge  ext_bridge;
	struct mipi_dsi_host host;
	struct mipi_phy_register phy;
	struct videomode vm;

	u32 lanes;
	u32 format;
	u32 date_enable_pol;
	u32 mode_flags;
	u8 color_mode;
	void *ctx;

	bool enable;
	bool registered;
};

struct hisi_dsi_context {
	struct hisi_dsi dsi;

	struct clk *dsi_cfg_clk;
	struct drm_device *dev;

	void __iomem *base;
	int nominal_pixel_clk_kHz;
};

struct dsi_phy_seq_info {
	u32 min_range_kHz;
	u32 max_range_kHz;
	u32 pll_vco_750M;
	u32 hstx_ckg_sel;
};

struct dsi_phy_seq_info dphy_seq_info[] = {
	{   46000,    62000,   1,    7 },
	{   62000,    93000,   0,    7 },
	{   93000,   125000,   1,    6 },
	{  125000,   187000,   0,    6 },
	{  187000,   250000,   1,    5 },
	{  250000,   375000,   0,    5 },
	{  375000,   500000,   1,    4 },
	{  500000,   750000,   0,    4 },
	{  750000,  1000000,   1,    0 },
	{ 1000000,  1500000,   0,    0 }
};

void set_dsi_phy_rate_equal_or_faster(u32 *phy_freq_kHz,
				      struct mipi_phy_register *phy)
{
	u32 ui = 0;
	u32 cfg_clk_ps = DEFAULT_MIPI_CLK_PERIOD_PS;
	u32 i = 0;
	u32 q_pll = 1;
	u32 m_pll = 0;
	u32 n_pll = 0;
	u32 r_pll = 1;
	u32 m_n = 0;
	u32 m_n_int = 0;
	u64 f_kHz;
	u64 temp;

	do {
		f_kHz = *phy_freq_kHz;

		/* Find the PLL clock range from the table */
		for (i = 0; i < ARRAY_SIZE(dphy_seq_info); i++)
			if (f_kHz > dphy_seq_info[i].min_range_kHz &&
			    f_kHz <= dphy_seq_info[i].max_range_kHz)
				break;

		if (i == ARRAY_SIZE(dphy_seq_info)) {
			DRM_ERROR("%lldkHz out of range\n", f_kHz);
			return;
		}

		phy->pll_vco_750M = dphy_seq_info[i].pll_vco_750M;
		phy->hstx_ckg_sel = dphy_seq_info[i].hstx_ckg_sel;

		if (phy->hstx_ckg_sel <= 7 &&
		    phy->hstx_ckg_sel >= 4)
			q_pll = 0x10 >> (7 - phy->hstx_ckg_sel);

		temp = f_kHz * (u64)q_pll * (u64)cfg_clk_ps;
		m_n_int = temp / (u64)1000000000;
		m_n = (temp % (u64)1000000000) / (u64)100000000;

		if (m_n_int % 2 == 0) {
			if (m_n * 6 >= 50) {
				n_pll = 2;
				m_pll = (m_n_int + 1) * n_pll;
			} else if (m_n * 6 >= 30) {
				n_pll = 3;
				m_pll = m_n_int * n_pll + 2;
			} else {
				n_pll = 1;
				m_pll = m_n_int * n_pll;
			}
		} else {
			if (m_n * 6 >= 50) {
				n_pll = 1;
				m_pll = (m_n_int + 1) * n_pll;
			} else if (m_n * 6 >= 30) {
				n_pll = 1;
				m_pll = (m_n_int + 1) * n_pll;
			} else if (m_n * 6 >= 10) {
				n_pll = 3;
				m_pll = m_n_int * n_pll + 1;
			} else {
				n_pll = 2;
				m_pll = m_n_int * n_pll;
			}
		}

		if (n_pll == 1) {
			phy->pll_fbd_p = 0;
			phy->pll_pre_div1p = 1;
		} else {
			phy->pll_fbd_p = n_pll;
			phy->pll_pre_div1p = 0;
		}

		if (phy->pll_fbd_2p <= 7 && phy->pll_fbd_2p >= 4)
			r_pll = 0x10 >> (7 - phy->pll_fbd_2p);

		if (m_pll == 2) {
			phy->pll_pre_p = 0;
			phy->pll_fbd_s = 0;
			phy->pll_fbd_div1f = 0;
			phy->pll_fbd_div5f = 1;
		} else if (m_pll >= 2 * 2 * r_pll && m_pll <= 2 * 4 * r_pll) {
			phy->pll_pre_p = m_pll / (2 * r_pll);
			phy->pll_fbd_s = 0;
			phy->pll_fbd_div1f = 1;
			phy->pll_fbd_div5f = 0;
		} else if (m_pll >= 2 * 5 * r_pll && m_pll <= 2 * 150 * r_pll) {
			if (((m_pll / (2 * r_pll)) % 2) == 0) {
				phy->pll_pre_p =
					(m_pll / (2 * r_pll)) / 2 - 1;
				phy->pll_fbd_s =
					(m_pll / (2 * r_pll)) % 2 + 2;
			} else {
				phy->pll_pre_p =
					(m_pll / (2 * r_pll)) / 2;
				phy->pll_fbd_s =
					(m_pll / (2 * r_pll)) % 2;
			}
			phy->pll_fbd_div1f = 0;
			phy->pll_fbd_div5f = 0;
		} else {
			phy->pll_pre_p = 0;
			phy->pll_fbd_s = 0;
			phy->pll_fbd_div1f = 0;
			phy->pll_fbd_div5f = 1;
		}

		f_kHz = (u64)1000000000 * (u64)m_pll /
			((u64)cfg_clk_ps * (u64)n_pll * (u64)q_pll);

		if (f_kHz >= *phy_freq_kHz)
			break;

		(*phy_freq_kHz) += 10;

	} while (1);

	*phy_freq_kHz = f_kHz;
	ui = 1000000 / f_kHz;

	phy->clk_t_lpx = ROUND(50, 8 * ui);
	phy->clk_t_hs_prepare = ROUND(133, 16 * ui) - 1;

	phy->clk_t_hs_zero = ROUND(262, 8 * ui);
	phy->clk_t_hs_trial = 2 * (ROUND(60, 8 * ui) - 1);
	phy->clk_t_wakeup = ROUND(1000000, (cfg_clk_ps / 1000) - 1);
	if (phy->clk_t_wakeup > 0xff)
		phy->clk_t_wakeup = 0xff;
	phy->data_t_wakeup = phy->clk_t_wakeup;
	phy->data_t_lpx = phy->clk_t_lpx;
	phy->data_t_hs_prepare = ROUND(125 + 10 * ui, 16 * ui) - 1;
	phy->data_t_hs_zero = ROUND(105 + 6 * ui, 8 * ui);
	phy->data_t_hs_trial = 2 * (ROUND(60 + 4 * ui, 8 * ui) - 1);
	phy->data_t_ta_go = 3;
	phy->data_t_ta_get = 4;

	phy->pll_enbwt = 1;
	phy->clklp2hs_time = ROUND(407, 8 * ui) + 12;
	phy->clkhs2lp_time = ROUND(105 + 12 * ui, 8 * ui);
	phy->lp2hs_time = ROUND(240 + 12 * ui, 8 * ui) + 1;
	phy->hs2lp_time = phy->clkhs2lp_time;
	phy->clk_to_data_delay = 1 + phy->clklp2hs_time;
	phy->data_to_clk_delay = ROUND(60 + 52 * ui, 8 * ui) +
				phy->clkhs2lp_time;

	phy->lane_byte_clk_kHz = f_kHz / 8;
	phy->clk_division = phy->lane_byte_clk_kHz / MAX_TX_ESC_CLK;
	if (phy->lane_byte_clk_kHz % MAX_TX_ESC_CLK)
		phy->clk_division++;

	phy->burst_mode = DSI_BURST_MODE;
}

int dsi_mipi_init(struct hisi_dsi *dsi)
{
	struct hisi_dsi_context *ctx = dsi->ctx;
	struct mipi_phy_register *phy = &dsi->phy;
	void __iomem *base = ctx->base;

	u32 i = 0;
	u32 hline_time = 0;
	u32 hsa_time = 0;
	u32 hbp_time = 0;
	u32 pixel_clk_kHz;
	u32 delay_count = 0;
	int refresh_nom, refresh_real;
	int htot, vtot, blc_hactive;
	int tmp, tmp1, val;
	bool is_ready = false;

	/* reset Core */
	writel(PWR_UP_OFF, base + PWR_UP);

	/* set lanes value */
	val = (dsi->lanes - 1);
	val |= PHY_STOP_WAIT_TIME << 8;
	writel(val, base + PHY_IF_CFG);

	/* set phy clk division */
	writel(phy->clk_division, base + CLKMGR_CFG);

	/* clean up phy set param */
	writel(0x00, base + PHY_RSTZ);
	writel(0x00, base + PHY_TST_CTRL0);
	writel(0x01, base + PHY_TST_CTRL0);
	writel(0x00, base + PHY_TST_CTRL0);

	/* clock lane Timing control - TLPX */
	dsi_phy_tst_set(base, CLK_LPX_ADDR, phy->clk_t_lpx);

	/* clock lane Timing control - THS-PREPARE */
	dsi_phy_tst_set(base, CLK_HS_PRE_ADDR, phy->clk_t_hs_prepare);

	/* clock lane Timing control - THS-ZERO */
	dsi_phy_tst_set(base, CLK_HS_ZERO_ADDR, phy->clk_t_hs_zero);

	/* clock lane Timing control - THS-TRAIL */
	dsi_phy_tst_set(base, CLK_HS_TRIAL_ADDR, phy->clk_t_hs_trial);

	/* clock lane Timing control - TWAKEUP */
	dsi_phy_tst_set(base, CLK_WAKEUP_ADDR, phy->clk_t_wakeup);

	/* data lane */
	for (i = 0; i < dsi->lanes; i++) {
		/* Timing control - TLPX*/
		dsi_phy_tst_set(base, DATA_LPX_ADDR + (i << 4),
				phy->data_t_lpx);

		/* Timing control - THS-PREPARE */
		dsi_phy_tst_set(base, DATA_HS_PRE_ADDR + (i << 4),
				phy->data_t_hs_prepare);

		/* Timing control - THS-ZERO */
		dsi_phy_tst_set(base, DATA_HS_ZERO_ADDR + (i << 4),
				phy->data_t_hs_zero);

		/* Timing control - THS-TRAIL */
		dsi_phy_tst_set(base, DATA_HS_TRIAL_ADDR + (i << 4),
				phy->data_t_hs_trial);

		/* Timing control - TTA-GO */
		dsi_phy_tst_set(base, DATA_TA_GO_ADDR + (i << 4),
				phy->data_t_ta_go);

		/* Timing control - TTA-GET */
		dsi_phy_tst_set(base, DATA_TA_GET_ADDR + (i << 4),
				phy->data_t_ta_get);

		/*  Timing control - TWAKEUP */
		dsi_phy_tst_set(base, DATA_WAKEUP_ADDR + (i << 4),
				phy->data_t_wakeup);
	}

	/* physical configuration I  */
	dsi_phy_tst_set(base, HSTX_CKG_SEL_ADDR, phy->hstx_ckg_sel);

	/* physical configuration pll II  */
	val = (phy->pll_fbd_div5f << 5) + (phy->pll_fbd_div1f << 4) +
				(phy->pll_fbd_2p << 1) + phy->pll_enbwt;
	dsi_phy_tst_set(base, PLL_FBD_DPN_ADDR, val);

	/* physical configuration pll II  */
	dsi_phy_tst_set(base, PLL_FBD_P_ADDR, phy->pll_fbd_p);

	/* physical configuration pll III  */
	dsi_phy_tst_set(base, PLL_FBD_S_ADDR, phy->pll_fbd_s);

	/*physical configuration pll IV*/
	val = (phy->pll_pre_div1p << 7) + phy->pll_pre_p;
	dsi_phy_tst_set(base, PLL_PRA_DP_ADDR, val);

	/*physical configuration pll V*/
	val = (phy->pll_vco_750M << 4) + (phy->pll_lpf_rs << 2) +
					phy->pll_lpf_cs + BIT(5);
	dsi_phy_tst_set(base, PLL_LPF_VOF_ADDR, val);

	writel(0x04, base + PHY_RSTZ);
	udelay(1);

	writel(0x05, base + PHY_RSTZ);
	udelay(1);

	writel(0x07, base + PHY_RSTZ);
	usleep_range(1000, 1500);

	while (1) {
		val = readl(base +  PHY_STATUS);
		if ((0x01 & val) || delay_count > 100) {
			is_ready = (delay_count < 100) ? true : false;
			delay_count = 0;
			break;
		}

		udelay(1);
		++delay_count;
	}

	if (!is_ready)
		DRM_INFO("phylock is not ready.\n");

	while (1) {
		val = readl(base + PHY_STATUS);
		if ((BIT(2) & val) || delay_count > 100) {
			is_ready = (delay_count < 100) ? true : false;
			break;
		}

		udelay(1);
		++delay_count;
	}

	if (!is_ready)
		DRM_INFO("phystopstateclklane is not ready.\n");

	/* DSI color mode setting */
	writel(0, base + DPI_VCID);
	writel(dsi->color_mode, base + DPI_COLOR_CODING);

	/* DSI format and pol setting */
	val = dsi->date_enable_pol;
	val |= (dsi->vm.flags & DISPLAY_FLAGS_HSYNC_HIGH ? 0 : 1) << 0x2;
	val |= (dsi->vm.flags & DISPLAY_FLAGS_VSYNC_HIGH ? 0 : 1) << 0x1;
	writel(val, base +  DPI_CFG_POL);

	if (dsi->format == MIPI_DSI_FMT_RGB666)
		writel(dsi->color_mode | BIT(9), base + DPI_COLOR_CODING);

	/*
	 * The DSI IP accepts vertical timing using lines as normal,
	 * but horizontal timing is a mixture of pixel-clocks for the
	 * active region and byte-lane clocks for the blanking-related
	 * timings.  hfp is specified as the total hline_time in byte-
	 * lane clocks minus hsa, hbp and active.
	 */

	htot = dsi->vm.hactive + dsi->vm.hsync_len +
		  dsi->vm.hfront_porch + dsi->vm.hback_porch;
	vtot = dsi->vm.vactive + dsi->vm.vsync_len +
		  dsi->vm.vfront_porch + dsi->vm.vback_porch;

	pixel_clk_kHz = dsi->vm.pixelclock;

	hsa_time = (dsi->vm.hsync_len * phy->lane_byte_clk_kHz) /
		   pixel_clk_kHz;
	hbp_time = (dsi->vm.hback_porch * phy->lane_byte_clk_kHz) /
		   pixel_clk_kHz;
	hline_time  = (((u64)htot * (u64)phy->lane_byte_clk_kHz)) /
		      pixel_clk_kHz;
	blc_hactive  = (((u64)dsi->vm.hactive *
			(u64)phy->lane_byte_clk_kHz)) / pixel_clk_kHz;

	if ((R(hline_time) / 1000) > htot)
		hline_time--;

	if ((R(hline_time) / 1000) < htot)
		hline_time++;

	/* all specified in byte-lane clocks */
	writel(hsa_time, base + VID_HSA_TIME);
	writel(hbp_time, base + VID_HBP_TIME);
	writel(hline_time, base + VID_HLINE_TIME);

	if (dsi->vm.vsync_len > 15)
		dsi->vm.vsync_len = 15;

	writel(dsi->vm.vsync_len, base + VID_VSA_LINES);
	writel(dsi->vm.vback_porch, base + VID_VBP_LINES);
	writel(dsi->vm.vfront_porch, base + VID_VFP_LINES);
	writel(dsi->vm.vactive, base + VID_VACTIVE_LINES);
	writel(dsi->vm.hactive, base + VID_PKT_SIZE);

	refresh_nom = ((u64)ctx->nominal_pixel_clk_kHz * 1000000) /
		      (htot * vtot);

	tmp = 1000000000 / dsi->vm.pixelclock;
	tmp1 = 1000000000 / phy->lane_byte_clk_kHz;

	refresh_real = ((u64)dsi->vm.pixelclock * (u64)1000000000) /
		      ((u64)R(hline_time) * (u64)vtot);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		/*
		 * we disable this since it affects downstream
		 * DSI -> HDMI converter output
		 */
		writel(0x00, base + VID_MODE_CFG);

		/*VSA/VBP/VFP max transfer byte in LP mode*/
		writel(0x00, base + DPI_LP_CMD_TIM);
		/* enable LP command transfer */
		writel(0x00, base + VID_MODE_CFG);
		/* config max read time */
		writel(PHY_MAX_TIME, base + PHY_TMR_CFG);
	}

	/* Configure core's phy parameters */
	writel(0xFFF, base + BTA_TO_CNT);

	val = 0xFF;
	val |= (phy->lp2hs_time) << 16;
	val |= (phy->hs2lp_time) << 24;
	writel(val, base + PHY_TMR_CFG);

	val = phy->clklp2hs_time;
	val |= (phy->clkhs2lp_time) << 16;
	writel(val, base + PHY_TMR_LPCLK_CFG);

	/* setting burst mode */
	val = phy->clk_to_data_delay;
	val |= (phy->data_to_clk_delay) << 8;
	writel(val, base + NO_CONTINUE);
	writel(0x00, base + VID_MODE_CFG);
	writel(phy->burst_mode, base + VID_MODE_CFG);
	writel(0x0, base + LPCLK_CTRL);

	/* for dsi read */
	writel(0x0, base + PCKHDL_CFG);

	/* Enable EOTP TX; Enable EDPI */
	if (dsi->mode_flags == MIPI_DSI_MODE_VIDEO)
		writel(dsi->vm.hactive, base + EDPI_CMD_SIZE);

	/* DSI and D-PHY Initialization */
	writel(VIDEO_MODE, base + MODE_CFG);
	writel(0x1, base + LPCLK_CTRL);

	writel(PWR_UP_ON, base + PWR_UP);

	return 0;
}

void hisi_dsi_drm_encoder_disable(struct drm_encoder *encoder)
{
	struct hisi_dsi *dsi = to_hisi_dsi_encoder(encoder);
	struct hisi_dsi_context *ctx = dsi->ctx;
	void __iomem *base = ctx->base;

	DRM_DEBUG_DRIVER("enter\n");
	if (!dsi->enable)
		return;

	writel(PWR_UP_OFF, base + PWR_UP);
	writel(0x00, base + LPCLK_CTRL);
	writel(0x00, base + PHY_RSTZ);
	clk_disable_unprepare(ctx->dsi_cfg_clk);

	dsi->enable = false;
}

void hisi_dsi_drm_encoder_enable(struct drm_encoder *encoder)
{
	struct hisi_dsi *dsi = to_hisi_dsi_encoder(encoder);
	struct hisi_dsi_context *ctx = dsi->ctx;
	int ret;

	/* mipi dphy clock enable */
	ret = clk_prepare_enable(ctx->dsi_cfg_clk);
	if (ret) {
		DRM_ERROR("fail to enable dsi_cfg_clk: %d\n", ret);
		return;
	}

	ret = dsi_mipi_init(dsi);
	if (ret) {
		DRM_ERROR("failed to init mipi: %d!\n", ret);
		return;
	}
}

void hisi_dsi_drm_encoder_mode_set(struct drm_encoder *encoder,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	struct hisi_dsi *dsi = to_hisi_dsi_encoder(encoder);
	struct hisi_dsi_context *ctx = dsi->ctx;
	struct videomode *vm = &dsi->vm;
	u32 dphy_freq_kHz;

	vm->flags = 0;
	vm->hactive = mode->hdisplay;
	vm->vactive = mode->vdisplay;
	vm->vfront_porch = mode->vsync_start - mode->vdisplay;
	vm->vback_porch = mode->vtotal - mode->vsync_end;
	vm->vsync_len = mode->vsync_end - mode->vsync_start;
	vm->hfront_porch = mode->hsync_start - mode->hdisplay;
	vm->hback_porch = mode->htotal - mode->hsync_end;
	vm->hsync_len = mode->hsync_end - mode->hsync_start;

	vm->pixelclock = adjusted_mode->clock;
	ctx->nominal_pixel_clk_kHz = mode->clock;
	dsi->lanes = 3 + !!(vm->pixelclock >= 115000);
	dphy_freq_kHz = vm->pixelclock * 24 / dsi->lanes;

	/* avoid a less-compatible DSI rate with 1.2GHz px PLL */
	if (dphy_freq_kHz == 600000)
		dphy_freq_kHz = 640000;

	set_dsi_phy_rate_equal_or_faster(&dphy_freq_kHz, &dsi->phy);

	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		vm->flags |= DISPLAY_FLAGS_HSYNC_HIGH;
	else if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		vm->flags |= DISPLAY_FLAGS_HSYNC_LOW;
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		vm->flags |= DISPLAY_FLAGS_VSYNC_HIGH;
	else if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		vm->flags |= DISPLAY_FLAGS_VSYNC_LOW;
}

bool
hisi_dsi_drm_encoder_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	bool ret = true;

	return ret;
}

void hisi_dsi_drm_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static struct drm_encoder_helper_funcs hisi_encoder_helper_funcs = {
	.mode_fixup	= hisi_dsi_drm_encoder_mode_fixup,
	.mode_set	= hisi_dsi_drm_encoder_mode_set,
	.enable		= hisi_dsi_drm_encoder_enable,
	.disable	= hisi_dsi_drm_encoder_disable
};

static struct drm_encoder_funcs hisi_encoder_funcs = {
	.destroy = hisi_dsi_drm_encoder_destroy
};

void hisi_drm_encoder_init(struct drm_device *dev,
			   struct drm_encoder *encoder)
{
	encoder->possible_crtcs = 1;

	drm_encoder_init(dev, encoder, &hisi_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS);
	drm_encoder_helper_add(encoder, &hisi_encoder_helper_funcs);
}

static int dsi_host_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *mdsi)
{
	struct hisi_dsi *dsi = to_hisi_dsi_host(host);

	if (mdsi->lanes > 4 || mdsi->channel > 3)
		return -EINVAL;

	dsi->lanes = mdsi->lanes;
	dsi->format = mdsi->format;
	dsi->mode_flags = mdsi->mode_flags;

	return 0;
}

static int dsi_host_detach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *mdsi)
{
	struct hisi_dsi *dsi = to_hisi_dsi_host(host);

	dsi->device_node = NULL;

	return 0;
}

static struct mipi_dsi_host_ops dsi_host_ops = {
	.attach = dsi_host_attach,
	.detach = dsi_host_detach,
};

static void dsi_mgr_bridge_pre_enable(struct drm_bridge *bridge)
{
}

static void dsi_mgr_bridge_enable(struct drm_bridge *bridge)
{
}

static void dsi_mgr_bridge_disable(struct drm_bridge *bridge)
{
}

static void dsi_mgr_bridge_post_disable(struct drm_bridge *bridge)
{
}

static void dsi_mgr_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
}

static const struct drm_bridge_funcs hisi_dsi_bridge_funcs = {
	.pre_enable = dsi_mgr_bridge_pre_enable,
	.enable = dsi_mgr_bridge_enable,
	.disable = dsi_mgr_bridge_disable,
	.post_disable = dsi_mgr_bridge_post_disable,
	.mode_set = dsi_mgr_bridge_mode_set,
};

static int hisi_dsi_manager_bridge_init(struct drm_device *dev,
					struct hisi_dsi *dsi)
{
	struct drm_bridge *int_bridge = &dsi->bridge;
	struct drm_bridge *ext_bridge;
	struct drm_encoder *encoder = &dsi->encoder;
	struct mipi_dsi_host *host = &dsi->host;
	int ret;

	if (!dsi->registered) {
		host->dev = &dsi->pdev->dev;
		host->ops = &dsi_host_ops;
		ret = mipi_dsi_host_register(host);
		if (ret)
			return ret;

		dsi->registered = true;

	if (dsi->device_node) {
		if (!of_drm_find_bridge(dsi->device_node))
			return -EPROBE_DEFER;
		}
	}

	int_bridge->funcs = &hisi_dsi_bridge_funcs;
	ret = drm_bridge_attach(dev, int_bridge);
	if (ret) {
		DRM_ERROR("failed to drm bridge attach\n");
		return ret;
	}

	encoder->bridge = int_bridge;

	ext_bridge = of_drm_find_bridge(dsi->device_node);
	if (!ext_bridge) {
		DRM_ERROR("failed to find drm bridge\n");
		return ret;
	}

	/* link the internal dsi bridge to the external bridge */
	int_bridge->next = ext_bridge;
	/* set the external bridge's encoder as dsi's encoder */
	ext_bridge->encoder = encoder;

	drm_bridge_attach(dev, ext_bridge);

	return 0;
}

static int hisi_dsi_bind(struct device *dev, struct device *master,
			 void *data)
{
	struct hisi_dsi_context *ctx = dev_get_drvdata(dev);
	int ret = 0;

	ctx->dev = data;

	hisi_drm_encoder_init(ctx->dev, &ctx->dsi.encoder);

	ret = hisi_dsi_manager_bridge_init(ctx->dev, &ctx->dsi);
	if (ret) {
		DRM_ERROR("failed to dsi manager bridge init\n");
		return ret;
	}

	return ret;
}

static void hisi_dsi_unbind(struct device *dev, struct device *master,
			    void *data)
{
	/* do nothing */
}

static const struct component_ops hisi_dsi_ops = {
	.bind	= hisi_dsi_bind,
	.unbind	= hisi_dsi_unbind,
};

static int hisi_dsi_probe(struct platform_device *pdev)
{
	struct hisi_dsi *dsi;
	struct hisi_dsi_context *ctx;
	struct resource *res;
	struct device_node *endpoint, *device_node;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		DRM_ERROR("failed to allocate hisi dsi context.\n");
		ret = -ENOMEM;
	}

	ctx->dsi_cfg_clk = devm_clk_get(&pdev->dev, "pclk_dsi");
	if (IS_ERR(ctx->dsi_cfg_clk)) {
		DRM_ERROR("failed to parse the dsi config clock\n");
		ret = PTR_ERR(ctx->dsi_cfg_clk);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctx->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctx->base)) {
		DRM_ERROR("failed to remap dsi io region\n");
		ret = PTR_ERR(ctx->base);
	}

	/*
	 * Get the first endpoint node. In our case, dsi has one output port
	 * to which the panel is connected. Don't return an error if a port
	 * isn't defined. It's possible that there is nothing connected to
	 * the dsi output.
	 */
	endpoint = of_graph_get_next_endpoint(np, NULL);
	if (!endpoint) {
		dev_err(&pdev->dev, "%s: no endpoint\n", __func__);
		return 0;
	}

	/* Get panel node from the output port's endpoint data */
	device_node = of_graph_get_remote_port_parent(endpoint);
	if (!device_node) {
		dev_err(&pdev->dev, "%s: no valid device\n", __func__);
		of_node_put(endpoint);
		return -ENODEV;
	}

	of_node_put(endpoint);
	of_node_put(device_node);

	dsi = &ctx->dsi;
	dsi->pdev = pdev;
	dsi->device_node = device_node;
	dsi->ctx = ctx;
	dsi->lanes = 3;
	dsi->date_enable_pol = 0;
	dsi->color_mode = DSI_24BITS_1;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	platform_set_drvdata(pdev, ctx);

	return component_add(&pdev->dev, &hisi_dsi_ops);
}

static int hisi_dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &hisi_dsi_ops);

	return 0;
}

static const struct of_device_id hisi_dsi_of_match[] = {
	{.compatible = "hisilicon,hi6220-dsi"},
	{ }
};
MODULE_DEVICE_TABLE(of, hisi_dsi_of_match);

static struct platform_driver hisi_dsi_driver = {
	.probe = hisi_dsi_probe,
	.remove = hisi_dsi_remove,
	.driver = {
		.name = "hisi-dsi",
		.owner = THIS_MODULE,
		.of_match_table = hisi_dsi_of_match,
	},
};

module_platform_driver(hisi_dsi_driver);

MODULE_AUTHOR("Xinwei Kong <kong.kongxinwei@hisilicon.com>");
MODULE_AUTHOR("Xinliang Liu <z.liuxinliang@huawei.com>");
MODULE_DESCRIPTION("hisilicon SoC DRM driver");
MODULE_LICENSE("GPL v2");
