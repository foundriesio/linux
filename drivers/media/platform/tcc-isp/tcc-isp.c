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

#include <video/tcc/videosource_ioctl.h>
#include "tcc-isp.h"
#include "tcc-isp-reg.h"
#include "tcc-isp-settings.h"

static struct tcc_isp_state * arr_state[4];

#define SHIFT(x) (((unsigned long)(x) & (0x2)) ? (0) : (16))

static inline volatile void __iomem * ALIGN_32BIT(volatile void __iomem * reg)
{
	return (volatile void __iomem *)((((unsigned long)reg) >> 2) << 2);
}

static inline unsigned short read_isp(volatile void __iomem * reg)
{
	unsigned int val32;

	val32 = __raw_readl(ALIGN_32BIT(reg));
	return (short)(val32 >> SHIFT(reg));
}

static inline void write_isp(volatile void __iomem * reg, unsigned short val16)
{
	unsigned int val32;

	val32 = __raw_readl(ALIGN_32BIT(reg));
	val32 &= ~(0xFFFF << SHIFT(reg));
	val32 |= (((unsigned int)val16) << SHIFT(reg));

	__raw_writel((val32), ALIGN_32BIT(reg));
}

static inline void regw(volatile void __iomem * reg, unsigned long val)
{
	//printk("reg: 0x%x, val: 0x%x \n", (reg), (val));
	__raw_writel((val), (reg));
}

static void tcc_isp_load_firmware(
		struct tcc_isp_state * state, const void * fw, size_t count)
{
	unsigned short val = 0;
	volatile void __iomem * isp_base = state->isp_base;
	volatile void __iomem * mem_base = state->mem_base;

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

	pr_info("[INFO][tcc-isp-%d] copy firmware(%d) \n", 
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
	struct tcc_isp_state * state = (struct tcc_isp_state *)context;


	if (fw == NULL) {
		pr_err("[ERR][tcc-isp] FAIL - firmware loading \n");
		return;
	}


	pr_info("[INFO][tcc-isp] FW size: %d \n", fw->size);

	tcc_isp_load_firmware(state, fw->data, fw->size);

	release_firmware(fw);

	pr_info("[INFO][tcc-isp] success callback (%s) \n", __func__);
}

static int tcc_isp_request_firmware(
		struct tcc_isp_state * state, const char *fw_name)
{
	int ret = 0;

	pr_info("[INFO][tcc-isp] request firmware(%s) \n", fw_name);

	ret = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_HOTPLUG,
		fw_name,
		&(state->pdev->dev),
		GFP_KERNEL,
		state,
		tcc_isp_callback_load_firmware);

	pr_info("[INFO][tcc-isp] %s - ret is %d \n", __func__, ret);

	return ret;
}

static inline void tcc_isp_set_regster_update_mode(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = state->isp_base;
	unsigned short update_sel1, update_sel2;
	unsigned short update_mod1, update_mod2;
	const char * str_update_sel[] = {
		"USER SPECIFIED TIMING",
		"VSYNC FALLING EDGE TIMING",
		"VSYNC RISING EDGE TIMING",
		"WRITING TIMING"
	};
	const char * str_update_mod1[] = {
		"SYNC MODE",
		"ALWAYS MODE"
	};
	const char * str_update_mod2[] = {
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
	pr_info("[INFO][tcc-isp] update_sel1, 2(0x%x, 0x%x) is %s \n",
		update_sel1, update_sel2, 
		str_update_sel[update_sel1 & UPDATE_SEL1_UPDATE_00_SEL_MASK]);

	write_isp(isp_base + reg_update_sel1, update_sel1);
	write_isp(isp_base + reg_update_sel2, update_sel2);

	/*
	 * 0: sync mode
	 * 1: always mode
	 */
	pr_info("[INFO][tcc-isp] update_mod1(0x%x) is %s \n",
		update_mod1,
		str_update_mod1[update_mod1 & UPDATE_MOD1_UPDATE_MODE_0_MASK]);

	write_isp(isp_base + reg_update_mod1, update_mod1);

	/*
	 * 0: individual sync
	 * 1: group sync
	 */
	pr_info("[INFO][tcc-isp] update_mod2(0x%x) is %s \n",
		update_mod2,
		str_update_mod2[update_mod2 & UPDATE_MOD2_VSYNC_00_SEL_MASK]);

	write_isp(isp_base + reg_update_mod2, update_mod2);
}

static inline void tcc_isp_set_wdma(struct tcc_isp_state * state, int onOff)
{
	volatile void __iomem * isp_base = state->isp_base;

	write_isp(isp_base + reg_isp_wdma_ctl0, onOff);
}

static inline void tcc_isp_update_register(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = state->isp_base;

	write_isp(isp_base + reg_update_ctl, 0xFFFF);
}

static inline void tcc_isp_set_input(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = state->isp_base;
	const char * str[] = 
		{"Blue First", "Gb first", "Gr first", "Red first"};
	int w, h, rgb_order;

	w = state->isp->i_state.width;
	h = state->isp->i_state.height;
	rgb_order = state->isp->i_state.rgb_order;

	pr_info("[INFO][tcc-isp] input size(%d x %d) rgb order(%s) \n", 
		w, h, str[rgb_order]);

	/* size */
	write_isp(isp_base + reg_img_size_width, w);
	write_isp(isp_base + reg_img_size_height, h);

	/* bayer rgb order */
	write_isp(isp_base + reg_imgin_order_ctl, rgb_order);
}

static inline void tcc_isp_set_output(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = state->isp_base;
	const char * str[] = 
		{"YUV420", "YUV422", "YUV444", "RGB888"};
	int x, y, w, h, fmt;

	x = state->isp->o_state.x;
	y = state->isp->o_state.y;
	w = state->isp->o_state.width;
	h = state->isp->o_state.height;
	fmt = state->isp->o_state.format;

	pr_info("[INFO][tcc-isp] output crop(%d, %d / %d x %d) format(%s) \n", 
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

static inline void tcc_isp_set_decompanding(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = state->isp_base;
	int i = 0;

	pr_info("[INFO][tcc-isp] dcpd input bit(%d), output bit(%d) \n",
		state->hdr->decompanding_input_bit,
		state->hdr->decompanding_output_bit);

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

	for (i = 0; i < 8; i++) {
		write_isp(isp_base + reg_dcpd_crv_0 + (i * 2),
			state->hdr->dcpd_crv[i]);
		write_isp(isp_base + reg_dcpdx_0 + (i * 2),
			state->hdr->dcpdx[i]);
	}
}

static void tcc_isp_init(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = state->isp_base;

	pr_info("[INFO][tcc-isp] start init ISP \n");

	/* set register update control */
	tcc_isp_set_regster_update_mode(state);

	/* disable wdma */
	tcc_isp_set_wdma(state, OFF);

	/*
	 * OAK setting
	 */
	/* De-companding */
	if (state->hdr->mode == HDR_MODE_COMPANDING) {
		tcc_isp_set_decompanding(state);
	}

	/* 
	 * ZELCOVA setting 
	 */
	/* input */
	tcc_isp_set_input(state);
	/* output */
	tcc_isp_set_output(state);

	/* register update */
	tcc_isp_update_register(state);

	pr_info("[INFO][tcc-isp] finish init ISP \n");
}

static inline void tcc_isp_additional_setting(struct tcc_isp_state * state)
{
	volatile void __iomem * isp_base = \
		state->isp_base;
	int i = 0;

	pr_info("[INFO][tcc-isp] start %s \n", __func__);

	for (i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
		regw(isp_base + settings[i].reg, settings[i].val);
	}

	pr_info("[INFO][tcc-isp] finish %s \n", __func__);
}

void tcc_isp_enable(
		unsigned int idx, 
		videosource_format_t * format, unsigned int enable)
{
	/* input */
	arr_state[idx]->isp->i_state.width = format->width + 16;
	arr_state[idx]->isp->i_state.height = format->height + 16;

	/* output */
	arr_state[idx]->isp->o_state.width = format->width;
	arr_state[idx]->isp->o_state.height = format->height;

	if (enable) {
		tcc_isp_init(arr_state[idx]);
		tcc_isp_additional_setting(arr_state[idx]);
	} else {
		/*
		 * TODO
		 */
	}
}

static int tcc_isp_parse_dt(struct platform_device *pdev,
		struct tcc_isp_state *state)
{
	struct device_node *node = pdev->dev.of_node;
	struct device * dev = &pdev->dev;
	struct resource * mem_res;

	char prop_name[32] = {0, };
	int ret = 0, index = 0;

	/* Get ISP base address */
	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "isp_base");
	state->isp_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR((const void *)state->isp_base))
		return PTR_ERR((const void *)state->isp_base);

	pr_info("[INFO][tcc-isp] isp base addr is %px \n", state->isp_base);

	/* Get mem base address */
	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mem_base");
	state->mem_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR((const void *)state->mem_base))
		return PTR_ERR((const void *)state->mem_base);

	pr_info("[INFO][tcc-isp] mem base addr is %px \n", state->mem_base);

#if 1
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
	if (IS_ERR(state->uart_pinctrl)) 
		return PTR_ERR((const void *)state->uart_pinctrl);
#endif

err:
	of_node_put(node);

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

static int tcc_isp_probe(struct platform_device *pdev)
{
	struct tcc_isp_state * state;
	const struct of_device_id * of_id;
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("[INFO][tcc-isp] %s in \n", __func__);

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (WARN_ON(state == NULL)) {
		ret = -ENOMEM;
		goto err;
	}
	state->pdev = pdev;
	
	of_id = of_match_node(tcc_isp_of_match, dev->of_node);
	if (WARN_ON(of_id == NULL)) {
		ret = -EINVAL;
		goto err;
	}
	pdev->id = of_alias_get_id(pdev->dev.of_node, "isp");
	arr_state[pdev->id] = state;

	/*
	 * Parse device tree
	 */
	ret = tcc_isp_parse_dt(pdev, state);
	if (ret < 0) {
		goto err;
	}

	state->hdr = &setting_hdr;
	state->isp = &setting_isp;

	ret = tcc_isp_request_firmware(state, TCC_ISP_FIRMWARE_NAME);
	if (ret < 0) {
		pr_err("[ERR][tcc-isp] FAIL - loading firmware(%s) \n", 
			TCC_ISP_FIRMWARE_NAME);
	}

	pr_info("[INFO][tcc-isp] Success proving tcc-isp-%d \n", pdev->id);

err:
end:
	pr_info("[INFO][tcc-isp] %s out \n", __func__);

	return ret;
}

static int tcc_isp_remove(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[INFO][tcc-isp] %s in \n", __func__);

	pr_info("[INFO][tcc-isp] %s out \n", __func__);

	return ret;
}

static struct platform_driver tcc_isp_driver = {
	.probe = tcc_isp_probe,
	.remove = tcc_isp_remove,
	.driver = {
		.of_match_table = tcc_isp_of_match,
		.name = TCC_ISP_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};
module_platform_driver(tcc_isp_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC ISP driver");
MODULE_LICENSE("GPL");
