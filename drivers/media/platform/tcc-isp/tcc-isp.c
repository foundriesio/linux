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
#include <linux/firmware.h>
#include <media/v4l2-subdev.h>
#include <soc/tcc/chipinfo.h>
#include "tcc-isp.h"
#include "tcc-isp-reg.h"
#include "tcc-isp-settings.h"

#define LOG_TAG			TCC_ISP_DRIVER_NAME

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

#define DEFAULT_WIDTH		0x8000
#define DEFAULT_HEIGHT		0x8000

static DEFINE_MUTEX(isp_lock);
struct tcc_isp_mcu_control {
	struct tcc_isp_state *isp_state;
	int is_probed;
	int is_started;
};
static struct tcc_isp_mcu_control mcu_ctl[4];

struct v4l2_dv_timings tcc_isp_dv_timings = {
	.type			= V4L2_DV_BT_656_1120,
	.bt			= {
		.width			= DEFAULT_WIDTH,
		.height			= DEFAULT_HEIGHT,
		.interlaced		= V4L2_DV_PROGRESSIVE,
		/* IMPORTANT
		 * The below field "polarities" is not used
		 * becasue polarities for vsync and hsync are supported only.
		 * So, use flags of "struct v4l2_mbus_config".
		 */
		.polarities		= 0,
	},
};

struct v4l2_mbus_config isp_mbus_config = {
	.type			= V4L2_MBUS_PARALLEL,
	/* de: high, vs: high, hs: high, pclk: high */
	.flags			=
		V4L2_MBUS_DATA_ACTIVE_HIGH	|
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
		V4L2_MBUS_HSYNC_ACTIVE_HIGH	|
		V4L2_MBUS_PCLK_SAMPLE_RISING	|
		V4L2_MBUS_MASTER,
};

/*
 * Helper functions for reflection
 */
static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *c)
{
	return (&container_of(c->handler, struct tcc_isp_state, ctrl_hdl)->sd);
}

static inline struct tcc_isp_state *sd_to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct tcc_isp_state, sd);
}

static inline void regw(void __iomem *reg, u32 val)
{
#if 0
	u32 isp_reg_offset = (u32)reg & 0xFFFF;

	pr_info("[INFO][tcc-isp] reg: 0x%08x, val: 0x%08x\n",
		isp_reg_offset, val);
#endif
	__raw_writel(val, reg);
}

static inline u32 regr(void __iomem *reg)
{
	u32 val = 0;

	val = __raw_readl(reg);

	return val;
}

static int tcc_isp_check_all_isp_is_started(void)
{
	int ret = 0;

	mutex_lock(&isp_lock);

	if ((mcu_ctl[0].is_probed ? mcu_ctl[0].is_started : 1) &&
	    (mcu_ctl[1].is_probed ? mcu_ctl[1].is_started : 1) &&
	    (mcu_ctl[2].is_probed ? mcu_ctl[2].is_started : 1) &&
	    (mcu_ctl[3].is_probed ? mcu_ctl[3].is_started : 1)) {
		ret = 1;
	} else {
		ret = 0;
	}

	mutex_unlock(&isp_lock);

	return ret;
}

static int tcc_isp_check_all_isp_is_stopped(void)
{
	int ret = 0;

	mutex_lock(&isp_lock);

	if ((mcu_ctl[0].is_probed ? !(mcu_ctl[0].is_started) : 1) &&
	    (mcu_ctl[1].is_probed ? !(mcu_ctl[1].is_started) : 1) &&
	    (mcu_ctl[2].is_probed ? !(mcu_ctl[2].is_started) : 1) &&
	    (mcu_ctl[3].is_probed ? !(mcu_ctl[3].is_started) : 1)) {
		ret = 1;
	} else {
		ret = 0;
	}

	mutex_unlock(&isp_lock);

	return ret;
}

static void tcc_isp_set_apbaddr(struct tcc_isp_state *state)
{
	void __iomem *reg = 0;

	/* TCS: CV8050C-658 */
	reg = state->cfg_base + ISP_APBADDR_CFG0;
	__raw_writel(0x16371637, reg);
	reg = state->cfg_base + ISP_APBADDR_CFG1;
	__raw_writel(0x16371637, reg);
}

static void tcc_isp_set_input_data_padding(struct tcc_isp_state *state,
	unsigned int code)
{
	unsigned int val, padding;
	void __iomem *reg = 0;
	int ch = state->pdev->id;

	mutex_lock(&isp_lock);

	switch (code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		padding = ISP_FMT_RAW_RGB_8_TO_10_USING_ZERO;
		break;
	default:
		padding = ISP_FMT_RAW_RGB_10_OR_MORE;
		logi(&(state->pdev->dev), "padding data is not necessary\n");
	}
	reg = state->cfg_base + ISP_FMT_CFG;

	val = (__raw_readl(reg) & ~(ISP_FMT_CFG_ISP0_FMT_MASK << (ch * 4)));
	val |= (padding << (ISP_FMT_CFG_ISP0_FMT_SHIFT + (ch * 4)));
	__raw_writel(val, reg);

	mutex_unlock(&isp_lock);
}

static inline s32 tcc_isp_set_async_bridge_low_power_request(
				struct tcc_isp_state *state)
{
	void __iomem *mem_base = state->cfg_base;
	s32 ret = 0;
	u32 val = 0, cnt = 0;

	/* disable power down bypass(pwrdn_bypass) */
	val = regr(mem_base + ISP_X2X_CFG);
	val &= ~ISP_X2X_CFG_PWRDN_BYPASS_MASK;
	regw(mem_base + ISP_X2X_CFG, val);
	usleep_range(1000, 2000);

	/* request power down(pwrdnreqn) */
	val = regr(mem_base + ISP_X2X_CFG);
	val &= ~ISP_X2X_CFG_PWRDNREQN_MASK;
	regw(mem_base + ISP_X2X_CFG, val);
	usleep_range(1000, 2000);

	/* check power down request acknowledge(pwrdnackn) */
	while (cnt < 10) {
		val = regr(mem_base + ISP_X2X_CFG);
		if (!(val & ISP_X2X_CFG_PWRDNACKN_MASK)) {
			/* wait ack */
			break;
		}

		cnt++;
		usleep_range(1000, 2000);
	}

	if (cnt >= 10) {
		ret = -1;
		loge(&(state->pdev->dev), "NOT recevied ack\n");
	}

	return ret;
}

static inline s32 tcc_isp_set_async_bridge_low_power_release(
				struct tcc_isp_state *state)
{
	void __iomem *mem_base = state->cfg_base;
	s32 ret = 0;
	u32 val = 0, cnt = 0;

	/* request normal operation(pwrdnreqn) */
	val = regr(mem_base + ISP_X2X_CFG);
	val |= ISP_X2X_CFG_PWRDNREQN_MASK;
	regw(mem_base + ISP_X2X_CFG, val);
	usleep_range(1000, 2000);

	/* check normal operation request acknowledge(pwrdnackn) */
	while (cnt < 10) {
		val = regr(mem_base + ISP_X2X_CFG);
		if (val & ISP_X2X_CFG_PWRDNACKN_MASK) {
			/* get ack */
			break;
		}

		cnt++;
		usleep_range(1000, 2000);
	}

	if (cnt >= 10) {
		ret = -1;
		loge(&(state->pdev->dev), "NOT recevied ack\n");
	}

	return ret;
}

static s32 tcc_isp_pixel_order(u32 pixel_order)
{
	s32 tcc_order;

	switch (pixel_order) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		tcc_order = IMG_IN_ORDER_CTL_IMG_IN_PIXEL_ORDER_B_FIRST;
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		tcc_order = IMG_IN_ORDER_CTL_IMG_IN_PIXEL_ORDER_GB_FIRST;
		break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		tcc_order = IMG_IN_ORDER_CTL_IMG_IN_PIXEL_ORDER_GR_FIRST;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		tcc_order = IMG_IN_ORDER_CTL_IMG_IN_PIXEL_ORDER_R_FIRST;
		break;
	default:
		tcc_order = -1;
	}

	return tcc_order;
}

static inline void tcc_isp_update_register(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;

	regw(isp_base + REG_ISP_UP_CTL, UP_CTL_UP_ALL);
}

static inline void tcc_isp_mcu_enable(struct tcc_isp_state *state, int onOff)
{
	unsigned short val = 0;
	void __iomem *isp_base = state->isp_base;

	if (onOff == ON) {
		/* MCU enable */
		val = regr(isp_base + REG_ISP_MCU_CTL);
		val &= ~(MCU_CTL_MCU_EN_MASK <<
			 MCU_CTL_MCU_EN_SHIFT);
		val |= (MCU_CTL_MCU_EN_ENABLE << MCU_CTL_MCU_EN_SHIFT);
		regw(isp_base + REG_ISP_MCU_CTL, val);

	} else {
		/* MCU disable */
		val = regr(isp_base + REG_ISP_MCU_CTL);
		val &= ~(MCU_CTL_MCU_EN_MASK <<
			  MCU_CTL_MCU_EN_SHIFT);
		val |= (MCU_CTL_MCU_EN_DISABLE << MCU_CTL_MCU_EN_SHIFT);
		regw(isp_base + REG_ISP_MCU_CTL, val);
	}
}

static void tcc_isp_load_firmware(
		struct tcc_isp_state *state, const void *fw, size_t count)
{
	unsigned short val = 0;
	void __iomem *isp_base = state->isp_base;
	void __iomem *mem_base = state->mem_base;

	/* MCU memory download enable */
	val = regr(isp_base + REG_ISP_MCU_MEM_CTL);
	val &= ~(MCU_MEM_CTL_MCU_MEM_DL_EN_MASK <<
		MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	val |= (MCU_MEM_CTL_MCU_MEM_DL_EN_ENABLE <<
		MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	regw(isp_base + REG_ISP_MCU_MEM_CTL, val);

	if (count == ISP_MEM_SIZE_CODE_MEM) {
		logi(&(state->pdev->dev), "COPY FW(code mem)\n");

		/* copy firmware to the MCU memory(data) */
		memcpy(mem_base + ISP_MEM_OFFSET_CODE_MEM, fw, count);
	} else {
		logi(&(state->pdev->dev), "COPY FW(code and data mem)\n");

		/* copy firmware to the MCU memory(code + data) */
		memcpy(mem_base + ISP_MEM_OFFSET_DATA_MEM, fw, count);
	}

	/*
	 * after copying firmware,
	 * all related cache coherency should be ensured.
	 */
	mb();

	/* MCU memory download disable */
	val = regr(isp_base + REG_ISP_MCU_MEM_CTL);
	val &= ~(MCU_MEM_CTL_MCU_MEM_DL_EN_MASK <<
		MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	val |= (MCU_MEM_CTL_MCU_MEM_DL_EN_DISABLE <<
		MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	regw(isp_base + REG_ISP_MCU_MEM_CTL, val);
}

static void tcc_isp_callback_load_firmware(
		const struct firmware *fw, void *context)
{
	struct tcc_isp_state *state = (struct tcc_isp_state *)context;


	if (fw == NULL) {
		logw(&(state->pdev->dev), "Timeout - firmware loading\n");
		return;
	}


	logi(&(state->pdev->dev), "FW size: %ld\n", fw->size);

	tcc_isp_load_firmware(state, fw->data, fw->size);

	state->fw_load = 1;

	release_firmware(fw);

	logi(&(state->pdev->dev), "success callback (%s)\n", __func__);
}

static int tcc_isp_request_firmware(
		struct tcc_isp_state *state, const char *fw_name)
{
	int ret = 0;

	logi(&(state->pdev->dev), "request firmware(%s)\n", fw_name);

	ret = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_HOTPLUG,
		fw_name,
		&(state->pdev->dev),
		GFP_KERNEL,
		state,
		tcc_isp_callback_load_firmware);

	return ret;
}

static inline void tcc_isp_mem_share(struct tcc_isp_state *state, int onOff)
{
	void __iomem *isp_base = state->isp_base;

	if (onOff) {
		regw(isp_base + REG_ISP_MEM_SHARE,
			(MEM_SHARE_MEM_SHARE_EN_ENABLE <<
			MEM_SHARE_MEM_SHARE_EN_SHIFT));
	} else {
		regw(isp_base + REG_ISP_MEM_SHARE,
			(MEM_SHARE_MEM_SHARE_EN_DISABLE <<
			MEM_SHARE_MEM_SHARE_EN_SHIFT));
	}
}

static inline void tcc_isp_set_register_update_mode(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	u32 up_sel1, up_sel2, up_mode1, up_mode2, user_cnt1, user_cnt2;
	static const char * const str_update_sel[] = {
		"USER SPECIFIED TIMING",
		"VSYNC FALLING EDGE TIMING",
		"VSYNC RISING EDGE TIMING",
		"WRITING TIMING"
	};
	static const char * const str_up_mode1[] = {
		"SYNC MODE",
		"ALWAYS MODE"
	};
	static const char * const str_up_mode2[] = {
		"INDIVIDUAL SYNC",
		"GROUP SYNC"
	};

	up_sel1 = UP_SEL1_ALL_SYNC_ON_USER_SPECIFIED_TIMING;
	up_sel2 = UP_SEL2_ALL_SYNC_ON_USER_SPECIFIED_TIMING;

	up_mode1 = UP_MODE1_ALL_SYNC_MODE;
	up_mode2 = UP_MODE2_ALL_INDIV_SYNC_MODE;

	user_cnt1 = (0 << USR_CNT1_SHIFT);
	user_cnt2 = (0xF << USR_CNT2_SHIFT);

	/* USR_CNT1(MSB) + USR_CNT2(LSB) is used in USER SPECIFIED TIMING */
	regw(isp_base + REG_ISP_USR_CNT1, user_cnt1);
	regw(isp_base + REG_ISP_USR_CNT2, user_cnt2);

	/*
	 * 0: sync on user specified timing
	 * 1: sync on vsync falling edge timing
	 * 2: sync on vsync rising edge timing
	 * 3: sync on writing timing
	 */
	logi(&(state->pdev->dev), "up_sel1, 2(0x%x, 0x%x) is %s\n",
		up_sel1, up_sel2,
		str_update_sel[(up_sel1 >> UP_SEL1_TP_SEL_CTL_SHIFT) &
			(UP_SEL1_TP_SEL_CTL_MASK)]);

	regw(isp_base + REG_ISP_UP_SEL1, up_sel1);
	regw(isp_base + REG_ISP_UP_SEL2, up_sel2);

	/*
	 * 0: sync mode
	 * 1: async mode
	 */
	logi(&(state->pdev->dev), "up_mode1(0x%x) is %s\n",
		up_mode1,
		str_up_mode1[up_mode1 &
			(UP_MODE1_TP_UP_MODE1_MASK <<
			UP_MODE1_TP_UP_MODE1_SHIFT)]);

	regw(isp_base + REG_ISP_UP_MODE1, up_mode1);

	/*
	 * 0: individual sync
	 * 1: group sync
	 */
	logi(&(state->pdev->dev), "up_mode2(0x%x) is %s\n",
		up_mode2,
		str_up_mode2[up_mode2 &
			(UP_MODE2_TP_UP_MODE2_MASK <<
			UP_MODE2_TP_UP_MODE2_SHIFT)]);

	regw(isp_base + REG_ISP_UP_MODE2, up_mode2);
}

static inline void tcc_isp_set_sensor_id(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	static const char * const i2c_slv_mode[] = {
		"ADDR(8 bit), DATA(8 bit)",
		"ADDR(8 bit), DATA(16 bit)",
		"ADDR(16 bit), DATA(8 bit)",
		"ADDR(16 bit), DATA(16 bit)"
	};

	logi(&(state->pdev->dev), "I2C SLAVE MODE is %s, ID is 0x%x\n",
		i2c_slv_mode[state->tune->i2c_ctrl.i2c_slv_mode],
		state->tune->i2c_ctrl.i2c_slv_id);

	regw(isp_base + REG_ISP_ATI_I2C_SLV_CTL,
		((state->tune->i2c_ctrl.i2c_slv_id <<
		  ATI_I2C_SLV_CTL_I2C_SLV_ID_SHIFT) |
		(state->tune->i2c_ctrl.i2c_slv_mode <<
		 ATI_I2C_SLV_CTL_I2C_SLV_MODE_SHIFT)));
}

static inline void tcc_isp_set_wdma(struct tcc_isp_state *state, int onOff)
{
	void __iomem *isp_base = state->isp_base;

	if (onOff == ON) {
		regw(isp_base + REG_ISP_WDMA_CTL0,
			(WDMA_CTL0_WDMA_ENABLE <<
			WDMA_CTL0_WDMA_ENABLE_SHIFT));
	} else {
		regw(isp_base + REG_ISP_WDMA_CTL0,
			(WDMA_CTL0_WDMA_DISABLE <<
			WDMA_CTL0_WDMA_ENABLE_SHIFT));
		/* CV8050C-810 */
		/* regw(isp_base + REG_ISP_WDMA_CFG0, 0x0); */
	}
}

static inline void tcc_isp_set_input(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	static const char * const str[] = {
		"Blue First", "Gb first", "Gr first", "Red first"};
	int w, h, pixel_order;

	w = state->isp.i_state.width;
	h = state->isp.i_state.height;
	pixel_order = state->isp.i_state.pixel_order;

	logi(&(state->pdev->dev), "input size(%d x %d) rgb order(%s)\n",
		w, h, str[pixel_order]);

	/* size */
	regw(isp_base + REG_ISP_IMG_WIDTH,
		(w << IMG_WIDTH_IN_IMG_WIDTH_SHIFT));
	regw(isp_base + REG_ISP_IMG_HEIGHT,
		(h << IMG_HEIGHT_IN_IMG_HEIGHT_SHIFT));

	/* bayer rgb order */
	regw(isp_base + REG_ISP_IMG_IN_ORDER_CTL,
		(pixel_order << IMG_IN_ORDER_CTL_IMG_IN_PIXEL_ORDER_SHIFT));
}

static inline void tcc_isp_set_output(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	static const char * const str[] = {
		"YUV420", "YUV422", "YUV444", "RGB888"};
	int x, y, w, h, fmt, win_ctl;

	x = state->isp.o_state.x;
	y = state->isp.o_state.y;
	w = state->isp.o_state.width;
	h = state->isp.o_state.height;
	fmt = state->isp.o_state.format;
	win_ctl = 0;

	logi(&(state->pdev->dev), "output format(%s)\n", str[fmt]);

	if (state->isp.i_state.width != w || state->isp.i_state.height != h) {
		logi(&(state->pdev->dev), "enable crop window\n");
		win_ctl = (IMG_WIN_CTL_WIN_EN_ENABLE <<
				IMG_WIN_CTL_WIN_EN_SHIFT);
	} else {
		logi(&(state->pdev->dev), "disable crop window\n");
	}

	win_ctl |= (IMG_WIN_CTL_DEBLANK_EN_ENABLE <<
			IMG_WIN_CTL_DEBLANK_EN_SHIFT);

	regw(isp_base + REG_ISP_IMG_WIN_CTL, win_ctl);

	/* format */
	switch (fmt) {
	case IMG_WIN_FORMAT_IMG_WIN_FORMAT_YUV444:
		fmt <<= IMG_WIN_FORMAT_IMG_WIN_FORMAT_SHIFT;
		break;
	case IMG_WIN_FORMAT_IMG_WIN_FORMAT_RGB888:
		fmt <<= IMG_WIN_FORMAT_IMG_WIN_FORMAT_SHIFT;
		break;
	case IMG_WIN_FORMAT_IMG_WIN_FORMAT_YUV422:
	default:
		fmt <<= IMG_WIN_FORMAT_IMG_WIN_FORMAT_SHIFT;

		fmt &= ~(IMG_WIN_FORMAT_IMG_DATA_ORDER_MASK <<
			IMG_WIN_FORMAT_IMG_DATA_ORDER_SHIFT);
		fmt |= (IMG_WIN_FORMAT_IMG_DATA_ORDER_P0P2P1 <<
			IMG_WIN_FORMAT_IMG_DATA_ORDER_SHIFT);
		break;
	}
	regw(isp_base + REG_ISP_IMG_WIN_FORMAT, fmt);

	/* crop */
	if (state->isp.i_state.width != w || state->isp.i_state.height != h) {
		logi(&(state->pdev->dev), "output crop(%d, %d / %d x %d)\n",
				x, y, w, h);

		regw(isp_base + REG_ISP_IMG_WIN_X_START,
			x << IMG_WIN_X_START_SHIFT);
		regw(isp_base + REG_ISP_IMG_WIN_Y_START,
			y << IMG_WIN_Y_START_SHIFT);
		regw(isp_base + REG_ISP_IMG_WIN_WIDTH,
			w << IMG_WIN_WIDTH_SHIFT);
		regw(isp_base + REG_ISP_IMG_WIN_HEIGHT,
			h << IMG_WIN_HEIGHT_SHIFT);
	}
}

static inline void tcc_isp_set_decompanding(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	static const char * const str_dcpd_input_bit[] = {
		"10bit", "12bit"};
	static const char * const str_dcpd_output_bit[] = {
		"10bit", "12bit", "14bit", "15bit",
		"16bit", "17bit", "20bit"};
	u32 i = 0, val = 0;

	logi(&(state->pdev->dev), "dcpd input %s, output %s\n",
		str_dcpd_input_bit[state->hdr->decompanding_input_bit],
		str_dcpd_output_bit[state->hdr->decompanding_output_bit]);

	/* default decompanding setting */
	val = ((state->hdr->decompanding_curve_maxval <<
		DCPD_CTL_DCPD_CUR_MAXVAL_SHIFT) |
		(state->hdr->decompanding_output_bit <<
		DCPD_CTL_OUT_BIT_SEL_SHIFT) |
		(state->hdr->decompanding_input_bit <<
		DCPD_CTL_IN_BIT_SEL_SHIFT) |
		(DCPD_CTL_DCPD_EN_ENABLE <<
		DCPD_CTL_DCPD_EN_SHIFT));

	/* decompanding curve gain0 */
	val |= (state->hdr->dcpd_cur_gain[0] << DCPD_CTL_DCPD_CUR_GAIN0_SHIFT);

	regw(isp_base + REG_ISP_DCPD_CTL, val);

	/* decompanding curve gain1~6 */
	for (i = 0; i < 3; i++) {
		regw(isp_base + REG_ISP_DCPD_CUR_GCFG1 + (i * 4),
			((state->hdr->dcpd_cur_gain[(i * 2) + 1] <<
			DCPD_CUR_GCFG1_DCPD_CUR_GAIN1_SHIFT) |
			(state->hdr->dcpd_cur_gain[(i * 2) + 2] <<
			DCPD_CUR_GCFG1_DCPD_CUR_GAIN2_SHIFT)));
	}

	/* decompanding curve gain7, curve x-axis0 */
	val = ((state->hdr->dcpd_cur_gain[7] <<
		DCPD_CUR_GCFG4_DCPD_CUR_GAIN7_SHIFT) |
		(state->hdr->dcpd_cur_x_axis[0] <<
		DCPD_CUR_GCFG4_DCPD_CUR_X_AXIS0_SHIFT));

	regw(isp_base + REG_ISP_DCPD_CUR_GCFG4, val);

	/* decompanding curve x axis1~6 */
	for (i = 0; i < 3; i++) {
		regw(isp_base + REG_ISP_DCPD_CUR_XCFG1 + (i * 4),
			((state->hdr->dcpd_cur_x_axis[(i * 2) + 1] <<
			DCPD_CUR_XCFG1_DCPD_CUR_X_AXIS1_SHIFT) |
			(state->hdr->dcpd_cur_gain[(i * 2) + 2] <<
			DCPD_CUR_XCFG1_DCPD_CUR_X_AXIS2_SHIFT)));
	}

	/* decompanding x-axis7 */
	val = ((state->hdr->dcpd_cur_x_axis[7] <<
		DCPD_CUR_XCFG4_DCPD_CUR_X_AXIS7_SHIFT));

	regw(isp_base + REG_ISP_DCPD_CUR_XCFG4, val);
}

static inline void tcc_isp_reset(struct tcc_isp_state *state, int reset)
{
	void __iomem *cfg_base = state->cfg_base;
	void __iomem *isp_base = state->isp_base;
	u32 val = 0;
	u32 isp_pinrst =
		(ISP_SWRST_PX2X_SWRST_MASK |
		 ISP_SWRST_PX1X_SWRST_MASK |
		 ISP_SWRST_APB_SWRST_MASK);

	if (reset) {
		if (get_chip_rev()) {
			/* CV8050C-595, CV8050C-689 */
			tcc_isp_set_async_bridge_low_power_request(state);
		}

		/* ISP pin reset (IM032B-35) */
		val = regr(cfg_base + ISP_SWRST);
		val |= isp_pinrst;
		regw(cfg_base + ISP_SWRST, val);
	} else {
		if (get_chip_rev()) {
			/* CV8050C-595, CV8050C-689 */
			tcc_isp_set_async_bridge_low_power_release(state);
		}

		/* ISP pin reset release */
		val = regr(cfg_base + ISP_SWRST);
		val &= ~(isp_pinrst);
		regw(cfg_base + ISP_SWRST, val);

		/* Wakeup ISP */
		regw(isp_base + REG_ISP_SLEEP_MODE,
			(SLEEP_MODE_SLEEP_MODE_DISABLE <<
			 SLEEP_MODE_SLEEP_MODE_SHIFT));
	}
}

static void tcc_isp_set_basic(struct tcc_isp_state *state)
{
	/* set register update control */
	tcc_isp_set_register_update_mode(state);

	/* disable wdma(IM896A-22) */
	tcc_isp_set_wdma(state, OFF);

	if (get_chip_rev()) {
		/* memory sharing */
		tcc_isp_mem_share(state, state->mem_share);

		/* set sensor I2C slave address */
		/* IM032B-35: each firmware includes sensor id */
		//tcc_isp_set_sensor_id(state);
	}

	/*
	 * ZELCOVA setting
	 */
	/* input */
	tcc_isp_set_input(state);
	/* output */
	tcc_isp_set_output(state);

	/*
	 * OAK setting
	 */
	/* De-companding */
	if (state->hdr->mode == HDR_MODE_COMPANDING)
		tcc_isp_set_decompanding(state);

	/* register update */
	tcc_isp_update_register(state);
}

static inline void tcc_isp_set_tune(struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	int i = 0;

	for (i = 0; i < state->tune->isp_setting_size; i++) {
		/* set tune value of ISP */
		regw(isp_base + state->tune->isp[i].reg,
				state->tune->isp[i].val);
	}
	tcc_isp_update_register(state);

	logi(&(state->pdev->dev), "complete %s\n", __func__);
}

static void tcc_isp_enable(struct tcc_isp_state *state, unsigned int enable)
{
	if (enable) {
		/* enable isp */
		tcc_isp_set_basic(state);
		tcc_isp_set_tune(state);
	} else {
		/* disable isp */
		if (tcc_isp_check_all_isp_is_stopped()) {
			/* reset all ISP */
			tcc_isp_reset(state, ON);
			/* reset release of all ISP */
			tcc_isp_reset(state, OFF);
		} else {
			/* already reset has been done */
			logi(&(state->pdev->dev), "skip isp reset\n");
		}
	}
}

static int tcc_isp_parse_dt(struct platform_device *pdev,
		struct tcc_isp_state *state)
{
	struct device *dev = &pdev->dev;
	struct resource *mem_res;
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	/* Get ISP base address */
	mem_res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM,
			"isp_base");
	state->isp_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR((const void *)state->isp_base)) {
		ret = PTR_ERR((const void *)state->isp_base);
		loge(&(state->pdev->dev), "Fail parsing isp_base\n");
		goto err;
	}

	logd(&(state->pdev->dev), "isp base addr is %px\n", state->isp_base);

	/* Get mem base address */
	mem_res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM,
			"mem_base");
	state->mem_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR((const void *)state->mem_base)) {
		ret = PTR_ERR((const void *)state->mem_base);
		loge(&(state->pdev->dev), "Fail parsing mem_base\n");
		goto err;
	}
	logd(&(state->pdev->dev), "mem base addr is %px\n", state->mem_base);

	/*
	 * Get CFG base address
	 */
	state->cfg_base = (void __iomem *)of_iomap(node, 2);
	if (IS_ERR((const void *)state->cfg_base))
		return PTR_ERR((const void *)state->cfg_base);

	logd(&(state->pdev->dev), "cfg base addr is %px\n", state->cfg_base);

#ifdef USE_ISP_UART
	/* Get UART pinctrl */
	state->uart_pinctrl = devm_pinctrl_get_select_default(&(pdev->dev));
	if (IS_ERR(state->uart_pinctrl)) {
		ret = PTR_ERR((const void *)state->uart_pinctrl);
		loge(&(state->pdev->dev), "Fail parsing uart pinctrl\n");
		goto err;
	}
#endif

	/* Get mem_share option */
	of_property_read_u32_index(node,
		"mem_share", 0, &(state->mem_share));
	logi(&(state->pdev->dev), "%s mem_share\n",
		state->mem_share ? "enable" : "disable");
err:

	return ret;
}

static void tcc_isp_set_default(struct tcc_isp_state *state)
{
	state->hdr = hdr_value[state->pdev->id];
	state->tune = &tune_value[state->pdev->id];

#ifndef CONFIG_TCC_ISP_USE_IOMMU
	if (get_chip_rev()) {
		/*
		 * CV8050C-658
		 * set upper 16bit addr
		 */
		tcc_isp_set_apbaddr(state);
	}
#endif

	logi(&(state->pdev->dev),
		"The number of isp tune data(%d)\n",
		state->tune->isp_setting_size);

	/*
	 * Because of ISP algorithm characteristics,
	 * isp output resolution is small than input resolution
	 * (top, bottom, left, top 8 lines will be reduced)
	 */
	state->isp.o_state.x = 8;
	state->isp.o_state.y = 8;

	state->isp.o_state.format = IMG_WIN_FORMAT_IMG_WIN_FORMAT_YUV422;
	state->isp.o_state.data_order = IMG_WIN_FORMAT_IMG_DATA_ORDER_P0P2P1;

	/*
	 * set axi bus output disable
	 */
	/* set register update control */
	tcc_isp_set_register_update_mode(state);

	/* disable wdma(IM896A-22) */
	tcc_isp_set_wdma(state, OFF);

	/* register update */
	tcc_isp_update_register(state);
}

static int tcc_isp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct tcc_isp_state *state = sd_to_state(sd);
	int ret = 0;
	int val;

	if (ret)
		return ret;

	val = ctrl->val;
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		logi(&(state->pdev->dev), "V4L2_CID_BRIGHTNESS\n");
		break;
	default:
		loge(&(state->pdev->dev),
			"NOT supported CID(0x%x)\n", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops tcc_isp_ctrl_ops = {
	.s_ctrl = tcc_isp_s_ctrl,
};

static int tcc_isp_init_controls(struct tcc_isp_state *state)
{
	int ret = 0;

	v4l2_ctrl_handler_init(&state->ctrl_hdl, 1);

	v4l2_ctrl_new_std(&state->ctrl_hdl, &tcc_isp_ctrl_ops,
			V4L2_CID_BRIGHTNESS, TCC_ISP_BRI_MIN,
			TCC_ISP_BRI_MAX, 1, TCC_ISP_BRI_DEF);

	state->sd.ctrl_handler = &state->ctrl_hdl;
	if (state->ctrl_hdl.error) {
		ret  = state->ctrl_hdl.error;

		v4l2_ctrl_handler_free(&state->ctrl_hdl);
		loge(&(state->pdev->dev), "FAIL %s\n", __func__);
		return ret;
	}

	/*
	 * call s_ctrl for all controls unconditionally.
	 * this ensures that both the internal data and
	 * the hardware are in sync
	 */
	 v4l2_ctrl_handler_setup(&state->ctrl_hdl);

	return ret;
}

static void tcc_isp_exit_controls(struct tcc_isp_state *state)
{
	v4l2_ctrl_handler_free(&state->ctrl_hdl);
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_isp_s_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	return ret;
}

static int tcc_isp_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tcc_isp_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	tcc_isp_enable(state, enable);

	return ret;
}

static int tcc_isp_load_fw(struct v4l2_subdev *sd)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	int ret = 0;

	if (state->fw_load == 1) {
		logi(&(state->pdev->dev), "skip loading firmware\n");
		goto end;
	}

	ret = tcc_isp_request_firmware(state, state->isp_fw_name);
	if (ret < 0) {
		loge(&(state->pdev->dev), "FAIL - loading firmware(%s)\n",
			TCC_ISP_FIRMWARE_NAME);
		goto end;
	}
end:
	return ret;
}

static int tcc_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	int ret = 0;

	if (enable) {
		mutex_lock(&isp_lock);
		mcu_ctl[state->pdev->id].is_started = 1;
		mutex_unlock(&isp_lock);

		if (tcc_isp_check_all_isp_is_started()) {
			logi(&(state->pdev->dev), "enable mcu\n");

			/* enable mcu */
			if (mcu_ctl[0].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[0].isp_state, ON);
			if (mcu_ctl[1].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[1].isp_state, ON);
			if (mcu_ctl[2].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[2].isp_state, ON);
			if (mcu_ctl[3].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[3].isp_state, ON);

			if (state->mdelay_to_out) {
				logi(&(state->pdev->dev),
					"delay %dms\n", state->mdelay_to_out);
				usleep_range(state->mdelay_to_out * 1000,
					(state->mdelay_to_out * 1000) + 5000);
			}
		} else {
			logi(&(state->pdev->dev), "skip enable mcu\n");
		}
	} else {
		/* disable mcu */
		mutex_lock(&isp_lock);
		mcu_ctl[state->pdev->id].is_started = 0;
		mutex_unlock(&isp_lock);

		if (tcc_isp_check_all_isp_is_stopped()) {
			logi(&(state->pdev->dev), "disable mcu\n");

			if (mcu_ctl[0].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[0].isp_state, OFF);
			if (mcu_ctl[1].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[1].isp_state, OFF);
			if (mcu_ctl[2].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[2].isp_state, OFF);
			if (mcu_ctl[3].is_probed)
				tcc_isp_mcu_enable(mcu_ctl[3].isp_state, OFF);
		} else {
			logi(&(state->pdev->dev), "skip disable mcu\n");
		}
	}

	return ret;
}

static int tcc_isp_g_mbus_config(struct v4l2_subdev *sd,
				       struct v4l2_mbus_config *cfg)
{
	memcpy((void *)cfg, (const void *)&isp_mbus_config, sizeof(*cfg));

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int tcc_isp_enum_mbus_code(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_mbus_code_enum *code)
{
	return 0;
}

static int tcc_isp_g_dv_timings(struct v4l2_subdev *sd,
				      struct v4l2_dv_timings *timings)
{
	int ret = 0;

	memcpy((void *)timings,
		(const void *)&tcc_isp_dv_timings,
		sizeof(*timings));

	return ret;
}

static int tcc_isp_get_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *format)
{
	struct tcc_isp_state	*state	= sd_to_state(sd);
	int ret	= 0;

	memcpy((void *)&format->format,
		(const void *)&state->fmt,
		sizeof(struct v4l2_mbus_framefmt));
	return ret;
}

static int tcc_isp_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *format)
{
	struct tcc_isp_state	*state	= sd_to_state(sd);
	int ret	= 0;

	memcpy((void *)&state->fmt,
		(const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	state->isp.i_state.width = state->fmt.width;
	/* set -2 to get the margin of vertical front porch */
	state->isp.i_state.height = state->fmt.height - 2;

	ret = tcc_isp_pixel_order(state->fmt.code);
	if (ret < 0) {
		loge(&(state->pdev->dev),
			"RGB order(0x%x) is not supported\n", state->fmt.code);
		goto err;
	}

	state->isp.i_state.pixel_order = ret;

	tcc_isp_set_input_data_padding(state, state->fmt.code);

	/*
	 * Because of ISP algorithm characteristics,
	 * isp output resolution is small than input resolution
	 * (top, bottom, left, top 8 lines will be reduced)
	 */
	state->fmt.width -= 16;
	state->fmt.height -= 16;

#if 0
	/* for test. if this code is enabled, crop is disabled  */
	state->fmt.width += 16;
	state->fmt.height += 16;
	state->isp.o_state.x = 0;
	state->isp.o_state.y = 0;
#endif

	state->isp.o_state.width = state->fmt.width;
	state->isp.o_state.height = state->fmt.height;

	tcc_isp_dv_timings.bt.width = state->fmt.width;
	tcc_isp_dv_timings.bt.height = state->fmt.height;

	switch (state->isp.o_state.format) {
	case IMG_WIN_FORMAT_IMG_WIN_FORMAT_YUV422:
		state->fmt.code = MEDIA_BUS_FMT_UYVY8_1X16;
		break;
	case IMG_WIN_FORMAT_IMG_WIN_FORMAT_YUV444:
		state->fmt.code = MEDIA_BUS_FMT_YUV8_1X24;
		break;
	case IMG_WIN_FORMAT_IMG_WIN_FORMAT_RGB888:
		state->fmt.code = MEDIA_BUS_FMT_BGR888_1X24;
		break;
	}
err:
	return ret;
}
/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops tcc_isp_core_ops = {
	.s_power		= tcc_isp_s_power,
	.init			= tcc_isp_init,
	.load_fw		= tcc_isp_load_fw,
};

static const struct v4l2_subdev_pad_ops tcc_isp_pad_ops = {
	.enum_mbus_code		= tcc_isp_enum_mbus_code,
	.get_fmt		= tcc_isp_get_fmt,
	.set_fmt		= tcc_isp_set_fmt,
};

static const struct v4l2_subdev_video_ops tcc_isp_video_ops = {
	.s_stream		= tcc_isp_s_stream,
	.g_mbus_config		= tcc_isp_g_mbus_config,
	.g_dv_timings		= tcc_isp_g_dv_timings,
};

static const struct v4l2_subdev_ops tcc_isp_ops = {
	.core			= &tcc_isp_core_ops,
	.video			= &tcc_isp_video_ops,
	.pad			= &tcc_isp_pad_ops,
};

static const struct of_device_id tcc_isp_of_match[];

static ssize_t mdelay_to_output_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tcc_isp_state *state;
	ssize_t ret = 0;

	state = platform_get_drvdata(to_platform_device(dev));
	if (state == NULL) {
		pr_err("Fail - tcc_isp_state pointer\n");
		ret = -ENODEV;
		goto end;
	}

end:
	return ret ? ret : sprintf(buf, "%d\n", state->mdelay_to_out);
}

static ssize_t mdelay_to_output_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned int val;
	struct tcc_isp_state *state;
	ssize_t ret = 0;

	state = platform_get_drvdata(to_platform_device(dev));
	if (state == NULL) {
		pr_err("Fail - tcc_isp_state pointer\n");
		ret = -ENODEV;
		goto end;
	}

	if (kstrtouint(buf, NULL, &val)) {
		loge(&(state->pdev->dev), "Fail - read data from attribute\n");
		ret = -EINVAL;
		goto end;
	}

	logi(&(state->pdev->dev), "set mdelay_to_output (%d)\n", val);

	state->mdelay_to_out = val;

end:
	return ret ? ret : count;
}

static DEVICE_ATTR(mdelay_to_output, 0644,
	mdelay_to_output_show, mdelay_to_output_store);

static int tcc_isp_probe(struct platform_device *pdev)
{
	struct tcc_isp_state *state;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	int ret = 0;

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (WARN_ON(state == NULL)) {
		ret = -ENOMEM;
		goto err;
	}
	platform_set_drvdata(pdev, state);

	of_id = of_match_node(tcc_isp_of_match, dev->of_node);
	if (WARN_ON(of_id == NULL)) {
		ret = -EINVAL;
		goto err;
	}
	pdev->id = of_alias_get_id(pdev->dev.of_node, "isp");
	state->pdev = pdev;

	/* Parse device tree */
	ret = tcc_isp_parse_dt(pdev, state);
	if (ret < 0) {
		loge(&(state->pdev->dev), "Fail tcc_isp_parse_dt\n");
		goto err;
	}

	sprintf(state->isp_fw_name, "%s-%d", TCC_ISP_FIRMWARE_NAME, pdev->id);

	tcc_isp_set_default(state);

	v4l2_subdev_init(&(state->sd), &tcc_isp_ops);
	state->sd.owner = pdev->dev.driver->owner;
	state->sd.dev = &pdev->dev;
	state->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;

	v4l2_set_subdevdata(&(state->sd), state);

	/* initialize v4l2 control handler */
	ret = tcc_isp_init_controls(state);
	if (ret)
		goto err;

	/* initialize name */
	sprintf(state->sd.name, "tcc-isp-%d", pdev->id);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&(state->sd));
	if (ret) {
		loge(&(state->pdev->dev),
				"Failed to register subdevice\n");
		goto err_async_register_subdev;
	}
	logi(&(state->pdev->dev),
		"%s is registered as a v4l2 sub device.\n",
		state->sd.name);

	ret = device_create_file(&(state->pdev->dev),
			&dev_attr_mdelay_to_output);
	if (ret < 0) {
		loge(&(state->pdev->dev), "Fail create attribute\n");
		goto err_attr;
	}

	logi(&(state->pdev->dev), "Success proving tcc-isp-%d\n", pdev->id);
	mcu_ctl[pdev->id].isp_state = state;
	mcu_ctl[pdev->id].is_probed = 1;

	goto end;

err_attr:
	v4l2_async_unregister_subdev(&(state->sd));
err_async_register_subdev:
	tcc_isp_exit_controls(state);
err:
	mcu_ctl[pdev->id].isp_state = NULL;
	mcu_ctl[pdev->id].is_probed = 0;
end:
	return ret;
}

static int tcc_isp_remove(struct platform_device *pdev)
{
	struct tcc_isp_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

	device_remove_file(&(state->pdev->dev), &dev_attr_mdelay_to_output);
	tcc_isp_exit_controls(state);

	mcu_ctl[pdev->id].isp_state = NULL;
	mcu_ctl[pdev->id].is_probed = 0;

	logi(&(state->pdev->dev), "%s out\n", __func__);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_isp_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tcc_isp_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);


	return ret;
}

static int tcc_isp_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tcc_isp_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

	tcc_isp_set_default(state);
	state->fw_load = 0;

	return ret;
}
#endif

static const struct dev_pm_ops tcc_isp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_isp_suspend, tcc_isp_resume)
};

static const struct of_device_id tcc_isp_of_match[] = {
	{
		.compatible = "telechips,tcc805x-isp",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tcc_isp_of_match);

static struct platform_driver tcc_isp_driver = {
	.probe = tcc_isp_probe,
	.remove = tcc_isp_remove,
	.driver = {
		.name = TCC_ISP_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tcc_isp_of_match,
		.pm = &tcc_isp_pm_ops,
	},
};
module_platform_driver(tcc_isp_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC ISP driver");
MODULE_LICENSE("GPL");
