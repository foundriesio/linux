/*
 * Hisilicon Hi6220 SoC ADE(Advanced Display Engine)'s crtc&plane driver
 *
 * Copyright (c) 2014-2015 Hisilicon Limited.
 * Author:
 * 	Xinliang Liu <xinliang.liu@linaro.org>
 * 	Xinliang Liu <z.liuxinliang@hisilicon.com>
 * 	Xinwei Kong <kong.kongxinwei@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <video/display_timing.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#include "hisi_drm_drv.h"
#include "hisi_ade_reg.h"

#define FORCE_PIXEL_CLOCK_SAME_OR_HIGHER 0

#define to_ade_crtc(crtc) \
	container_of(crtc, struct ade_crtc, base)
#define to_ade_crtc_state(state) \
	container_of(state, struct ade_crtc_state, base)

enum composotion_type {
	COMPOSITION_UNKNOWN	= 0,
	COMPOSITION_GLES	= 1,
	COMPOSITION_HWC		= 2,
	COMPOSITION_MIXED	= 3
};

struct ade_hw_ctx {
	void __iomem  *base;
	void __iomem  *media_base;

	int irq;
	u32 ade_core_rate;
	u32 media_noc_rate;

	struct clk *ade_core_clk;
	struct clk *media_noc_clk;
	struct clk *ade_pix_clk;
	bool power_on;
};

struct ade_crtc_state {
	struct drm_crtc_state base;
	u8 comp_type;
};

struct ade_crtc {
	struct drm_crtc base;
	struct ade_hw_ctx *ctx;
	struct drm_display_mode *dmode;
	bool enable;
	u64 use_mask;
};

struct ade_data {
	struct ade_crtc acrtc;
	struct ade_hw_ctx ctx;
};

static void ade_init(struct ade_hw_ctx *ctx)
{
	void __iomem *base = ctx->base;

	/* enable clk gate */
	set_TOP_CTL_clk_gate_en(base, 1);
	/* clear overlay */
	writel(0, base + ADE_OVLY1_TRANS_CFG);
	writel(0, base + ADE_OVLY_CTL);
	writel(0, base + ADE_OVLYX_CTL(ADE_OVLY2));
	/* clear reset and reload regs */
	writel(0, base + ADE_SOFT_RST_SEL0);
	writel(0, base + ADE_SOFT_RST_SEL1);
	writel(0xFFFFFFFF, base + ADE_RELOAD_DIS0);
	writel(0xFFFFFFFF, base + ADE_RELOAD_DIS1);
	/* for video set to 1, means that ade registers
	 * became effective at frame end */
	set_TOP_CTL_frm_end_start(base, 1);
}

static void ade_ldi_set_mode(struct ade_hw_ctx *ctx, struct drm_display_mode *mode)
{
	void __iomem *base = ctx->base;
	u32 hfp, hbp, hsw, vfp, vbp, vsw;
	u32 plr_flags;

	plr_flags = (mode->flags & DRM_MODE_FLAG_NVSYNC)
			? HISI_LDI_FLAG_NVSYNC : 0;
	plr_flags |= (mode->flags & DRM_MODE_FLAG_NHSYNC)
			? HISI_LDI_FLAG_NHSYNC : 0;
	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	hsw = mode->hsync_end - mode->hsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;
	vsw = mode->vsync_end - mode->vsync_start;
	if (vsw > 15) {
		pr_err("%s: vsw exceeded 15\n", __func__);
		vsw = 15;
	}

	writel((hbp << 20) | (hfp << 0), base + LDI_HRZ_CTRL0);
	/* p3-73 6220V100 pdf:
	 *  "The configured value is the actual width - 1"
	 */
	writel(hsw - 1, base + LDI_HRZ_CTRL1);
	writel((vbp << 20) | (vfp << 0), base + LDI_VRT_CTRL0);
	/* p3-74 6220V100 pdf:
	 *  "The configured value is the actual width - 1"
	 */
	writel(vsw - 1, base + LDI_VRT_CTRL1);

	/* p3-75 6220V100 pdf:
	 *  "The configured value is the actual width - 1"
	 */
	writel(((mode->vdisplay - 1) << 20) | ((mode->hdisplay - 1) << 0),
	       base + LDI_DSP_SIZE);
	writel(plr_flags, base + LDI_PLR_CTRL);

	/*
	 * other parameters setting
	 */
	writel(BIT(0), base + LDI_WORK_MODE);
	writel((0x3c << 6) | (ADE_OUT_RGB_888 << 3) | BIT(2) | BIT(0),
	       base + LDI_CTRL);
	set_reg(base + LDI_DE_SPACE_LOW, 0x1, 1, 1);
}

static int ade_power_up(struct ade_hw_ctx *ctx)
{
	void __iomem *media_base = ctx->media_base;
	int ret;

	ret = clk_set_rate(ctx->ade_core_clk, ctx->ade_core_rate);
	if (ret) {
		DRM_ERROR("clk_set_rate ade_core_rate error\n");
		return ret;
	}
	ret = clk_set_rate(ctx->media_noc_clk, ctx->media_noc_rate);
	if (ret) {
		DRM_ERROR("media_noc_clk media_noc_rate error\n");
		return ret;
	}
	ret = clk_prepare_enable(ctx->media_noc_clk);
	if (ret) {
		DRM_ERROR("fail to clk_prepare_enable media_noc_clk\n");
		return ret;
	}

	writel(0x20, media_base + SC_MEDIA_RSTDIS);

	ret = clk_prepare_enable(ctx->ade_core_clk);
	if (ret) {
		DRM_ERROR("fail to clk_prepare_enable ade_core_clk\n");
		return ret;
	}

	ade_init(ctx);
	ctx->power_on = true;
	return 0;
}

static void ade_power_down(struct ade_hw_ctx *ctx)
{
	void __iomem *base = ctx->base;
	void __iomem *media_base = ctx->media_base;

	set_LDI_CTRL_ldi_en(base, ADE_DISABLE);
	/* dsi pixel off */
	set_reg(base + LDI_HDMI_DSI_GT, 0x1, 1, 0);

	clk_disable_unprepare(ctx->ade_core_clk);
	writel(0x20, media_base + SC_MEDIA_RSTEN);
	clk_disable_unprepare(ctx->media_noc_clk);
	ctx->power_on = false;
}



/*
 * set modules' reset mode: by software or hardware
 * set modules' reload enable/disable
 * */
static void ade_set_reset_and_reload(struct ade_crtc *acrtc)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 mask0 = (u32)acrtc->use_mask;
	u32 mask1 = (u32)(acrtc->use_mask >> 32);

	DRM_DEBUG_DRIVER("mask=0x%llX, mask0=0x%X, mask1=0x%X\n",
			acrtc->use_mask, mask0, mask1);

	writel(mask0, base + ADE_SOFT_RST_SEL0);
	writel(mask1, base + ADE_SOFT_RST_SEL1);
	writel(~mask0, base + ADE_RELOAD_DIS0);
	writel(~mask1, base + ADE_RELOAD_DIS1);
}

/*
 * commit to ldi to display
 */
static void ade_display_commit(struct ade_crtc *acrtc)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 out_w = acrtc->dmode->hdisplay;
	u32 out_h = acrtc->dmode->vdisplay;

	/* TODO: set rotator after overlay */

	/* TODO: set scale after overlay */

	/* display source setting */
	writel(TOP_DISP_SRC_OVLY2, base + ADE_DISP_SRC_CFG);

	/* ctran6 setting */
	writel(1, base + ADE_CTRAN_DIS(ADE_CTRAN6));
	writel(out_w * out_h - 1, base + ADE_CTRAN_IMAGE_SIZE(ADE_CTRAN6));
	acrtc->use_mask |= BIT(ADE_CTRAN_BIT_OFST + ADE_CTRAN6);

	/* set reset mode:soft or hw, and reload modules */
	ade_set_reset_and_reload(acrtc);

	DRM_INFO("ADE GO: %dx%d \n", out_w, out_h);
	/* enable ade */
	wmb();
	writel(ADE_ENABLE, base + ADE_EN);
	wmb();
	set_LDI_CTRL_ldi_en(base, ADE_ENABLE);
	/* dsi pixel on */
	set_reg(base + LDI_HDMI_DSI_GT, 0x0, 1, 0);
}

static void ade_crtc_enable(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	int ret;

	if (acrtc->enable)
		return;

	if (!ctx->power_on) {
		ret = ade_power_up(ctx);
		if (ret) {
			DRM_ERROR("failed to initialize ade clk\n");
			return ;
		}
	}

	ade_display_commit(acrtc);
	acrtc->enable = true;

	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_crtc_disable(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;

	DRM_DEBUG_DRIVER("enter.\n");

	if (!acrtc->enable)
		return;

	ade_power_down(ctx);
	acrtc->use_mask = 0;
	acrtc->enable = false;
	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_crtc_mode_prepare(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;

	DRM_DEBUG_DRIVER("enter.\n");
	if (!ctx->power_on)
		(void) ade_power_up(ctx);
	DRM_DEBUG_DRIVER("exit success.\n");
}

static bool ade_crtc_mode_fixup(struct drm_crtc *crtc,
			 const struct drm_display_mode *mode,
			 struct drm_display_mode *adj_mode)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	u32 clock_kHz = mode->clock;
	int ret;

	DRM_DEBUG_DRIVER("enter.\n");

	if (!ctx->power_on)
		if (ade_power_up(ctx))
			DRM_ERROR("%s: failed to power up ade\n", __func__);

	do {
		ret = clk_set_rate(ctx->ade_pix_clk, clock_kHz * 1000);
		if (ret) {
			DRM_ERROR("set ade_pixel_clk_rate fail\n");
			return false;
		}
		adj_mode->clock = clk_get_rate(ctx->ade_pix_clk) / 1000;
#if FORCE_PIXEL_CLOCK_SAME_OR_HIGHER
		if (adj_mode->clock >= clock_kHz)
#endif
		/* This avoids a bad 720p DSI clock with 1.2GHz DPI PLL */
		if (adj_mode->clock != 72000)
			break;

		clock_kHz += 10;
	} while (1);

	pr_info("%s: pixel clock: req %dkHz -> actual: %dkHz\n",
		__func__, mode->clock, adj_mode->clock);

	DRM_DEBUG_DRIVER("mode_fixup  exit successfully.\n");
	return true;
}

static void ade_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;

	DRM_DEBUG_DRIVER("enter.\n");
	acrtc->dmode = &acrtc->base.state->mode;
	ade_ldi_set_mode(ctx, &acrtc->base.state->mode);
	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_crtc_atomic_begin(struct drm_crtc *crtc,
			   struct drm_crtc_state *old_state)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;

	DRM_DEBUG_DRIVER("enter.\n");
	if (!ctx->power_on)
		(void) ade_power_up(ctx);
	DRM_DEBUG_DRIVER("exit success.\n");
}
static void ade_crtc_atomic_flush(struct drm_crtc *crtc,
			   struct drm_crtc_state *old_state)

{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;

	DRM_DEBUG_DRIVER("enter.\n");
	/* commit to  display: LDI input setting */
	if (acrtc->enable) {
		/* set reset and reload */
		ade_set_reset_and_reload(acrtc);
		/* flush ade regitsters */
		wmb();
		writel(ADE_ENABLE, base + ADE_EN);
	}
	DRM_DEBUG_DRIVER("exit success.\n");
}

static const struct drm_crtc_helper_funcs ade_crtc_helper_funcs = {
	.enable		= ade_crtc_enable,
	.disable	= ade_crtc_disable,
	.prepare	= ade_crtc_mode_prepare,
	.mode_fixup	= ade_crtc_mode_fixup,
	.mode_set_nofb	= ade_crtc_mode_set_nofb,
	.atomic_begin	= ade_crtc_atomic_begin,
	.atomic_flush	= ade_crtc_atomic_flush,
};

static void ade_crtc_atomic_reset(struct drm_crtc *crtc)
{
	struct ade_crtc_state *state;

	if (crtc->state)
		kfree(to_ade_crtc_state(crtc->state));

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL)
		return;

	/* set to default value */
	state->comp_type = COMPOSITION_UNKNOWN;

	crtc->state = &state->base;
	crtc->state->crtc = crtc;
}

static struct drm_crtc_state *ade_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct ade_crtc_state *state;
	struct ade_crtc_state *copy;

	if (WARN_ON(!crtc->state))
		return NULL;

	state = to_ade_crtc_state(crtc->state);
	copy = kmemdup(state, sizeof(*state), GFP_KERNEL);
	if (copy == NULL)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &copy->base);

	return &copy->base;
}

static void ade_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					  struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(crtc, state);
	kfree(to_ade_crtc_state(state));
}

static int ade_crtc_atomic_set_property(struct drm_crtc *crtc,
					 struct drm_crtc_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct hisi_drm_private *priv = crtc->dev->dev_private;
	struct ade_crtc_state *astate = to_ade_crtc_state(state);

	DRM_DEBUG_DRIVER("\"%s\": val=%d\n", property->name, (int)val);

	if (property == priv->comp_type_prop)
		astate->comp_type = val;
	else
		return -EINVAL;

	return 0;
}

static int ade_crtc_atomic_get_property(struct drm_crtc *crtc,
					 const struct drm_crtc_state *state,
					 struct drm_property *property,
					 uint64_t *val)
{
	struct hisi_drm_private *priv = crtc->dev->dev_private;
	struct ade_crtc_state *astate = to_ade_crtc_state(state);

	if (property == priv->comp_type_prop)
		*val =	astate->comp_type;
	else
		return -EINVAL;

	return 0;
}

static const struct drm_crtc_funcs ade_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= ade_crtc_atomic_reset,
	.set_property = drm_atomic_helper_crtc_set_property,
	.atomic_duplicate_state	= ade_crtc_atomic_duplicate_state,
	.atomic_destroy_state	= ade_crtc_atomic_destroy_state,
	.atomic_set_property = ade_crtc_atomic_set_property,
	.atomic_get_property = ade_crtc_atomic_get_property,
};

static const struct drm_prop_enum_list ade_composition_type_enum_list[] = {
	{ COMPOSITION_UNKNOWN, "composition unknown" },
	{ COMPOSITION_GLES, "composition GLES" },
	{ COMPOSITION_HWC, "composition hwc" },
	{ COMPOSITION_MIXED, "composition mixed" }
};

int ade_install_crtc_properties(struct drm_device *dev,
				struct drm_mode_object *obj)
{
	struct hisi_drm_private *priv = dev->dev_private;
	struct drm_property *prop;

	/*
	 * create and attach composition type properties
	 */
	prop = priv->comp_type_prop;
	if (!prop) {
		prop = drm_property_create_enum(dev, 0,	"comp_type",
				ade_composition_type_enum_list,
				ARRAY_SIZE(ade_composition_type_enum_list));
		if (!prop)
			return 0;

		priv->comp_type_prop = prop;
	}
	drm_object_attach_property(obj, prop, COMPOSITION_UNKNOWN);

	return 0;
}

static int ade_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
		  struct drm_plane *plane)
{
	int ret;

	ret = drm_crtc_init_with_planes(dev, crtc, plane,
					NULL, &ade_crtc_funcs);
	if (ret) {
		DRM_ERROR("failed to init crtc.\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &ade_crtc_helper_funcs);

	ret = ade_install_crtc_properties(dev, &crtc->base);
	if (ret)
		return ret;

	return 0;
}

static int ade_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

static void ade_unbind(struct device *dev, struct device *master,
	void *data)
{
	/* do nothing */
}

static const struct component_ops ade_ops = {
	.bind	= ade_bind,
	.unbind	= ade_unbind,
};

static int ade_dts_parse(struct platform_device *pdev,
			      struct ade_hw_ctx *ctx)
{
	struct resource *res;
	struct device *dev;
	struct device_node *np;
	int ret;

	dev = &pdev->dev;
	np  = dev->of_node;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ade_base");
	ctx->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctx->base)) {
		DRM_ERROR("failed to remap ade io base\n");
		return  PTR_ERR(ctx->base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "media_base");
	ctx->media_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctx->media_base)) {
		DRM_ERROR("failed to remap media io base\n");
		return PTR_ERR(ctx->media_base);
	}

	ctx->irq = platform_get_irq(pdev, 0);
	if (ctx->irq < 0) {
		DRM_ERROR("failed to parse the irq\n");
		return -ENODEV;
	}

	ctx->ade_core_clk = devm_clk_get(&pdev->dev, "clk_ade_core");
	if (ctx->ade_core_clk == NULL) {
		DRM_ERROR("failed to parse the ADE_CORE\n");
		return -ENODEV;
	}
	ctx->media_noc_clk = devm_clk_get(&pdev->dev,
					"aclk_codec_jpeg_src");
	if (ctx->media_noc_clk == NULL) {
		DRM_ERROR("failed to parse the CODEC_JPEG\n");
	    return -ENODEV;
	}
	ctx->ade_pix_clk = devm_clk_get(&pdev->dev, "clk_ade_pix");
	if (ctx->ade_pix_clk == NULL) {
		DRM_ERROR("failed to parse the ADE_PIX_SRC\n");
	    return -ENODEV;
	}

	ret = of_property_read_u32(np, "ade_core_clk_rate",
				    &ctx->ade_core_rate);
	if (ret) {
		DRM_ERROR("failed to parse the ade_core_clk_rate\n");
	    return -ENODEV;
	}
	ret = of_property_read_u32(np, "media_noc_clk_rate",
				    &ctx->media_noc_rate);
	if (ret) {
		DRM_ERROR("failed to parse the media_noc_clk_rate\n");
		return -ENODEV;
	}

	return 0;
}

static int ade_probe(struct platform_device *pdev)
{
	struct ade_data *ade;
	int ret;

	DRM_DEBUG_DRIVER("enter.\n");

	ade = devm_kzalloc(&pdev->dev, sizeof(*ade), GFP_KERNEL);
	if (!ade) {
		DRM_ERROR("failed to alloc ade_data\n");
		return -ENOMEM;
	}

	ret = ade_dts_parse(pdev, &ade->ctx);
	if (ret) {
		DRM_ERROR("failed to parse dts!!\n");
		return ret;
	}

	platform_set_drvdata(pdev, ade);

	return component_add(&pdev->dev, &ade_ops);
}

static int ade_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &ade_ops);

	return 0;
}

static struct of_device_id ade_of_match[] = {
	{ .compatible = "hisilicon,hi6220-ade" },
	{ }
};
MODULE_DEVICE_TABLE(of, ade_of_match);

static struct platform_driver ade_driver = {
	.probe = ade_probe,
	.remove = ade_remove,
	.driver = {
		   .name = "hisi-ade",
		   .owner = THIS_MODULE,
		   .of_match_table = ade_of_match,
	},
};

module_platform_driver(ade_driver);

MODULE_AUTHOR("Xinliang Liu <xinliang.liu@linaro.org>");
MODULE_AUTHOR("Xinliang Liu <z.liuxinliang@hisilicon.com>");
MODULE_AUTHOR("Xinwei Kong <kong.kongxinwei@hisilicon.com>");
MODULE_DESCRIPTION("Hisilicon DRM ADE(crtc/plane) Driver");
MODULE_LICENSE("GPL v2");
