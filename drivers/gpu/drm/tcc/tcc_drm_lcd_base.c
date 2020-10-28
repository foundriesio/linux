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
#include <drm/drm_crtc_helper.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_ddicfg.h>
#include <video/tcc/vioc_config.h>

#include <tcc_drm_address.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_fb.h>
#include <tcc_drm_crtc.h>
#include <tcc_drm_plane.h>

#define LOG_TAG "DRMLCD"

/*
 * LCD stands for Fully Interactive Various Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
struct lcd_chromakeys {
	unsigned int chromakey_enable;
	struct drm_chromakey_t value;
	struct drm_chromakey_t mask;
};
#endif

struct lcd_context {
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct tcc_drm_crtc		*crtc;
	struct drm_encoder 		*encoder;
	struct tcc_drm_plane		planes[CRTC_WIN_NR_MAX];

	unsigned long			crtc_flags;

	/* VSYNC */
	wait_queue_head_t		wait_vsync_queue;
	atomic_t			wait_vsync_event;

	spinlock_t 			irq_lock;

	/*
	 * keep_logo
	 * If this flag is set to 1, DRM driver skips process that fills its plane with block color
	 * when it is binded. In other words, If so any Logo image was displayed on screen at booting time,
	 * it will is keep dislayed on screen until the DRM application is executed. */
	int 				keep_logo;


	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	struct mutex chromakey_mutex;
	struct lcd_chromakeys lcd_chromakeys[3];
	#endif

	/* count for fifo-underrun */
	unsigned int 			cnt_underrun;

	/* Device driver data */
	struct tcc_drm_device_data 	*data;

	/* H/W data */
	struct tcc_hw_device 		hw_data;

};

#if defined(CONFIG_DRM_TCC_LCD)
static struct tcc_drm_device_data lcd_old_data = {
	.version = TCC_DRM_DT_VERSION_OLD,
	.output_type = TCC_DISPLAY_TYPE_LCD,
};
static struct tcc_drm_device_data lcd_v1_x_data = {
	.version = TCC_DRM_DT_VERSION_1_0,
	.output_type = TCC_DISPLAY_TYPE_LCD,
};
#endif

#if defined(CONFIG_DRM_TCC_EXT)
static struct tcc_drm_device_data ext_old_data = {
	.version = TCC_DRM_DT_VERSION_OLD,
	.output_type = TCC_DISPLAY_TYPE_EXT,
};
static struct tcc_drm_device_data ext_v1_x_data = {
	.version = TCC_DRM_DT_VERSION_1_0,
	.output_type = TCC_DISPLAY_TYPE_EXT,
};
#endif

#if defined(CONFIG_DRM_TCC_THIRD)
static struct tcc_drm_device_data third_old_data = {
	.version = TCC_DRM_DT_VERSION_OLD,
	.output_type = TCC_DISPLAY_TYPE_THIRD,
};
static struct tcc_drm_device_data third_v1_x_data = {
	.version = TCC_DRM_DT_VERSION_1_0,
	.output_type = TCC_DISPLAY_TYPE_THIRD,
};
#endif

#if defined(CONFIG_DRM_TCC_FOURTH)
static struct tcc_drm_device_data fourth_old_data = {
	.version = TCC_DRM_DT_VERSION_OLD,
	.output_type = TCC_DISPLAY_TYPE_FOURTH,
};
static struct tcc_drm_device_data fourth_v1_x_data = {
	.version = TCC_DRM_DT_VERSION_1_0,
	.output_type = TCC_DISPLAY_TYPE_FOURTH,
};
#endif

static const struct of_device_id lcd_driver_dt_match[] = {
	#if defined(CONFIG_DRM_TCC_LCD)
	{
		.compatible = "telechips,tcc-drm-lcd",
		.data = (const void*)&lcd_old_data,
	}, {
		.compatible = "telechips,tcc-drm-lcd-v1.0",
		.data = (const void*)&lcd_v1_x_data,
	},
	#endif
}
MODULE_DEVICE_TABLE(of, lcd_driver_dt_match);

static const struct of_device_id ext_driver_dt_match[] = {
	#if defined(CONFIG_DRM_TCC_EXT)
	{
		.compatible = "telechips,tcc-drm-ext",
		.data = (const void*)&ext_old_data,
	}, {
		.compatible = "telechips,tcc-drm-ext-v1.0",
		.data = (const void*)&ext_v1_x_data,
	},
	#endif
}
MODULE_DEVICE_TABLE(of, ext_driver_dt_match);

static const struct of_device_id third_driver_dt_match[] = {
	#if defined(CONFIG_DRM_TCC_THIRD)
	{
		.compatible = "telechips,tcc-drm-third",
		.data = (const void*)&third_old_data,
	}, {
		.compatible = "telechips,tcc-drm-third-v1.0",
		.data = (const void*)&third_v1_x_data,
	},
	#endif
}
MODULE_DEVICE_TABLE(of, third_driver_dt_match);

static const struct of_device_id fourth_driver_dt_match[] = {
	#if defined(CONFIG_DRM_TCC_FOURTH)
	{
		.compatible = "telechips,tcc-drm-fourth",
		.data = (const void*)&fourth_old_data,
	}, {
		.compatible = "telechips,tcc-drm-fourth-v1.0",
		.data = (const void*)&fourth_v1_x_data,
	},
	#endif
	{},
};
MODULE_DEVICE_TABLE(of, fourth_driver_dt_match);

static const uint32_t lcd_formats[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
};

static const uint32_t lcd_formats_vrdma[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420,
};

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	unsigned int display_device_blk_num;

	struct lcd_context *ctx = (struct lcd_context *)dev_id;
	u32 dispblock_status = 0;

	if (ctx == NULL) {
		dev_warn(ctx->dev,
				"[WARN][%s] %s line(%d) ctx is NULL\r\n",
					LOG_TAG, __func__, __LINE__);
		return IRQ_HANDLED;
	}

	display_device_blk_num = get_vioc_index(ctx->hw_data.display_device.blk_num);

	/* Get TCC VIOC block status register */
	dispblock_status = vioc_intr_get_status(display_device_blk_num);

	if (dispblock_status & (1 << VIOC_DISP_INTR_RU)) {
		vioc_intr_clear(display_device_blk_num, (1 << VIOC_DISP_INTR_RU));

		/* check the crtc is detached already from encoder */
		if (ctx->drm_dev == NULL) {
			dev_warn(ctx->dev,
				"[WARN][%s] %s line(%d) drm_dev is not binded\r\n",
					LOG_TAG, __func__, __LINE__);
			goto out;
		}

		/* set wait vsync event to zero and wake up queue. */
		if (atomic_read(&ctx->wait_vsync_event)) {
			atomic_set(&ctx->wait_vsync_event, 0);
			wake_up(&ctx->wait_vsync_queue);
		}
		tcc_drm_crtc_vblank_handler(&ctx->crtc->base);
	}

	/* Check FIFO underrun. */
	if (dispblock_status & (1 << VIOC_DISP_INTR_FU)) {
		vioc_intr_clear(display_device_blk_num, (1 << VIOC_DISP_INTR_FU));
		if(VIOC_DISP_Get_TurnOnOff(ctx->hw_data.display_device.virt_addr)) {
			if((ctx->cnt_underrun++ % 120) == 0) {
				dev_err(ctx->dev, "[ERROR][%s] FIFO UNDERRUN status(0x%x) %s\n",
							LOG_TAG, dispblock_status, __func__);
			}
		}
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_DD))
		vioc_intr_clear(display_device_blk_num, (1 << VIOC_DISP_INTR_DD));

	if (dispblock_status & (1 << VIOC_DISP_INTR_SREQ))
		vioc_intr_clear(display_device_blk_num, (1 << VIOC_DISP_INTR_SREQ));
out:
	return IRQ_HANDLED;
}

static int lcd_enable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
	unsigned long irqflags;
	int ret = 0;

	spin_lock_irqsave(&ctx->irq_lock, irqflags);
	if (!test_and_set_bit(CRTC_FLAGS_IRQ_BIT, &ctx->crtc_flags)) {
		vioc_intr_clear(ctx->hw_data.display_device.blk_num, VIOC_DISP_INTR_DISPLAY);

		ret = devm_request_irq(ctx->dev, ctx->hw_data.display_device.irq_num,
				lcd_irq_handler, IRQF_SHARED, ctx->data->name, ctx);
		if(ret < 0) {
			dev_err(ctx->dev, "[ERROR][%s] %s failed to request irq\r\n",
									LOG_TAG, __func__);
			goto err_irq_request;
		}
		vioc_intr_enable(ctx->hw_data.display_device.irq_num,
		  get_vioc_index(ctx->hw_data.display_device.blk_num), VIOC_DISP_INTR_DISPLAY);
	}
err_irq_request:
	spin_unlock_irqrestore(&ctx->irq_lock, irqflags);
	return ret;
}


static void lcd_disable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
	unsigned long irqflags;
	spin_lock_irqsave(&ctx->irq_lock, irqflags);
	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT, &ctx->crtc_flags)) {
		vioc_intr_disable(ctx->hw_data.display_device.irq_num,
		  get_vioc_index(ctx->hw_data.display_device.blk_num), VIOC_DISP_INTR_DISPLAY);
		devm_free_irq(ctx->dev, ctx->hw_data.display_device.irq_num, ctx);
	}
	spin_unlock_irqrestore(&ctx->irq_lock, irqflags);
}

#if defined(CONFIG_TCCDRM_USES_WAIT_VBLANK)
static void lcd_wait_for_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;

	atomic_set(&ctx->wait_vsync_event, 1);

	/*
	 * wait for LCD to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (test_bit(CRTC_FLAGS_IRQ_BIT, &ctx->crtc_flags)) {
		if (!wait_event_timeout(ctx->wait_vsync_queue,
					!atomic_read(&ctx->wait_vsync_event),
					msecs_to_jiffies(50)))
			DRM_DEBUG_KMS("vblank wait timed out.\n");
	}
}
#endif

static void lcd_enable_video_output(struct lcd_context *ctx, unsigned int win,
					bool enable)
{
	do {
		if(win >= ctx->hw_data.rdma_counts) {
			dev_info(ctx->dev, "[ERROR][%s] %s line(%d) win(%d) is out of range (%d)\r\n",
				LOG_TAG, __func__, __LINE__, win, ctx->hw_data.rdma_counts);
			break;
		}
		if(ctx->hw_data.rdma[win].virt_addr == NULL) {
			dev_info(ctx->dev, "[ERROR][%s] %s line(%d) virtual address of win(%d) is NULL\r\n",
				LOG_TAG, __func__, __LINE__, win);
			break;
		}
		if (enable) {
			VIOC_RDMA_SetImageEnable(ctx->hw_data.rdma[win].virt_addr);
		} else {
			if (test_bit(CRTC_FLAGS_PCLK_BIT, &ctx->crtc_flags)) {
				VIOC_RDMA_SetImageDisable(ctx->hw_data.rdma[win].virt_addr);
			} else {
				unsigned int enabled;
				VIOC_RDMA_GetImageEnable(ctx->hw_data.rdma[win].virt_addr, &enabled);
				if(enabled) {
					dev_info(ctx->dev,
						"[INFO][%s] %s win(%d) Disable RDMA with NoWait\r\n",
										LOG_TAG, __func__, win);
					VIOC_RDMA_SetImageDisableNW(ctx->hw_data.rdma[win].virt_addr);
				}
			}
		}
	} while(0);
}

static int lcd_atomic_check(struct tcc_drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct drm_display_mode *mode = &state->adjusted_mode;
	struct lcd_context *ctx = crtc->ctx;
	unsigned long ideal_clk, lcd_rate;
	volatile void __iomem *ddi_config;
	int pixel_clock_from_hdmi = 0;
	u32 clkdiv;

	ddi_config = VIOC_DDICONFIG_GetAddress();
	#if !defined(CONFIG_ARCH_TCC897X)
	if(VIOC_DDICONFIG_GetPeriClock(ddi_config, get_vioc_index(ctx->hw_data.display_device.blk_num))) {
		pixel_clock_from_hdmi = 1;
	}
	#endif

	if (mode->clock == 0) {
		dev_warn(ctx->dev, "[WARN][%s] %s line(%d) Mode has zero clock value.\n",
								LOG_TAG, __func__, __LINE__);
		return -EINVAL;
	}

	if(!pixel_clock_from_hdmi) {
		ideal_clk = mode->clock * 1000;

		lcd_rate = clk_get_rate(ctx->hw_data.ddc_clock);
		if (2 * lcd_rate < ideal_clk) {
			dev_err(ctx->dev,
				"[ERROR][%s] %s line(%d) sclk_lcd clock too low(%lu) for requested pixel clock(%lu)\n",
				 					LOG_TAG, __func__, __LINE__, lcd_rate, ideal_clk);
			return -EINVAL;
		}

		/* Find the clock divider value that gets us closest to ideal_clk */
		clkdiv = DIV_ROUND_CLOSEST(lcd_rate, ideal_clk);
		if (clkdiv >= 0x200) {
			dev_err(ctx->dev,
				"[ERROR][%s] %s line(%d) requested pixel clock(%lu) too low\n",
				 			LOG_TAG, __func__, __LINE__, ideal_clk);
			return -EINVAL;
		}
	}

	return 0;
}

static void lcd_win_set_pixfmt(struct lcd_context *ctx, unsigned int win,
				uint32_t pixel_format, uint32_t width)
{
	volatile void __iomem *pRDMA = NULL;
	unsigned int vioc_swap = 0;
	unsigned int vioc_y2r = 0;
	unsigned int vioc_fmt;

	do {
		if(win >= ctx->hw_data.rdma_counts) {
			pr_err("[ERROR][%s] %s line(%d) win(%d) is out of range (%d)\r\n",
				LOG_TAG, __func__, __LINE__, win, ctx->hw_data.rdma_counts);
			break;
		}
		if(ctx->hw_data.rdma[win].virt_addr == NULL) {
			pr_err("[ERROR][%s] %s line(%d) virtual address of win(%d) is NULL\r\n",
				LOG_TAG, __func__, __LINE__, win);
			break;
		}

		pRDMA = ctx->hw_data.rdma[win].virt_addr;
		/* Default RGB SWAP */
		VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0); /* R-G-B */
		switch (pixel_format) {
		case DRM_FORMAT_BGR565:
			//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_BGR565\r\n", LOG_TAG, __func__, win);
			vioc_swap = 5;  /* B-G-R */
		case DRM_FORMAT_RGB565:
			//if(pixel_format != DRM_FORMAT_BGR565)
				//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_RGB565\r\n", LOG_TAG, __func__, win);
			vioc_fmt = TCC_LCDC_IMG_FMT_RGB565;
			break;
		case DRM_FORMAT_BGR888:
			//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_BGR888\r\n", LOG_TAG, __func__, win);
			vioc_swap = 5;  /* B-G-R */
		case DRM_FORMAT_RGB888:
			//if(pixel_format != DRM_FORMAT_BGR888)
				//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_RGB888\r\n", LOG_TAG, __func__, win);
			vioc_fmt = TCC_LCDC_IMG_FMT_RGB888_3;
			break;
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_ABGR8888:
			//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_ABGR8888\r\n", LOG_TAG, __func__, win);
			vioc_swap = 5;  /* B-G-R */
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_ARGB8888:
			//if(pixel_format != DRM_FORMAT_ABGR8888)
				//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_ARGB8888\r\n", LOG_TAG, __func__, win);
			vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
			break;
		case DRM_FORMAT_NV12:
			//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_NV12\r\n", LOG_TAG, __func__, win);
			vioc_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
			vioc_y2r = 1;
			break;
		case DRM_FORMAT_NV21:
			//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_NV21\r\n", LOG_TAG, __func__, win);
			vioc_fmt = TCC_LCDC_IMG_FMT_YUV420ITL1;
			vioc_y2r = 1;
			break;
		case DRM_FORMAT_YVU420:
			//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_YVU420\r\n", LOG_TAG, __func__, win);
		case DRM_FORMAT_YUV420:
			//if(pixel_format != DRM_FORMAT_YVU420)
				//dev_info(ctx->dev,"[INFO][%s] %s win(%d) with DRM_FORMAT_YUV420\r\n", LOG_TAG, __func__, win);
			vioc_fmt = TCC_LCDC_IMG_FMT_YUV420SP;
			vioc_y2r = 1;
			break;
		default:
			DRM_DEBUG_KMS("invalid pixel size so using unpacked 24bpp.\n");
			vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
			break;
		}

		VIOC_RDMA_SetImageY2REnable(pRDMA, vioc_y2r);
		VIOC_RDMA_SetImageRGBSwapMode(pRDMA, vioc_swap);
		VIOC_RDMA_SetImageOffset(pRDMA, vioc_fmt, width);
		VIOC_RDMA_SetImageFormat(pRDMA, vioc_fmt);

	} while(0);
}

static void lcd_atomic_begin(struct tcc_drm_crtc *crtc)
{
	//struct lcd_context *ctx = crtc->ctx;
}

static void lcd_atomic_flush(struct tcc_drm_crtc *crtc)
{
	//struct lcd_context *ctx = crtc->ctx;
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
	unsigned int val, size, offset;
	unsigned int win = plane->win;
	unsigned int cpp = fb->format->cpp[0];
	unsigned int pitch = fb->pitches[0];
	unsigned int enabled;
	/* TCC specific structure */
	volatile void __iomem *pWMIX;
	volatile void __iomem *pRDMA;

	do {
		if(ctx->keep_logo) {
			dev_info(ctx->dev, "[INFO][%s] %s line(%d) skip logo\r\n",
							LOG_TAG, __func__, __LINE__);
			ctx->keep_logo--;
			break;
		}
		if(win >= ctx->hw_data.rdma_counts) {
			pr_err("[ERROR][%s] %s line(%d) win(%d) is out of range (%d)\r\n",
				LOG_TAG, __func__, __LINE__, win, ctx->hw_data.rdma_counts);
			break;
		}
		if(ctx->hw_data.rdma[win].virt_addr == NULL) {
			pr_err("[ERROR][%s] %s line(%d) virtual address of win(%d) is NULL\r\n",
				LOG_TAG, __func__, __LINE__, win);
			break;
		}
		pWMIX = ctx->hw_data.wmixer.virt_addr;
		pRDMA = ctx->hw_data.rdma[win].virt_addr;

		offset = state->src.x * cpp;
		offset += state->src.y * pitch;

		dma_addr = tcc_drm_fb_dma_addr(fb, 0) + offset;
		if(dma_addr == (dma_addr_t)0) {
			DRM_ERROR("[ERROR][%s] %s line(%d) dma address of win(%d) is NULL\r\n",
				LOG_TAG,__func__, __LINE__, win);
			break;
		}
		if(upper_32_bits(dma_addr) > 0 ) {
			DRM_ERROR("[ERROR][%s] %s line(%d) dma address of win(%d) is out of range\r\n",
				LOG_TAG, __func__, __LINE__, win);
			break;
		}

		/* Swreset RDMAs to prevent fifo-underrun */
		VIOC_RDMA_GetImageEnable(ctx->hw_data.rdma[win].virt_addr, &enabled);
		if(!enabled) {
			if(get_vioc_type(ctx->hw_data.rdma[win].blk_num) == get_vioc_type(VIOC_RDMA)) {
				dev_info(ctx->dev,
					"[INFO][%s] %s win(%d) swreset RDMA, because rdma was disabled\r\n",
											LOG_TAG, __func__, win);
				VIOC_CONFIG_SWReset(ctx->hw_data.rdma[win].blk_num, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(ctx->hw_data.rdma[win].blk_num, VIOC_CONFIG_CLEAR);
			}
		}

		/* Using the pixel alpha */
		VIOC_RDMA_SetImageAlphaSelect(pRDMA, 1);
		if(fb->format->format == DRM_FORMAT_ARGB8888
			|| fb->format->format == DRM_FORMAT_ABGR8888)
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
		else
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, 0);

		/* buffer start address */
		val = lower_32_bits(dma_addr);
		//dev_info(ctx->dev, "[INFO][%s] %s win(%d) Base (0x%x, 0x%x, 0x%x)\r\n",
			//LOG_TAG, __func__, win, val, val+fb->offsets[1], val+fb->offsets[2]);
		switch(fb->format->format) {
		case DRM_FORMAT_YVU420:
			VIOC_RDMA_SetImageBase(pRDMA, val, val+fb->offsets[2], val+fb->offsets[1]);
			break;
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_YUV420:
			VIOC_RDMA_SetImageBase(pRDMA, val, val+fb->offsets[1], val+fb->offsets[2]);
			break;
		default:
			VIOC_RDMA_SetImageBase(pRDMA, val, 0, 0);
			break;
		}
		VIOC_RDMA_SetImageSize(pRDMA, state->crtc.w, state->crtc.h);

		/* buffer end address */
		size = pitch * state->crtc.h;
		val = lower_32_bits(dma_addr) + size;

		DRM_DEBUG_KMS("start addr = 0x%llx, end addr = 0x%x, size = 0x%x\n",
				dma_addr, val, size);
		DRM_DEBUG_KMS("ovl_width = %d, ovl_height = %d\n",
				state->crtc.w, state->crtc.h);

		lcd_win_set_pixfmt(ctx, win, fb->format->format, state->src.w);

		lcd_enable_video_output(ctx, win, true);
		#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
		mutex_lock(&ctx->chromakey_mutex);
		#endif
		VIOC_WMIX_SetPosition(pWMIX, win, state->crtc.x, state->crtc.y);
		VIOC_WMIX_SetUpdate(pWMIX);
		#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
		mutex_unlock(&ctx->chromakey_mutex);
		#endif
	} while(0);
}

static void lcd_disable_plane(struct tcc_drm_crtc *crtc,
			       struct tcc_drm_plane *plane)
{
	struct lcd_context *ctx = crtc->ctx;
	unsigned int win = plane->win;

	lcd_enable_video_output(ctx, win, false);
}

static void lcd_enable(struct tcc_drm_crtc *crtc)
{
	int ret;
	struct lcd_context *ctx = crtc->ctx;

	if(ctx->hw_data.connector_type == DRM_MODE_CONNECTOR_LVDS) {
		if (!test_and_set_bit(CRTC_FLAGS_PCLK_BIT, &ctx->crtc_flags)) {
			dev_info(ctx->dev,
				"[INFO][%s] %s line(%d) enable pclk %ldHz for LVDS\r\n",
						LOG_TAG, __func__, __LINE__,
						clk_get_rate(ctx->hw_data.ddc_clock));
			ret = clk_prepare_enable(ctx->hw_data.ddc_clock);
			if  (ret < 0) {
				dev_warn(ctx->dev,
				"[WARN][%s] %s line(%d) failed to enable the lcd clk\r\n",
								LOG_TAG, __func__, __LINE__);
			}
		}
	}

	if(!crtc->enabled) {
		dev_info(ctx->dev,
			"[INFO][%s] %s line(%d) turn on display dev\r\n",
							LOG_TAG, __func__, __LINE__);
		#if defined(CONFIG_PM)
		pm_runtime_get_sync(ctx->dev);
		#endif
		crtc->enabled = 1;
		VIOC_DISP_TurnOn(ctx->hw_data.display_device.virt_addr);
	}

	ctx->cnt_underrun = 0;
}

static void lcd_disable(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
	int i;
	for(i = 0;i < RDMA_MAX_NUM; i++) {
		lcd_enable_video_output(ctx, i, false);
	}

	dev_info(ctx->dev, "[INFO][%s] %s line(%d) turn off display dev\r\n",
							LOG_TAG, __func__, __LINE__);
	if(VIOC_DISP_Get_TurnOnOff(ctx->hw_data.display_device.virt_addr))
		VIOC_DISP_TurnOff(ctx->hw_data.display_device.virt_addr);

	if(crtc->enabled) {
		#if defined(CONFIG_PM)
		pm_runtime_put_sync(ctx->dev);
		#endif
	}

	if (test_and_clear_bit(CRTC_FLAGS_PCLK_BIT, &ctx->crtc_flags)) {
		dev_info(ctx->dev, "[INFO][%s] %s line(%d) disable pck\r\n",
							LOG_TAG, __func__, __LINE__);
		clk_disable_unprepare(ctx->hw_data.ddc_clock);
	}
	crtc->enabled = 0;
}

#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
static int lcd_set_chromakey(struct tcc_drm_crtc *crtc,
	unsigned int chromakey_layer,
	unsigned int chromakey_enable,
	struct drm_chromakey_t *value, struct drm_chromakey_t *mask)
{
	struct lcd_context *ctx = crtc->ctx;

	mutex_lock(&ctx->chromakey_mutex);
	if(crtc->enabled) {
		VIOC_WMIX_SetChromaKey(ctx->hw_data.wmixer.virt_addr, chromakey_layer,
			chromakey_enable,
			value->red, value->green, value->blue,
			mask->red, mask->green, mask->blue);
		VIOC_WMIX_SetUpdate(ctx->hw_data.wmixer.virt_addr);
	}
	/* store information */
	ctx->lcd_chromakeys[chromakey_layer].chromakey_enable = chromakey_enable;
	memcpy(&ctx->lcd_chromakeys[chromakey_layer].value, value, sizeof(*value));
	memcpy(&ctx->lcd_chromakeys[chromakey_layer].mask, mask, sizeof(*mask));
	mutex_unlock(&ctx->chromakey_mutex);
	return 0;
}

static int lcd_get_chromakey(struct tcc_drm_crtc *crtc,
	unsigned int chromakey_layer,
	unsigned int *chromakey_enable,
	struct drm_chromakey_t *value, struct drm_chromakey_t *mask)
{
	int ret = -1;
	struct lcd_context *ctx = crtc->ctx;
	mutex_lock(&ctx->chromakey_mutex);
	if(crtc->enabled) {
		VIOC_WMIX_GetChromaKey(ctx->hw_data.wmixer.virt_addr, chromakey_layer,
			chromakey_enable, &value->red,
			&value->green, &value->blue,
			&mask->red, &mask->green,
			&mask->blue);
		ret = 0;
	} else {
		*chromakey_enable = ctx->lcd_chromakeys[chromakey_layer].chromakey_enable;
		memcpy(value, &ctx->lcd_chromakeys[chromakey_layer].value, sizeof(*value));
		memcpy(mask, &ctx->lcd_chromakeys[chromakey_layer].mask, sizeof(*mask));
	}
	mutex_unlock(&ctx->chromakey_mutex);
	return ret;
}
#endif

static void lcd_mode_set_nofb(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *ctx = crtc->ctx;
        struct drm_crtc_state *state = crtc->base.state;
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	int i;
	#endif
	#if defined(CONFIG_ARCH_TCC805X)
	tcc_drm_crtc_set_display_timing(&crtc->base,
			&state->adjusted_mode, &ctx->hw_data);
	#endif
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	mutex_lock(&ctx->chromakey_mutex);
	for(i = 0; i < 3; i++) {
		VIOC_WMIX_SetChromaKey(ctx->hw_data.wmixer.virt_addr, i,
			ctx->lcd_chromakeys[i].chromakey_enable,
			ctx->lcd_chromakeys[i].value.red,
			ctx->lcd_chromakeys[i].value.green,
			ctx->lcd_chromakeys[i].value.blue,
			ctx->lcd_chromakeys[i].mask.red,
			ctx->lcd_chromakeys[i].mask.green,
			ctx->lcd_chromakeys[i].mask.blue);
		VIOC_WMIX_SetUpdate(ctx->hw_data.wmixer.virt_addr);
	}
	mutex_unlock(&ctx->chromakey_mutex);
	#endif
	if(ctx->hw_data.connector_type != DRM_MODE_CONNECTOR_LVDS) {
		int ret;
		if (!test_and_set_bit(CRTC_FLAGS_PCLK_BIT, &ctx->crtc_flags)) {
			dev_info(ctx->dev,
				"[INFO][%s] %s line(%d) enable pclk %ldHz\r\n",
						LOG_TAG, __func__, __LINE__,
						clk_get_rate(ctx->hw_data.ddc_clock));
			ret = clk_prepare_enable(ctx->hw_data.ddc_clock);
			if  (ret < 0) {
				dev_warn(ctx->dev,
				"[WARN][%s] %s line(%d) failed to enable the lcd clk\r\n",
								LOG_TAG, __func__, __LINE__);
			}
		}
	}
}

static const struct tcc_drm_crtc_ops lcd_crtc_ops = {
	.enable = lcd_enable, 	/* drm_crtc_helper_funcs->atomic_enable */
	.disable = lcd_disable, /* drm_crtc_helper_funcs->atomic_disable */
	.enable_vblank = lcd_enable_vblank,
	.disable_vblank = lcd_disable_vblank,
	.atomic_begin = lcd_atomic_begin,
	.update_plane = lcd_update_plane,
	.disable_plane = lcd_disable_plane,
	.atomic_flush = lcd_atomic_flush,
	.atomic_check = lcd_atomic_check,
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	.set_chromakey = lcd_set_chromakey,
	.get_chromakey = lcd_get_chromakey,
	#endif
	.mode_set_nofb = lcd_mode_set_nofb,
};

static int lcd_bind(struct device *dev, struct device *master, void *data)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct drm_plane *primary = NULL;
	struct drm_plane *cursor = NULL;
	const uint32_t *formats_list;
	int formats_list_size;
	int i, ret = 0;

	ctx->drm_dev = drm_dev;
	/* Activate CRTC clocks */
	if (!test_and_set_bit(CRTC_FLAGS_VCLK_BIT, &ctx->crtc_flags)) {
		ret = clk_prepare_enable(ctx->hw_data.vioc_clock);
		if (ret < 0) {
			dev_err(dev,
				"[ERROR][%s] %s line(%d) failed to enable the bus clk\n",
								LOG_TAG, __func__, __LINE__);
			return -1;
		}
	}
	for (i = 0; i < ctx->hw_data.rdma_counts; i++) {
		if(VIOC_RDMA_IsVRDMA(ctx->hw_data.rdma[i].blk_num)) {
			formats_list = lcd_formats_vrdma;
			formats_list_size = ARRAY_SIZE(lcd_formats_vrdma);
		} else {
			formats_list = lcd_formats;
			formats_list_size = ARRAY_SIZE(lcd_formats);
		}

		ret = tcc_plane_init(drm_dev,
					&ctx->planes[i].base,
					DRM_PLANE_TYPE(ctx->hw_data.rdma_plane_type[i]),
					formats_list, formats_list_size);
		if(ret) {
			dev_err(dev,
				"[ERROR][%s] %s line(%d) failed to initizliaed the planes\n",
									LOG_TAG, __func__, __LINE__);
			return ret;
		}

		ctx->planes[i].win = i;
		if(DRM_PLANE_TYPE(ctx->hw_data.rdma_plane_type[i]) == DRM_PLANE_TYPE_PRIMARY) {
			primary = &ctx->planes[i].base;
		}

		if(DRM_PLANE_TYPE(ctx->hw_data.rdma_plane_type[i]) == DRM_PLANE_TYPE_CURSOR) {
			cursor = &ctx->planes[i].base;
		}
	}

	ctx->crtc = tcc_drm_crtc_create(drm_dev, primary, cursor,
			ctx->data->output_type, &lcd_crtc_ops, ctx);
	if (IS_ERR(ctx->crtc))
		return PTR_ERR(ctx->crtc);

	if (ctx->encoder)
		tcc_dpi_bind(drm_dev, ctx->encoder, &ctx->hw_data);

	return ret;
}

static void lcd_unbind(struct device *dev, struct device *master,
			void *data)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);

	if (ctx->encoder)
		tcc_dpi_remove(ctx->encoder);

	/* Deactivate CRTC clock */
	if (test_and_clear_bit(CRTC_FLAGS_VCLK_BIT, &ctx->crtc_flags)) {
		clk_disable_unprepare(ctx->hw_data.vioc_clock);
	}
}

static const struct component_ops lcd_component_ops = {
	.bind	= lcd_bind,
	.unbind = lcd_unbind,
};



static int lcd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lcd_context *ctx;
	int ret = 0;

	if (!dev->of_node) {
		dev_err(dev,
			"[ERROR][%s] %s line(%d) failed to get the device node\n",
							LOG_TAG, __func__, __LINE__);
		return -ENODEV;
	}
	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		dev_err(dev,
			"[ERROR][%s] %s line(%d) ctx is NULL\n",
					LOG_TAG, __func__, __LINE__);
		return -ENOMEM;
	}

	ctx->dev = dev;
	ctx->data = (struct tcc_drm_device_data*)of_device_get_match_data(&pdev->dev);
	if(ctx->data == NULL) {
		dev_err(dev,
			"[ERROR][%s] %s line(%d) failed to get match data\r\n",
					LOG_TAG, __func__, __LINE__);
		return -ENOMEM;
	}
	if(tcc_drm_address_dt_parse(pdev, &ctx->hw_data, ctx->data->version) < 0) {
		dev_err(dev,
			"[ERROR][%s] %s line(%d) failed to parse device tree\n",
							LOG_TAG, __func__, __LINE__);
		return ret;
	}
	switch(ctx->data->output_type) {
	case TCC_DISPLAY_TYPE_LCD:
		ctx->data->name = kstrdup("drm_lcd", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_LCD_VIC)
		ctx->hw_data.vic = CONFIG_DRM_TCC_LCD_VIC;
		#endif
		break;
	case TCC_DISPLAY_TYPE_EXT:
		ctx->data->name = kstrdup("drm_ext", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_EXT_VIC)
		ctx->hw_data.vic = CONFIG_DRM_TCC_EXT_VIC;
		#endif
		break;
	case TCC_DISPLAY_TYPE_THIRD:
		ctx->data->name = kstrdup("drm_third", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_THIRD_VIC)
		ctx->hw_data.vic = CONFIG_DRM_TCC_THIRD_VIC;
		#endif
		break;
	case TCC_DISPLAY_TYPE_FOURTH:
		ctx->data->name = kstrdup("drm_fourth", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_FOURTH_VIC)
		ctx->hw_data.vic = CONFIG_DRM_TCC_FOURTH_VIC;
		#endif
		break;
	default:
		return -ENODEV;
	}

	dev_dbg(dev, "[DEBUG][%s] %s line(%d) %s start probe with pdev(%p)\r\n",
				LOG_TAG, __func__, __LINE__, ctx->data->name, pdev);

	spin_lock_init(&ctx->irq_lock);

	/* Disable & clean interrupt */
	vioc_intr_disable(ctx->hw_data.display_device.irq_num,
		  get_vioc_index(ctx->hw_data.display_device.blk_num), VIOC_DISP_INTR_DISPLAY);
	vioc_intr_clear(ctx->hw_data.display_device.blk_num, VIOC_DISP_INTR_DISPLAY);

	init_waitqueue_head(&ctx->wait_vsync_queue);
	atomic_set(&ctx->wait_vsync_event, 0);
	platform_set_drvdata(pdev, ctx);
	ctx->encoder = tcc_dpi_probe(dev, &ctx->hw_data);
	if (IS_ERR(ctx->encoder))
		return PTR_ERR(ctx->encoder);
	#if defined(CONFIG_PM)
	pm_runtime_enable(dev);
	#endif
	ret = component_add(dev, &lcd_component_ops);
	if (ret) {
		dev_err(dev, "[ERROR][%s] %s line(%d) failed to component_add\r\n",
							LOG_TAG, __func__, __LINE__);
		goto err_disable_pm_runtime;
	}
	#if defined(CONFIG_DRM_TCC_KEEP_LOGO)
	#if defined(CONFIG_DRM_FBDEV_EMULATION)
	/* If fbdev emulation was selected then FB core calls set_par */
	ctx->keep_logo = 1;
	#if defined(CONFIG_LOGO) && (!defined(CONFIG_ARCH_TCC897X) && !defined(CONFIG_ARCH_TCC803X))
	/* If logo was selected then FB core calls set_par twice */
	ctx->keep_logo++;
	#endif // CONFIG_LOGO
	#endif // CONFIG_DRM_FBDEV_EMULATION
	#endif // CONFIG_DRM_TCC_KEEP_LOGO

	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	mutex_init(&ctx->chromakey_mutex);
	#endif

	return ret;

err_disable_pm_runtime:
	#if defined(CONFIG_PM)
	pm_runtime_disable(dev);
	#endif

	return ret;
}

static int lcd_remove(struct platform_device *pdev)
{
	struct lcd_context *ctx = platform_get_drvdata(pdev);
	#if defined(CONFIG_PM)
	pm_runtime_disable(&pdev->dev);
	#endif

	component_del(&pdev->dev, &lcd_component_ops);

	kfree(ctx->data->name);
	devm_kfree(&pdev->dev, ctx);

	return 0;
}

#ifdef CONFIG_PM
static int tcc_lcd_suspend(struct device *dev)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);
	dev_info(dev, "[ERROR][%s] %s line(%d) \r\n", LOG_TAG, __func__, __LINE__);

	if (test_bit(CRTC_FLAGS_VCLK_BIT, &ctx->crtc_flags)) {
		dev_info(dev, "[INFO][%s] %s line(%d) disable vclk\r\n",
						LOG_TAG, __func__, __LINE__);
		clk_disable_unprepare(ctx->hw_data.vioc_clock);
	}

	return 0;
}

static int tcc_lcd_resume(struct device *dev)
{
	struct lcd_context *ctx = dev_get_drvdata(dev);
	dev_info(dev, "[ERROR][%s] %s line(%d) \r\n", LOG_TAG, __func__, __LINE__);
	if (test_bit(CRTC_FLAGS_VCLK_BIT, &ctx->crtc_flags)) {
		dev_info(dev, "[INFO][%s] %s line(%d) enable vclk\r\n", LOG_TAG, __func__, __LINE__);
		clk_prepare_enable(ctx->hw_data.vioc_clock);
	}
	return 0;
}
#endif

static const struct dev_pm_ops tcc_lcd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_lcd_suspend, tcc_lcd_resume)
};
