/* tcc_drm_lcd.c
 *
 * Copyright (c) 2017 Telechips Inc.
 *
 * Author:  Telechips Inc.
 * Created: Sep 05, 2017
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *---------------------------------------------------------------------------
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
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/regmap.h>

#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <drm/tcc_drm.h>

#include <video/tcc/tccfb.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tccfb_ioctrl.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_fbdev.h"
#include "tcc_drm_crtc.h"
#include "tcc_drm_plane.h"

/*
 * LCD stands for Fully Interactive Embedded Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

/* LCD has totally four hardware windows. */
#define WINDOWS_NR	4
#define CURSOR_WIN	5 /* Not Used for tcc */

struct lcd_driver_data {
	unsigned int timing_base;
};

static struct lcd_driver_data fld0800_lcd_driver_data = {
	.timing_base = 0x0,
};

struct lcd_context {
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct tcc_drm_crtc		*crtc;
	struct tcc_drm_plane		planes[WINDOWS_NR];
	struct clk			*bus_clk;
	struct clk			*lcd_clk;
	void __iomem			*ddc_reg;	/* DDC register address */
	unsigned int			ddc_id;		/* TCC display path number */
	void __iomem			*virt_addr; /* TCC wmixer node address */
	int				irq;		/* TCC interrupt number */
	unsigned long			irq_flags;
	bool				suspended;
	int				pipe;
	wait_queue_head_t		wait_vsync_queue;
	atomic_t			wait_vsync_event;
	atomic_t			win_updated;
	atomic_t			triggering;

	struct tcc_drm_panel_info panel;
	struct lcd_driver_data *driver_data;
	struct drm_encoder *encoder;
};

static const struct of_device_id lcd_driver_dt_match[] = {
	{ .compatible = "telechips,tcc-drm-lcd",
	  .data = &fld0800_lcd_driver_data },
	{},
};
MODULE_DEVICE_TABLE(of, lcd_driver_dt_match);

static const uint32_t lcd_formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
};

static inline struct lcd_driver_data *drm_lcd_get_driver_data(
	struct platform_device *pdev)
{
	const struct of_device_id *of_id =
			of_match_device(lcd_driver_dt_match, &pdev->dev);

	return (struct lcd_driver_data *)of_id->data;
}

static int lcd_enable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return -EPERM;

	if (!test_and_set_bit(0, &ctx->irq_flags))
		vioc_intr_enable(ctx->irq, ctx->ddc_id, VIOC_DISP_INTR_DISPLAY);

	return 0;
}

static void lcd_disable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	if (test_and_clear_bit(0, &ctx->irq_flags))
		vioc_intr_disable(ctx->irq, ctx->ddc_id, VIOC_DISP_INTR_DISPLAY);
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
				struct drm_framebuffer *fb)
{
	void __iomem *pRDMA = ctx->planes[win].virt_addr;

	switch (fb->pixel_format) {
	case DRM_FORMAT_RGB565:
		VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB565, fb->width);
		VIOC_RDMA_SetImageFormat(pRDMA, TCC_LCDC_IMG_FMT_RGB565);
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB888, fb->width);
		VIOC_RDMA_SetImageFormat(pRDMA, TCC_LCDC_IMG_FMT_RGB888);
		break;
	default:
		DRM_DEBUG_KMS("invalid pixel size so using unpacked 24bpp.\n");
		VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB888, fb->width);
		VIOC_RDMA_SetImageFormat(pRDMA, TCC_LCDC_IMG_FMT_RGB888);
		break;
	}

	DRM_DEBUG_KMS("bpp = %d\n", fb->bits_per_pixel);
}

static void lcd_win_set_colkey(struct lcd_context *ctx, unsigned int win)
{
	/* TODO : Setting the color key registers */
}

/**
 * shadow_protect_win() - disable updating values from shadow registers at vsync
 *
 * @win: window to protect registers for
 * @protect: 1 to protect (disable updates)
 */
static void lcd_shadow_protect_win(struct lcd_context *ctx,
				    unsigned int win, bool protect)
{
	/* TODO : check this function is need? */
}

static void lcd_atomic_begin(struct tcc_drm_crtc *crtc,
			       struct tcc_drm_plane *plane)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	lcd_shadow_protect_win(ctx, plane->zpos, true);
}

static void lcd_atomic_flush(struct tcc_drm_crtc *crtc,
			       struct tcc_drm_plane *plane)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	lcd_shadow_protect_win(ctx, plane->zpos, false);
}

static void lcd_update_plane(struct tcc_drm_crtc *crtc,
			      struct tcc_drm_plane *plane)
{
	struct lcd_context *ctx = crtc->ctx;
	struct drm_plane_state *state = plane->base.state;
	dma_addr_t dma_addr;
	unsigned long val, size, offset;
	unsigned int win = plane->zpos;
	unsigned int bpp = state->fb->bits_per_pixel >> 3;
	unsigned int pitch = state->fb->pitches[0];
	/* TCC specific structure */
	void __iomem *pWMIX;
	void __iomem *pRDMA;

	if (ctx->suspended)
		return;

	pWMIX = ctx->virt_addr;
	pRDMA = ctx->planes[win].virt_addr;

	offset = plane->src_x * bpp;
	offset += plane->src_y * pitch;

	VIOC_WMIX_SetPosition(pWMIX, win, plane->crtc_x, plane->crtc_y);

	/* In DRM Driver case, using the pixel alpha only for RDMA. */
	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 1);
	VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);

	/* buffer start address */
	dma_addr = plane->dma_addr[0] + offset;
	val = (unsigned long)dma_addr;
	VIOC_RDMA_SetImageBase(pRDMA, val, 0, 0);
	VIOC_RDMA_SetImageSize(pRDMA, plane->src_w, plane->src_h);

	/* buffer end address */
	size = pitch * plane->crtc_h;
	val = (unsigned long)(dma_addr + size);

	DRM_DEBUG_KMS("start addr = 0x%lx, end addr = 0x%lx, size = 0x%lx\n",
			(unsigned long)dma_addr, val, size);
	DRM_DEBUG_KMS("ovl_width = %d, ovl_height = %d\n",
			plane->crtc_w, plane->crtc_h);

	/* buffer size */
	/* TODO: check buffer size and osd are needed for vioc */
	lcd_win_set_pixfmt(ctx, win, state->fb);

	VIOC_RDMA_SetImageEnable(pRDMA);
	VIOC_WMIX_SetUpdate(pWMIX);
}

static void lcd_disable_plane(struct tcc_drm_crtc *crtc,
			       struct tcc_drm_plane *plane)
{
	struct lcd_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;
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
	if (ctx->pipe < 0 || !ctx->drm_dev)
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
	.commit = lcd_commit,
	.enable_vblank = lcd_enable_vblank,
	.disable_vblank = lcd_disable_vblank,
	.wait_for_vblank = lcd_wait_for_vblank,
	.atomic_begin = lcd_atomic_begin,
	.update_plane = lcd_update_plane,
	.disable_plane = lcd_disable_plane,
	.atomic_flush = lcd_atomic_flush,
	.te_handler = lcd_te_handler,
};

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	struct lcd_context *ctx = (struct lcd_context *)dev_id;
	u32 dispblock_status = 0;
	int win;

	if (ctx == NULL) {
		pr_err("%s irq: %d dev_id:%p \n",__func__, irq, dev_id);
		return IRQ_HANDLED;
	}

	/* Get TCC VIOC block status register */
	dispblock_status = vioc_intr_get_status(ctx->ddc_id);

	if(dispblock_status & (1 << VIOC_DISP_INTR_RU))
	{
		vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_RU));

		/* check the crtc is detached already from encoder */
		if (ctx->pipe < 0 || !ctx->drm_dev)
			goto out;

		drm_crtc_handle_vblank(&ctx->crtc->base);

		/* TODO: Need to check below code */
		for (win = 0 ; win < WINDOWS_NR ; win++) {
			struct tcc_drm_plane *plane = &ctx->planes[win];

			if (!plane->pending_fb)
				continue;

			/*start = readl(ctx->ddc_reg + VIDWx_BUF_START(win, 0));*/
			/*start_s = readl(ctx->ddc_reg + VIDWx_BUF_START_S(win, 0));*/
			/*if (start == start_s)*/
			tcc_drm_crtc_finish_update(ctx->crtc, plane);
		}

		/* set wait vsync event to zero and wake up queue. */
		if (atomic_read(&ctx->wait_vsync_event)) {
			atomic_set(&ctx->wait_vsync_event, 0);
			wake_up(&ctx->wait_vsync_queue);
		}
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_DD)) {
		vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_DD));
		/* TODO */
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_SREQ))
		vioc_intr_clear(ctx->ddc_id, (1 << VIOC_DISP_INTR_SREQ));

out:
	return IRQ_HANDLED;
}

static int lcd_bind(struct device *dev, struct device *master, void *data)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct tcc_drm_private *priv = drm_dev->dev_private;
	struct tcc_drm_plane *tcc_plane;
	enum drm_plane_type type;
	unsigned int zpos;
	int ret;

	ctx->drm_dev = drm_dev;
	ctx->pipe = priv->pipe++;

	for (zpos = 0; zpos < WINDOWS_NR; zpos++) {
		/* -1 For overlay plane */
		type = tcc_plane_get_type(zpos, -1);
		ret = tcc_plane_init(drm_dev, &ctx->planes[zpos],
					1 << ctx->pipe, type, lcd_formats,
					ARRAY_SIZE(lcd_formats), zpos);
		if (ret)
			return ret;
	}

	tcc_plane = &ctx->planes[DEFAULT_WIN];
	ctx->crtc = tcc_drm_crtc_create(drm_dev, &tcc_plane->base,
					   ctx->pipe, TCC_DISPLAY_TYPE_LCD,
					   &lcd_crtc_ops, ctx);
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
	ctx->driver_data = drm_lcd_get_driver_data(pdev);

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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	ctx->ddc_reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctx->ddc_reg))
		return PTR_ERR(ctx->ddc_reg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	ctx->planes[0].virt_addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctx->planes[0].virt_addr))
		return PTR_ERR(ctx->planes[0].virt_addr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);

	ctx->virt_addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctx->virt_addr))
		return PTR_ERR(ctx->virt_addr);

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
					   "vsync");
	if (!res) {
		dev_err(dev, "irq request failed.\n");
		return -ENXIO;
	}

	ctx->irq = res->start;
	ret = devm_request_irq(dev, ctx->irq, lcd_irq_handler,
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

struct platform_driver lcd_driver = {
	.probe		= lcd_probe,
	.remove		= lcd_remove,
	.driver		= {
		.name	= "tcc-drm-lcd",
		.owner	= THIS_MODULE,
		.of_match_table = lcd_driver_dt_match,
	},
};
