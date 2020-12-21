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

#define SHIFT(x)	\
	((((unsigned long)(x) & (REG_HIGH_LOW)) == REG_HIGH) ? (16) : (0))

struct v4l2_dv_timings tcc_isp_dv_timings = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.width = DEFAULT_WIDTH,
		.height = DEFAULT_HEIGHT,
		.interlaced = V4L2_DV_PROGRESSIVE,
		.polarities = V4L2_DV_VSYNC_POS_POL,
	},
};

struct v4l2_mbus_config isp_mbus_config = {
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
static inline struct tcc_isp_state *sd_to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct tcc_isp_state, sd);
}

static inline volatile void __iomem *ALIGN_32BIT(volatile void __iomem *reg)
{
	return (volatile void __iomem *)((((unsigned long)reg) >> 2) << 2);
}

static inline unsigned short read_isp(volatile void __iomem *reg)
{
	unsigned int val32;
	volatile void __iomem *alligned_addr = ALIGN_32BIT(reg);

	val32 = __raw_readl(alligned_addr);
	return (short)(val32 >> SHIFT(reg));
}

static inline void write_isp(volatile void __iomem *reg, unsigned short val16)
{
	unsigned int val32;
	volatile void __iomem *alligned_addr = ALIGN_32BIT(reg);

	val32 = __raw_readl(alligned_addr);
	val32 &= ~(0xFFFF << SHIFT(reg));
	val32 |= (((unsigned int)val16) << SHIFT(reg));

	pr_debug("[DEBUG][tcc-isp] reg: 0x%p, val: 0x%d\n",
		alligned_addr, val32);
	__raw_writel(val32, alligned_addr);
}

static inline void regw(volatile void __iomem *reg, unsigned long val)
{
	pr_debug("[DEBUG][tcc-isp] reg: 0x%p, val: 0x%ld\n",
		reg, val);
	__raw_writel(val, reg);
}

static s32 tcc_isp_rgb_order(u32 rgb_order)
{
	s32 tcc_order;

	switch (rgb_order) {
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		tcc_order = IMGIN_ORDER_CTL_B_FIRST;
		break;
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		tcc_order = IMGIN_ORDER_CTL_GB_FIRST;
		break;
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		tcc_order = IMGIN_ORDER_CTL_GR_FIRST;
		break;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		tcc_order = IMGIN_ORDER_CTL_R_FIRST;
		break;
	default:
		tcc_order = -1;
	}

	return tcc_order;
}

static void tcc_isp_load_firmware(
		struct tcc_isp_state *state, const void *fw, size_t count)
{
	unsigned short val = 0;
	volatile void __iomem *isp_base = state->isp_base;
	void __iomem *mem_base = state->mem_base;

	/* MSP430 disable */
	val = read_isp(isp_base + reg_msp430_ctl);
	val &= ~(MSP430_CTL_MSP430_ENABLE_MASK <<
		MSP430_CTL_MSP430_ENABLE_SHIFT);
	val |= (OFF << MSP430_CTL_MSP430_ENABLE_SHIFT);
	write_isp(isp_base + reg_msp430_ctl, val);

	/* MSP430 memory download enable */
	val = read_isp(isp_base + reg_msp430_mem_ctl);
	val &= ~MSP430_MEM_CTL_DOWNLOAD_ENABLE_MASK;
	val |= (ON << MSP430_MEM_CTL_DOWNLOAD_ENABLE_SHIFT);
	write_isp(isp_base + reg_msp430_mem_ctl, val);

	pr_info("[INFO][tcc-isp-%d] copy firmware(%ld)\n",
		state->pdev->id, count);
	/* copy firmware to the msp430 code memory */
	memcpy(mem_base + ISP_MEM_OFFSET_CODE_MEM, fw, count);

	mb();

	/* MSP430 memory download disable */
	val = read_isp(isp_base + reg_msp430_mem_ctl);
	val &= ~(MSP430_MEM_CTL_DOWNLOAD_ENABLE_MASK <<
		MSP430_MEM_CTL_DOWNLOAD_ENABLE_SHIFT);
	val |= (OFF << MSP430_MEM_CTL_DOWNLOAD_ENABLE_SHIFT);
	write_isp(isp_base + reg_msp430_mem_ctl, val);

	/* MSP430 enable */
	val = read_isp(isp_base + reg_msp430_ctl);
	val &= ~(MSP430_CTL_MSP430_ENABLE_MASK <<
		MSP430_CTL_MSP430_ENABLE_SHIFT);
	val |= (ON << MSP430_CTL_MSP430_ENABLE_SHIFT);
	write_isp(isp_base + reg_msp430_ctl, val);
}

static void tcc_isp_callback_load_firmware(
		const struct firmware *fw, void *context)
{
	struct tcc_isp_state *state = (struct tcc_isp_state *)context;


	if (fw == NULL) {
		pr_warn("[WARN][tcc-isp] Timeout - firmware loading\n");
		return;
	}


	pr_info("[INFO][tcc-isp] FW size: %ld\n", fw->size);

	tcc_isp_load_firmware(state, fw->data, fw->size);

	release_firmware(fw);

	pr_info("[INFO][tcc-isp] success callback (%s)\n", __func__);
}

static int tcc_isp_request_firmware(
		struct tcc_isp_state *state, const char *fw_name)
{
	int ret = 0;

	pr_info("[INFO][tcc-isp] request firmware(%s)\n", fw_name);

	ret = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_HOTPLUG,
		fw_name,
		&(state->pdev->dev),
		GFP_KERNEL,
		state,
		tcc_isp_callback_load_firmware);

	pr_info("[INFO][tcc-isp] %s - ret is %d\n", __func__, ret);

	return ret;
}

static inline void tcc_isp_mem_share(struct tcc_isp_state *state, int onOff)
{
	volatile void __iomem *isp_base = state->isp_base;

	write_isp(isp_base + reg_mem_swt_en, onOff);
}

static inline void tcc_isp_set_regster_update_mode(struct tcc_isp_state *state)
{
	volatile void __iomem *isp_base = state->isp_base;
	unsigned short update_sel1, update_sel2;
	unsigned short update_mod1, update_mod2;
	static const char * const str_update_sel[] = {
		"USER SPECIFIED TIMING",
		"VSYNC FALLING EDGE TIMING",
		"VSYNC RISING EDGE TIMING",
		"WRITING TIMING"
	};
	static const char * const str_update_mod1[] = {
		"SYNC MODE",
		"ALWAYS MODE"
	};
	static const char * const str_update_mod2[] = {
		"INDIVIDUAL SYNC",
		"GROUP SYNC"
	};

	update_sel1 = update_sel2 =
		UPDATE_SEL_ALL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING;

	update_mod1 = UPDATE_MOD1_ALL_ALWAYS_MODE;
	update_mod2 = UPDATE_MOD2_ALL_GROUP_SYNC;

	write_isp(isp_base + reg_update_user_cnt1, 0x0);
	write_isp(isp_base + reg_update_user_cnt2, 0x0300);

	/*
	 * 0: sync on user specified timing
	 * 1: sync on vsync falling edge timing
	 * 2: sync on vsync rising edge timing
	 * 3: sync on writing timing
	 */
	pr_info("[INFO][tcc-isp] update_sel1, 2(0x%x, 0x%x) is %s\n",
		update_sel1, update_sel2,
		str_update_sel[update_sel1 & UPDATE_SEL1_UPDATE_00_SEL_MASK]);

	write_isp(isp_base + reg_update_sel1, update_sel1);
	write_isp(isp_base + reg_update_sel2, update_sel2);

	/*
	 * 0: sync mode
	 * 1: always mode
	 */
	pr_info("[INFO][tcc-isp] update_mod1(0x%x) is %s\n",
		update_mod1,
		str_update_mod1[update_mod1 & UPDATE_MOD1_UPDATE_MODE_0_MASK]);

	write_isp(isp_base + reg_update_mod1, update_mod1);

	/*
	 * 0: individual sync
	 * 1: group sync
	 */
	pr_info("[INFO][tcc-isp] update_mod2(0x%x) is %s\n",
		update_mod2,
		str_update_mod2[update_mod2 & UPDATE_MOD2_VSYNC_00_SEL_MASK]);

	write_isp(isp_base + reg_update_mod2, update_mod2);
}

static inline void tcc_isp_set_wdma(struct tcc_isp_state *state, int onOff)
{
	volatile void __iomem *isp_base = state->isp_base;

	write_isp(isp_base + reg_isp_wdma_ctl0, onOff);

	if (onOff) {

	} else {
		/* CV8050C-810 */
		write_isp(isp_base + reg_isp_wdma_cfg0, 0x0);
	}
}

static inline void tcc_isp_update_register(struct tcc_isp_state *state)
{
	volatile void __iomem *isp_base = state->isp_base;

	write_isp(isp_base + reg_update_ctl, 0xFFFF);
}

static inline void tcc_isp_set_input(struct tcc_isp_state *state)
{
	volatile void __iomem *isp_base = state->isp_base;
	static const char * const str[] = {
		"Blue First", "Gb first", "Gr first", "Red first"};
	int w, h, rgb_order;

	w = state->isp.i_state.width;
	h = state->isp.i_state.height;
	rgb_order = state->isp.i_state.rgb_order;

	pr_info("[INFO][tcc-isp] input size(%d x %d) rgb order(%s)\n",
		w, h, str[rgb_order]);

	/* size */
	write_isp(isp_base + reg_img_size_width, w);
	write_isp(isp_base + reg_img_size_height, h);

	/* bayer rgb order */
	write_isp(isp_base + reg_imgin_order_ctl, rgb_order);
}

static inline void tcc_isp_set_output(struct tcc_isp_state *state)
{
	volatile void __iomem *isp_base = state->isp_base;
	static const char * const str[] = {
		"YUV420", "YUV422", "YUV444", "RGB888"};
	int x, y, w, h, fmt;

	x = state->isp.o_state.x;
	y = state->isp.o_state.y;
	w = state->isp.o_state.width;
	h = state->isp.o_state.height;
	fmt = state->isp.o_state.format;

	pr_info("[INFO][tcc-isp] output crop(%d, %d / %d x %d) format(%s)\n",
		x, y, w, h, str[fmt]);

	/* window enable */
	write_isp(isp_base + reg_imgout_window_ctl,
		((IMGOUT_WINDOW_CTL_ISP_OUTPUT_WINDOW_EN <<
		 IMGOUT_WINDOW_CTL_ISP_OUTPUT_WINDOW_SHIFT) |
		(IMGOUT_WINDOW_CTL_DEBLANK_EN <<
		 IMGOUT_WINDOW_CTL_DEBLANK_SHIFT)));

	/* format */
	switch (fmt) {
	case IMGOUT_WINDOW_CFG_FORMAT_YUV444:
		break;
	case IMGOUT_WINDOW_CFG_FORMAT_RGB888:
		break;
	case IMGOUT_WINDOW_CFG_FORMAT_YUV422:
	default:
		fmt &= ~(IMGOUT_WINDOW_CFG_DATA_ORDER_MASK <<
			 IMGOUT_WINDOW_CFG_DATA_ORDER_SHIFT);
		fmt |= (IMGOUT_WINDOW_CFG_DATA_ORDER_P0P2P1 <<
			IMGOUT_WINDOW_CFG_DATA_ORDER_SHIFT);
		break;
	}
	write_isp(isp_base + reg_imgout_window_cfg, fmt);

	/* crop */
	write_isp(isp_base + reg_imgout_window_x_start, x);
	write_isp(isp_base + reg_imgout_window_y_start, y);
	write_isp(isp_base + reg_imgout_window_x_width,	w);
	write_isp(isp_base + reg_imgout_window_y_width,	h);
}

static inline void tcc_isp_set_decompanding(struct tcc_isp_state *state)
{
	volatile void __iomem *isp_base = state->isp_base;
	static const char * const str_dcpd_input_bit[] = {
		"10bit", "12bit"};
	static const char * const str_dcpd_output_bit[] = {
		"10bit", "12bit", "14bit", "15bit",
		"16bit", "17bit", "20bit"};
	int i = 0;

	pr_info("[INFO][tcc-isp] dcpd input %s, output %s\n",
		str_dcpd_input_bit[state->hdr->decompanding_input_bit],
		str_dcpd_output_bit[state->hdr->decompanding_output_bit]);

	write_isp(isp_base + reg_dcpd_ctl,
		((state->hdr->decompanding_curve_maxval <<
		  DCPD_CTL_DCPD_CURVE_MAXVAL_NO_SHIFT) |
		 (state->hdr->decompanding_output_bit <<
		  DCPD_CTL_OUTPUT_BIT_SELECTION_SHIFT) |
		 (state->hdr->decompanding_input_bit <<
		  DCPD_CTL_INPUT_BIT_SELECTION_SHIFT) |
		 (ON <<
		  DCPD_CTL_DCPD_ON_OFF_SHIFT))
	);

	pr_info("[INFO][tcc-isp] setting dcpd curve\n");
	for (i = 0; i < 8; i++) {
		write_isp(isp_base + reg_dcpd_crv_0 + (i * 2),
			state->hdr->dcpd_crv[i]);
	}
	pr_info("[INFO][tcc-isp] setting dcpd curve axis\n");
	for (i = 0; i < 8; i++) {
		write_isp(isp_base + reg_dcpdx_0 + (i * 2),
			state->hdr->dcpdx[i]);
	}
}

static void tcc_isp_init_local(struct tcc_isp_state *state)
{
	pr_info("[INFO][tcc-isp] start init ISP\n");

	/* set register update control */
	tcc_isp_set_regster_update_mode(state);

	/* memory sharing */
	tcc_isp_mem_share(state, state->mem_share);

	/* disable wdma */
	tcc_isp_set_wdma(state, OFF);

	/*
	 * OAK setting
	 */
	/* De-companding */
	if (state->hdr->mode == HDR_MODE_COMPANDING)
		tcc_isp_set_decompanding(state);

	/*
	 * ZELCOVA setting
	 */
	/* input */
	tcc_isp_set_input(state);
	/* output */
	tcc_isp_set_output(state);

	/* register update */
	tcc_isp_update_register(state);

	pr_info("[INFO][tcc-isp] finish init ISP\n");
}

static inline void tcc_isp_additional_setting(struct tcc_isp_state *state)
{
	volatile void __iomem *isp_base = state->isp_base;
	int i = 0;

	pr_info("[INFO][tcc-isp] start %s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(settings); i++)
		regw(isp_base + settings[i].reg, settings[i].val);

	pr_info("[INFO][tcc-isp] finish %s\n", __func__);
}

void tcc_isp_enable(struct tcc_isp_state *state, unsigned int enable)
{
	pr_info("[INFO][tcc-isp] start %s\n", __func__);

	if (enable) {
		tcc_isp_init_local(state);
		/* tcc_isp_additional_setting(state); */
	} else {
		/*
		 * TODO
		 */
	}
	pr_info("[INFO][tcc-isp] end %s\n", __func__);
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

	pr_info("[INFO][tcc-isp] isp base addr is %px\n", state->isp_base);

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
	pr_info("[INFO][tcc-isp] mem base addr is %px\n", state->mem_base);

#if 0
	/*
	 * Get UART pinctrl
	 *
	 * TCC8059 does not have an ball out of ISP UART.
	 * TCC8050 has an ball out of ISP UART. Refer to the GPIO pins below.
	 * UART TX - GPIO_MB[0] / GPIO_MB[6] / GPIO_MB[12] / GPIO_MB[24]
	 * UART RX - GPIO_MB[1] / GPIO_MB[7] / GPIO_MB[13] / GPIO_MB[25]
	 * But, GPIO pins above is used for other purpose.
	 * So, H/W modification is needed to use ISP UART on the TCC8050 EVB.
	 * Refer to the TCS(CV8050C-606)
	 */
	state->uart_pinctrl = devm_pinctrl_get_select_default(&(pdev->dev));
	if (IS_ERR(state->uart_pinctrl)) {
		ret = PTR_ERR((const void *)state->uart_pinctrl);
		loge(&(state->pdev->dev), "Fail parsing uart pinctrl\n");
		goto err;
	}
#endif

	of_property_read_u32_index(node,
		"mem_share", 0, &(state->mem_share));
	logi(&(state->pdev->dev), "mem_share %d\n", state->mem_share);
err:

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_isp_s_power(struct v4l2_subdev *sd, int on)
{
	struct tcc_isp_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	logi(&(state->pdev->dev), "call\n");

	return ret;
}
static int tcc_isp_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tcc_isp_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	logi(&(state->pdev->dev), "call\n");

	tcc_isp_enable(state, enable);

	return ret;
}

static void tcc_isp_set_default(struct tcc_isp_state *state)
{
	state->hdr = &setting_hdr;

	/*
	 * Because of ISP algorithm characteristics,
	 * isp output resolution is small than input resolution
	 * (top, bottom, left, top 8 lines will be reduced)
	 */
	state->isp.o_state.x = 8;
	state->isp.o_state.y = 8;

	state->isp.o_state.format = IMGOUT_WINDOW_CFG_FORMAT_YUV422;
	state->isp.o_state.data_order = IMGOUT_WINDOW_CFG_DATA_ORDER_P0P2P1;

	/*
	 * set axi bus output disable
	 */
	/* set register update control */
	tcc_isp_set_regster_update_mode(state);
	/* disable wdma */
	tcc_isp_set_wdma(state, OFF);
	/* register update */
	tcc_isp_update_register(state);
}

static int tcc_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	int ret = 0;

	logi(&(state->pdev->dev), "in\n");

	logi(&(state->pdev->dev), "out\n");

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
	struct tcc_isp_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	logi(&(state->pdev->dev), "%s call\n", __func__);

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
	int i = 0;

	logi(&(state->pdev->dev), "call\n");

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

	logi(&(state->pdev->dev), "call\n");

	memcpy((void *)&state->fmt,
		(const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	state->isp.i_state.width = state->fmt.width;
	state->isp.i_state.height = state->fmt.height;

	ret = tcc_isp_rgb_order(state->fmt.code);
	if (ret < 0) {
		loge(&(state->pdev->dev),
			"RGB order(0x%x) is not supported\n", state->fmt.code);
		goto err;
	}

	state->isp.i_state.rgb_order = ret;

	logi(&(state->pdev->dev),
		"RGB order(0x%x, 0x%x)\n",
		state->fmt.code, state->isp.i_state.rgb_order);

	/*
	 * Because of ISP algorithm characteristics,
	 * isp output resolution is small than input resolution
	 * (top, bottom, left, top 8 lines will be reduced)
	 */
	state->fmt.width -= 16;
	state->fmt.height -= 16;

	state->isp.o_state.width = state->fmt.width;
	state->isp.o_state.height = state->fmt.height;

	tcc_isp_dv_timings.bt.width = state->fmt.width;
	tcc_isp_dv_timings.bt.height = state->fmt.height;

	switch (state->isp.o_state.format) {
	case IMGOUT_WINDOW_CFG_FORMAT_YUV422:
		state->fmt.code = MEDIA_BUS_FMT_UYVY8_1X16;
		break;
	case IMGOUT_WINDOW_CFG_FORMAT_YUV444:
		state->fmt.code = MEDIA_BUS_FMT_YUV8_1X24;
		break;
	case IMGOUT_WINDOW_CFG_FORMAT_RGB888:
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

static int tcc_isp_probe(struct platform_device *pdev)
{
	struct tcc_isp_state *state;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("[INFO][tcc-isp] %s in\n", __func__);

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

	/*
	 * Parse device tree
	 */
	ret = tcc_isp_parse_dt(pdev, state);
	if (ret < 0) {
		loge(&(state->pdev->dev), "Fail tcc_isp_parse_dt\n");
		goto err;
	}

	sprintf(state->isp_fw_name, "%s-%d", TCC_ISP_FIRMWARE_NAME, pdev->id);

	tcc_isp_set_default(state);

	ret = tcc_isp_request_firmware(state, state->isp_fw_name);
	if (ret < 0) {
		pr_err("[ERR][tcc-isp] FAIL - loading firmware(%s)\n",
			TCC_ISP_FIRMWARE_NAME);
	}

	v4l2_subdev_init(&(state->sd), &tcc_isp_ops);
	state->sd.owner = pdev->dev.driver->owner;
	state->sd.dev = &pdev->dev;
	v4l2_set_subdevdata(&(state->sd), state);
	/* initialize name */
	sprintf(state->sd.name, "tcc-isp-%d", pdev->id);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&(state->sd));
	if (ret) {
		loge(&(state->pdev->dev),
				"Failed to register subdevice\n");
		goto err;
	}
	logi(&(state->pdev->dev),
		"%s is registered as a v4l2 sub device.\n",
		state->sd.name);

	pr_info("[INFO][tcc-isp] Success proving tcc-isp-%d\n", pdev->id);
	goto end;
err:
end:
	pr_info("[INFO][tcc-isp] %s out\n", __func__);

	return ret;
}

static int tcc_isp_remove(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[INFO][tcc-isp] %s in\n", __func__);

	pr_info("[INFO][tcc-isp] %s out\n", __func__);

	return ret;
}

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
	},
};
module_platform_driver(tcc_isp_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC ISP driver");
MODULE_LICENSE("GPL");
