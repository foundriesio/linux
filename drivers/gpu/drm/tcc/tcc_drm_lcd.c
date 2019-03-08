/* tcc_drm_lcd.c
 *
 * Copyright (c) 2016 Telechips Inc.
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <drm/drmP.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/regmap.h>

#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <drm/tcc_drm.h>

#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_ddicfg.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_crtc.h"
#include "tcc_drm_plane.h"

/*
 * LCD stands for Fully Interactive Various Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

/* LCD has totally four hardware windows. */
#define WINDOWS_NR	1

struct lcd_context {
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct tcc_drm_crtc		*crtc;
	struct tcc_drm_plane		planes[WINDOWS_NR];
	struct tcc_drm_plane_config	configs[WINDOWS_NR];
	struct clk			*bus_clk;
	struct clk			*lcd_clk;
	void __iomem			*ddc_reg;	/* TCC DDC register address */
	void __iomem			*vioc_reg;	/* TCC vioc config register address */
	unsigned int			ddc_id;		/* TCC display path number */
	void __iomem			*virt_addr;	/* TCC wmixer node address */
	int				irq_num;	/* TCC interrupt number */
	unsigned long			irq_flags;
	bool				suspended;
	wait_queue_head_t		wait_vsync_queue;
	atomic_t			wait_vsync_event;
	atomic_t			win_updated;
	atomic_t			triggering;
	u32				clkdiv;

	const struct lcd_driver_data *driver_data;
	struct drm_encoder *encoder;
	struct tcc_drm_clk		dp_clk;
};

static const struct of_device_id lcd_driver_dt_match[] = {
	{ .compatible = "telechips,tcc-drm-lcd" },
	{},
};
MODULE_DEVICE_TABLE(of, lcd_driver_dt_match);

static const enum drm_plane_type lcd_win_types[WINDOWS_NR] = {
	DRM_PLANE_TYPE_PRIMARY,
	/*DRM_PLANE_TYPE_OVERLAY,*/
	/*DRM_PLANE_TYPE_OVERLAY,*/
	/*DRM_PLANE_TYPE_OVERLAY,*/
};

static const uint32_t lcd_formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
};

static int lcd_enable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return -EPERM;

	if (!test_and_set_bit(0, &ctx->irq_flags))
		vioc_intr_enable(ctx->irq_num, ctx->ddc_id, VIOC_DISP_INTR_DISPLAY);

	return 0;
}

static void lcd_disable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	if (test_and_clear_bit(0, &ctx->irq_flags))
		vioc_intr_disable(ctx->irq_num, ctx->ddc_id, VIOC_DISP_INTR_DISPLAY);
}

static void lcd_wait_for_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	atomic_set(&ctx->wait_vsync_event, 1);

	/*
	 * wait for LCD to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (!wait_event_timeout(ctx->wait_vsync_queue,
				!atomic_read(&ctx->wait_vsync_event),
				HZ/20))
		DRM_DEBUG_KMS("vblank wait timed out.\n");
}

static void lcd_enable_video_output(struct lcd_context *ctx, unsigned int win,
					bool enable)
{
	if (enable)
		VIOC_RDMA_SetImageEnable(ctx->configs[win].virt_addr);
	else
		VIOC_RDMA_SetImageDisable(ctx->configs[win].virt_addr);
}

static int lcd_atomic_check(struct tcc_drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct drm_display_mode *mode = &state->adjusted_mode;
	struct lcd_context *ctx = crtc->ctx;
	unsigned long ideal_clk, lcd_rate;
	u32 clkdiv;

	if (mode->clock == 0) {
		DRM_INFO("Mode has zero clock value.\n");
		return -EINVAL;
	}

	ideal_clk = mode->clock * 1000;

	lcd_rate = clk_get_rate(ctx->lcd_clk);
	if (2 * lcd_rate < ideal_clk) {
		DRM_INFO("sclk_lcd clock too low(%lu) for requested pixel clock(%lu)\n",
			 lcd_rate, ideal_clk);
		return -EINVAL;
	}

	/* Find the clock divider value that gets us closest to ideal_clk */
	clkdiv = DIV_ROUND_CLOSEST(lcd_rate, ideal_clk);
	if (clkdiv >= 0x200) {
		DRM_INFO("requested pixel clock(%lu) too low\n", ideal_clk);
		return -EINVAL;
	}

	ctx->clkdiv = (clkdiv < 0x100) ? clkdiv : 0xff;

	return 0;
}

static void lcd_commit(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
	struct drm_display_mode *mode = &crtc->base.state->adjusted_mode;

	if (ctx->suspended)
		return;

	/* nothing to do if we haven't set the mode yet */
	if (mode->htotal == 0 || mode->vtotal == 0)
		return;

	/* TODO : Display Device Register. timing values */
}

static void lcd_win_set_pixfmt(struct lcd_context *ctx, unsigned int win,
				uint32_t pixel_format, uint32_t width)
{
	void __iomem *pRDMA = ctx->configs[win].virt_addr;

	switch (pixel_format) {
	case DRM_FORMAT_RGB565:
		VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB565, width);
		VIOC_RDMA_SetImageFormat(pRDMA, TCC_LCDC_IMG_FMT_RGB565);
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB888, width);
		VIOC_RDMA_SetImageFormat(pRDMA, TCC_LCDC_IMG_FMT_RGB888);
		break;
	default:
		DRM_DEBUG_KMS("invalid pixel size so using unpacked 24bpp.\n");
		VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB888, width);
		VIOC_RDMA_SetImageFormat(pRDMA, TCC_LCDC_IMG_FMT_RGB888);
		break;
	}
}

static void lcd_atomic_begin(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;
}

static void lcd_atomic_flush(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	tcc_crtc_handle_event(crtc);
}

static void lcd_update_plane(struct tcc_drm_crtc *crtc,
			      struct tcc_drm_plane *plane)
{
	struct tcc_drm_plane_state *state =
				to_tcc_plane_state(plane->base.state);
	struct lcd_context *ctx = crtc->ctx;
	struct drm_framebuffer *fb = state->base.fb;
	dma_addr_t dma_addr;
	unsigned long val, size, offset;
	unsigned int win = plane->index;
	unsigned int cpp = fb->format->cpp[0];
	unsigned int pitch = fb->pitches[0];

	/* TCC specific structure */
	void __iomem *pWMIX;
	void __iomem *pRDMA;

	if (ctx->suspended)
		return;

	pWMIX = ctx->virt_addr;
	pRDMA = ctx->configs[win].virt_addr;

	offset = state->src.x * cpp;
	offset += state->src.y * pitch;

	VIOC_WMIX_SetPosition(pWMIX, win, state->crtc.x, state->crtc.y);

	/* Using the pixel alpha */
	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 1);
	VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);

	/* buffer start address */
	dma_addr = tcc_drm_fb_dma_addr(fb, 0) + offset;
	val = (unsigned long)dma_addr;
	VIOC_RDMA_SetImageBase(pRDMA, val, 0, 0);
	VIOC_RDMA_SetImageSize(pRDMA, state->src.w, state->src.h);

	/* buffer end address */
	size = pitch * state->crtc.h;
	val = (unsigned long)(dma_addr + size);

	DRM_DEBUG_KMS("start addr = 0x%lx, end addr = 0x%lx, size = 0x%lx\n",
			(unsigned long)dma_addr, val, size);
	DRM_DEBUG_KMS("ovl_width = %d, ovl_height = %d\n",
			state->crtc.w, state->crtc.h);

	lcd_win_set_pixfmt(ctx, win, fb->format->format, state->src.w);

	lcd_enable_video_output(ctx, win, true);

	VIOC_WMIX_SetUpdate(pWMIX);
}

static void lcd_disable_plane(struct tcc_drm_crtc *crtc,
			       struct tcc_drm_plane *plane)
{
	struct lcd_context *ctx = crtc->ctx;
	unsigned int win = plane->index;

	if (ctx->suspended)
		return;

	lcd_enable_video_output(ctx, win, false);
}

static void lcd_enable(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
	int ret;

	if (!ctx->suspended)
		return;

	ctx->suspended = false;

	pm_runtime_get_sync(ctx->dev);

	ret = clk_prepare_enable(ctx->bus_clk);
	if (ret < 0) {
		DRM_ERROR("Failed to prepare_enable the bus clk [%d]\n", ret);
		return;
	}

	ret = clk_prepare_enable(ctx->lcd_clk);
	if  (ret < 0) {
		DRM_ERROR("Failed to prepare_enable the lcd clk [%d]\n", ret);
		return;
	}

	/* if vblank was enabled status, enable it again. */
	if (test_and_clear_bit(0, &ctx->irq_flags))
		lcd_enable_vblank(ctx->crtc);

	lcd_commit(ctx->crtc);
}

static void lcd_disable(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
	int i;

	if (ctx->suspended)
		return;

	/*
	 * We need to make sure that all windows are disabled before we
	 * suspend that connector. Otherwise we might try to scan from
	 * a destroyed buffer later.
	 */
	for (i = 0; i < WINDOWS_NR; i++)
		lcd_disable_plane(crtc, &ctx->planes[i]);

	lcd_enable_vblank(crtc);
	lcd_wait_for_vblank(crtc);
	lcd_disable_vblank(crtc);

	clk_disable_unprepare(ctx->lcd_clk);
	clk_disable_unprepare(ctx->bus_clk);

	pm_runtime_put_sync(ctx->dev);
	ctx->suspended = true;
}

static void lcd_trigger(struct device *dev)
{
	/* TODO */
}

static void lcd_te_handler(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	/* Checks the crtc is detached already from encoder */
	if (!ctx->drm_dev)
		return;

	/*
	 * If there is a page flip request, triggers and handles the page flip
	 * event so that current fb can be updated into panel GRAM.
	 */
	if (atomic_add_unless(&ctx->win_updated, -1, 0))
		lcd_trigger(ctx->dev);

	/* Wakes up vsync event queue */
	if (atomic_read(&ctx->wait_vsync_event)) {
		atomic_set(&ctx->wait_vsync_event, 0);
		wake_up(&ctx->wait_vsync_queue);
	}

	if (test_bit(0, &ctx->irq_flags))
		drm_crtc_handle_vblank(&ctx->crtc->base);
}

static const struct tcc_drm_crtc_ops lcd_crtc_ops = {
	.enable = lcd_enable,
	.disable = lcd_disable,
	.enable_vblank = lcd_enable_vblank,
	.disable_vblank = lcd_disable_vblank,
	.atomic_begin = lcd_atomic_begin,
	.update_plane = lcd_update_plane,
	.disable_plane = lcd_disable_plane,
	.atomic_flush = lcd_atomic_flush,
	.atomic_check = lcd_atomic_check,
	.te_handler = lcd_te_handler,
};

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	struct lcd_context *ctx = (struct lcd_context *)dev_id;
	u32 dispblock_status = 0;

	if (ctx == NULL) {
		pr_err("%s irq: %d dev_id:%p \n",__func__, irq, dev_id);
		return IRQ_HANDLED;
	}

	/* Get TCC VIOC block status register */
	dispblock_status = vioc_intr_get_status(ctx->ddc_id);

	if (dispblock_status & (1 << VIOC_DISP_INTR_RU)) {
		vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_RU));

		/* check the crtc is detached already from encoder */
		if (!ctx->drm_dev)
			goto out;

		drm_crtc_handle_vblank(&ctx->crtc->base);

		/* set wait vsync event to zero and wake up queue. */
		if (atomic_read(&ctx->wait_vsync_event)) {
			atomic_set(&ctx->wait_vsync_event, 0);
			wake_up(&ctx->wait_vsync_queue);
		}

		/* Check FIFO underrun. */
		if (dispblock_status & (1 << VIOC_DISP_INTR_FU)) {
			vioc_intr_disable(irq, ctx->ddc_id,
					  (1 << VIOC_DISP_INTR_FU));
			vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_FU));
			pr_crit("%s: FIFO UNDERRUN STATUS:0x%x \n", __func__,
				dispblock_status);
		}
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_DD))
		vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_DD));

	if (dispblock_status & (1 << VIOC_DISP_INTR_SREQ))
		vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_SREQ));

out:
	return IRQ_HANDLED;
}

static int lcd_bind(struct device *dev, struct device *master, void *data)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct tcc_drm_plane *tcc_plane;
	unsigned int i;
	int ret;

	ctx->drm_dev = drm_dev;

	for (i = 0; i < WINDOWS_NR; i++) {
		ctx->configs[i].pixel_formats = lcd_formats;
		ctx->configs[i].num_pixel_formats = ARRAY_SIZE(lcd_formats);
		ctx->configs[i].zpos = i;
		ctx->configs[i].type = lcd_win_types[i];
		ret = tcc_plane_init(drm_dev, &ctx->planes[i], i,
					&ctx->configs[i]);
		if (ret)
			return ret;
	}

	tcc_plane = &ctx->planes[DEFAULT_WIN];
	ctx->crtc = tcc_drm_crtc_create(drm_dev, &tcc_plane->base,
			TCC_DISPLAY_TYPE_LCD, &lcd_crtc_ops, ctx);
	if (IS_ERR(ctx->crtc))
		return PTR_ERR(ctx->crtc);

	if (ctx->encoder)
		tcc_dpi_bind(drm_dev, ctx->encoder);

	return ret;
}

static void lcd_unbind(struct device *dev, struct device *master,
			void *data)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);

	lcd_disable(ctx->crtc);

	if (ctx->encoder)
		tcc_dpi_remove(ctx->encoder);
}

static const struct component_ops lcd_component_ops = {
	.bind	= lcd_bind,
	.unbind = lcd_unbind,
};

static int lcd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lcd_context *ctx;
	struct resource *res;
	struct resource *res_ddc, *res_rdma, *res_wmix;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "failed to get the device node\n");
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->suspended = true;
	ctx->driver_data = of_device_get_match_data(dev);

	if (of_property_read_u32(dev->of_node,
				 "display-port-num", &ctx->ddc_id)) {
		dev_err(dev, "failed to get display port number\n");
		return -ENODEV;
	}

	ctx->bus_clk = devm_clk_get(dev, "ddibus_vioc");
	if (IS_ERR(ctx->bus_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		return PTR_ERR(ctx->bus_clk);
	}

	ctx->lcd_clk = devm_clk_get(dev, "peri_lcd");
	if (IS_ERR(ctx->lcd_clk)) {
		dev_err(dev, "failed to get lcd clock\n");
		return PTR_ERR(ctx->lcd_clk);
	}

	if (!is_VIOC_REMAP) {
		res_ddc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res_rdma = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		res_wmix = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	} else {
		res_ddc = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		res_rdma = platform_get_resource(pdev, IORESOURCE_MEM, 4);
		res_wmix = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	}

	ctx->ddc_reg = devm_ioremap_resource(dev, res_ddc);
	if (IS_ERR(ctx->ddc_reg))
		return PTR_ERR(ctx->ddc_reg);

	ctx->configs[0].virt_addr = devm_ioremap_resource(dev, res_rdma);
	if (IS_ERR(ctx->configs[0].virt_addr))
		return PTR_ERR(ctx->configs[0].virt_addr);

	ctx->virt_addr = devm_ioremap_resource(dev, res_wmix);
	if (IS_ERR(ctx->virt_addr))
		return PTR_ERR(ctx->virt_addr);

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
					   "vsync");
	if (!res) {
		dev_err(dev, "irq request failed.\n");
		return -ENXIO;
	}

	ctx->irq_num = res->start;
	ret = devm_request_irq(dev, ctx->irq_num, lcd_irq_handler,
					IRQF_SHARED, "drm_lcd", ctx);
	if (ret) {
		dev_err(dev, "irq request failed.\n");
		return ret;
	}

	DRM_DEBUG_KMS("DispNum:%d, virt_addr:0x%p\n", ctx->ddc_id, ctx->ddc_reg);

	init_waitqueue_head(&ctx->wait_vsync_queue);
	atomic_set(&ctx->wait_vsync_event, 0);

	platform_set_drvdata(pdev, ctx);

	ctx->encoder = tcc_dpi_probe(dev);
	if (IS_ERR(ctx->encoder))
		return PTR_ERR(ctx->encoder);

	pm_runtime_enable(dev);

	ret = component_add(dev, &lcd_component_ops);
	if (ret)
		goto err_disable_pm_runtime;

	return ret;

err_disable_pm_runtime:
	pm_runtime_disable(dev);

	return ret;
}

static int lcd_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	component_del(&pdev->dev, &lcd_component_ops);

	return 0;
}

#ifdef CONFIG_PM
static int tcc_lcd_suspend(struct device *dev)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);

	clk_disable_unprepare(ctx->lcd_clk);
	clk_disable_unprepare(ctx->bus_clk);

	return 0;
}

static int tcc_lcd_resume(struct device *dev)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(ctx->bus_clk);
	if (ret < 0) {
		DRM_ERROR("Failed to prepare_enable the bus clk [%d]\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(ctx->lcd_clk);
	if  (ret < 0) {
		DRM_ERROR("Failed to prepare_enable the lcd clk [%d]\n", ret);
		return ret;
	}

	return 0;
}
#endif

static const struct dev_pm_ops tcc_lcd_pm_ops = {
	SET_RUNTIME_PM_OPS(tcc_lcd_suspend, tcc_lcd_resume, NULL)
};

struct platform_driver lcd_driver = {
	.probe		= lcd_probe,
	.remove		= lcd_remove,
	.driver		= {
		.name	= "tcc-drm-lcd",
		.owner	= THIS_MODULE,
		.pm	= &tcc_lcd_pm_ops,
		.of_match_table = lcd_driver_dt_match,
	},
};
