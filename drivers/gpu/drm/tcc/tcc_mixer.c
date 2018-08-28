/*
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
 * Seung-Woo Kim <sw0312.kim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * Based on drivers/media/video/s5p-tv/mixer_reg.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drmP.h>

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <linux/component.h>

#include <drm/tcc_drm.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"
#include "tcc_drm_plane.h"
#include "tcc_mixer.h"

#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_global.h>

#define MIXER_WIN_UI		0
#define MIXER_WIN_VIDEO		3
#define MIXER_WIN_NR		4
#define MIXER_DEFAULT_WIN	0

struct mixer_resources {
	int				id;
	int				irq;
	void __iomem	*ddc_reg;
	void __iomem	*wmix_reg;
	void __iomem	*ddi_reg;
	void __iomem	*vioc_reg;
	struct clk		*clk;
};

struct mixer_context {
	struct platform_device	*pdev;
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct tcc_drm_crtc		*crtc;
	struct tcc_drm_plane	planes[MIXER_WIN_NR];
	struct drm_display_mode	drm_display_mode;

	int				pipe;
	bool			interlace;
	bool			powered;
	unsigned long	irq_flags;

	struct mutex			mixer_mutex;
	struct mixer_resources	mixer_res;
	wait_queue_head_t		wait_vsync_queue;
	atomic_t				wait_vsync_event;
};

struct mixer_drv_data {
	bool					is_vp_enabled;
	bool					has_sclk;
};

extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy, unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);
static void mixer_path_reset(struct mixer_context *ctx)
{
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	struct tcc_drm_plane *plane;
	int index;

	DRM_DEBUG_KMS(" \r\n");

	VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_WMIXER, mixer_res->id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_WMIXER, mixer_res->id, VIOC_CONFIG_CLEAR);

	for(index = 0; index < MIXER_WIN_NR; index++) {
		plane = &ctx->planes[index];
		if(plane->sc_reg) {
			VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_SCALER, index, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_SCALER, index, VIOC_CONFIG_CLEAR);
		}

		if(plane->dma_reg) {
			VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_RDMA, index, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_RDMA, index, VIOC_CONFIG_CLEAR);
		}
	}
}

static void mixer_commit(struct tcc_drm_crtc *crtc)
{
	struct drm_display_mode *mode = NULL;
	struct mixer_context *ctx = crtc->ctx;
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	volatile unsigned long bits, reg_val;
	unsigned int width, height;
	int vactive, loop;

	DRM_DEBUG_KMS(" \r\n");
	if (!ctx->powered) {
		pr_err("power is disabled\r\n");
		return;
	}

	// The adjusted_mode was stored in crtc->mode by the function tcc_drm_crtc_mode_set
	mode = &crtc->base.mode;

	for(loop = 0; loop < MIXER_WIN_NR; loop++) {
		if(ctx->planes[loop].enabled)
			VIOC_RDMA_SetImageDisable(ctx->planes[loop].dma_reg);
	}

	if(VIOC_DISP_Get_TurnOnOff(mixer_res->ddc_reg)) {
		DRM_INFO("Device%d Turn-off\r\n", mixer_res->id);
		VIOC_DISP_TurnOff(mixer_res->ddc_reg);
		if(!VIOC_DISP_Wait_DisplayDone(mixer_res->ddc_reg))
			DRM_INFO("Device%d Turn-off :: Timeout\r\n", mixer_res->id);
	}

	reg_val = mode->hdisplay -1;
	reg_val |= ((mode->hsync_end - mode->hsync_start)- 1) << 16;
	DRM_DEBUG_KMS("LPC (0x%lu), LPW (0x%lu)\r\n", reg_val & 0xFFFF, reg_val >> 16);
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x0C));

	reg_val = (mode->hsync_start - mode->hdisplay) - 1;
	reg_val |= ((mode->htotal - mode->hsync_end) - 1) << 16;
	DRM_DEBUG_KMS("LEWC (0x%lu), LSWC (0x%lu)\r\n", reg_val & 0xFFFF, reg_val >> 16);
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x10));

	vactive = mode->vdisplay;
	if( mode->flags & DRM_MODE_FLAG_3D_FRAME_PACKING ) {
		vactive = (vactive  << 1) + mode->vtotal;
	}

	if ( mode->flags & DRM_MODE_FLAG_INTERLACE) {
		reg_val = vactive -1;
		reg_val |= ((((mode->vsync_end >> 1) - (mode->vsync_start >> 1)) << 1)- 1) << 16;
	} else {

		reg_val = vactive -1;
		reg_val |= ((mode->vsync_end - mode->vsync_start)- 1) << 16;
	}

	DRM_DEBUG_KMS("FLC (0x%lu), FPW (0x%lu)\r\n", reg_val & 0xFFFF, reg_val >> 16);
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x14));
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x1C));

	if ( mode->flags & DRM_MODE_FLAG_INTERLACE) {
		reg_val = (((mode->vsync_start >> 1) - (mode->vdisplay >> 1)) << 1) - 1;
		reg_val |= (((mode->vtotal >> 1) - (mode->vsync_end >> 1)) -1) << 16;
	} else {
		reg_val = (mode->vsync_start - mode->vdisplay)- 1;
		reg_val |= ((mode->vtotal - mode->vsync_end) - 1) << 16;
	}

	DRM_DEBUG_KMS("FEWC (0x%lu), FSWC (0x%lu)\r\n", reg_val & 0xFFFF, reg_val >> 16);
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x18));

	if ( mode->flags & DRM_MODE_FLAG_INTERLACE) {
		reg_val = (((mode->vsync_start >> 1) - (mode->vdisplay >> 1)) << 1) - 2;
		reg_val |= (((mode->vtotal >> 1) - (mode->vdisplay >> 1))) << 16;
	}

	DRM_DEBUG_KMS("FEWC2 (0x%lu), FSWC2 (0x%lu)\r\n", reg_val & 0xFFFF, reg_val >> 16);
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x20));

	reg_val = 0;
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x58));
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x5C));

	reg_val = 0;
	reg_val |= (1 << 5);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		reg_val |= (1 << 6);

	if (!(mode->flags & DRM_MODE_FLAG_INTERLACE))
		reg_val |= (1 << 7);

	if (mode->flags & DRM_MODE_FLAG_DBLCLK) {
		width = (mode->hdisplay >> 1);
		height = mode->vdisplay;
		reg_val |= (1 << 8);
	} else {
		width = mode->hdisplay;
		height = mode->vdisplay;
	}

	reg_val |= (0 << 9);

	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		reg_val |= (1 << 12);

	if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		reg_val |= (1 << 13);

	reg_val |= (1 << 15);
	reg_val |= (23 << 16);
	reg_val |= (1 << 27);
	reg_val |= (1 << 5);
	if (!(mode->flags & DRM_MODE_FLAG_INTERLACE)) {
		reg_val |= (1 << 26);
	}
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x00));

	reg_val = height << 16 | width;
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x4C));

	reg_val = 0;
	bits = ioread32((void*)(mixer_res->ddc_reg + 0x04));
	bits &= ~(0x7 << 2);
	bits |= (reg_val << 2);
	iowrite32(bits, (void*)(mixer_res->ddc_reg + 0x04));

	VIOC_WMIX_SetSize(mixer_res->wmix_reg, width, height);
	VIOC_WMIX_SetUpdate(mixer_res->wmix_reg);

	bits = ioread32((void*)(mixer_res->ddi_reg + 0x00));
	reg_val = bits | (1 << 16);
	iowrite32(reg_val, (void*)(mixer_res->ddi_reg + 0x00));

	bits = ioread32((void*)(mixer_res->ddc_reg + 0x00));
	reg_val = bits | 1;
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x00));

	for(loop = 0; loop < MIXER_WIN_NR; loop++) {
		if(ctx->planes[loop].enabled) {
			VIOC_RDMA_SetImageSize(ctx->planes[loop].dma_reg, ctx->planes[loop].fb_width, ctx->planes[loop].fb_height);
			VIOC_RDMA_SetImageEnable(ctx->planes[loop].dma_reg);
		}
	}
}

void mixer_disable(struct tcc_drm_crtc *crtc)
{
	struct tcc_drm_plane *plane;
	struct mixer_context *ctx = crtc->ctx;
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	int i;

	DRM_DEBUG_KMS(" \r\n");
	if (!ctx->powered) {
		DRM_ERROR("Mixer is power-off\r\n");
		return;
	}

	for( i=0; i < MIXER_WIN_NR; i++) {
		plane = &ctx->planes[i];
		if(plane->enabled) {
			if(plane->dma_reg) {
				DRM_DEBUG_KMS("Disable RDMA_%d\r\n", i);
				VIOC_RDMA_SetImageDisableNW(plane->dma_reg);
			}
			plane->enabled = false;
		}
	}

	if(VIOC_DISP_Get_TurnOnOff(mixer_res->ddc_reg)) {
		DRM_DEBUG_KMS("Disable Display Device\r\n");
		VIOC_DISP_TurnOff(mixer_res->ddc_reg);
		VIOC_DISP_sleep_DisplayDone(mixer_res->ddc_reg);
	}

	iowrite32((1 << (mixer_res->id + 20)),(void*)(mixer_res->vioc_reg+0xDC));
	iowrite32((0 << (mixer_res->id + 20)), (void*)(mixer_res->vioc_reg+0xDC));

	DRM_INFO("Display Device%d SW Reset\r\n", mixer_res->id);

	mixer_path_reset(ctx);
}

void mixer_prepare(struct tcc_drm_crtc *crtc)
{
	struct mixer_context *ctx = crtc->ctx;

	DRM_DEBUG_KMS(" \r\n");

	if(!ctx->powered)
		mixer_disable(crtc);
}

static irqreturn_t mixer_irq_handler(int irq, void *arg)
{
	struct mixer_context *ctx = arg;
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	unsigned int dispblock_status = 0;

	if (ctx == NULL || mixer_res == NULL) {
		pr_err("%s irq: %d arg:%p \n",__func__, irq, arg);
		return IRQ_HANDLED;
	}

	/* Get TCC VIOC block status register */
	dispblock_status = ioread32((void *)(mixer_res->ddc_reg + 0x50));
	if(dispblock_status & (1 << VIOC_DISP_INTR_RU))
	{
		iowrite32(((1<< VIOC_DISP_INTR_RU)&VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg + 0x50));

		/* check the crtc is detached already from encoder */
		if (ctx->pipe < 0 || ctx->drm_dev == NULL)
			goto out;

		drm_handle_vblank(ctx->drm_dev, ctx->pipe);
		tcc_drm_crtc_finish_pageflip(ctx->drm_dev, ctx->pipe);

		/* set wait vsync event to zero and wake up queue. */
		if (atomic_read(&ctx->wait_vsync_event)) {
			atomic_set(&ctx->wait_vsync_event, 0);
			wake_up(&ctx->wait_vsync_queue);
		}
#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
		if (VIOC_DISP_Get_TurnOnOff(mixer_res->ddc_reg)) {
			if (dispblock_status & (1 << VIOC_DISP_INTR_FU)) {
				pr_err("%s FIFO UNDERUN STATUS:0x%x \n",__func__, dispblock_status);
				iowrite32(((1<< VIOC_DISP_INTR_FU)&VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg + 0x50));
			}
		} else {
			/*Don't care FIFO under-run, when DISP is off.*/
		}
#endif
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_DD)) {
		iowrite32(((1<< VIOC_DISP_INTR_DD)&VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg + 0x50));

		/* TODO */
		/*
		 *if (fbdev->pdata.Mdp_data.disp_dd_sync.state == 0) {
		 *    fbdev->pdata.Mdp_data.disp_dd_sync.state = 1;
		 *    wake_up_interruptible(&fbdev->pdata.Mdp_data.disp_dd_sync.waitq);
		 *}
		 *
		 *pr_info("%s DISABEL DONE Lcdc_num:%d 0x%p  STATUS:0x%x  \n",
		 *        __func__,fbdev->pdata.lcdc_number, fbdev->pdata.Mdp_data.ddc_info.virt_addr, dispblock_status);
		 */
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_SREQ))
		iowrite32(((1<< VIOC_DISP_INTR_SREQ)&VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg+0x50));

out:
	return IRQ_HANDLED;
}

static int mixer_parse_dt(struct mixer_context *ctx)
{
	struct device_node *np_vioc = NULL;
	struct mixer_resources *mixer_res = &ctx->mixer_res;

	np_vioc = of_find_compatible_node(NULL, NULL, "telechips,vioc-fb");
	if (of_property_read_u32(np_vioc, "telechips,fbdisplay_num", &mixer_res->id)) {
		DRM_ERROR("could not find tcc fbdisplay_num\n");
		return -ENODEV;
	}

	if ( mixer_res->id == 0 ) {
		struct device_node *np_dummy = NULL, *np_fb = NULL;
		int index, i;

		np_fb = of_find_node_by_name(np_vioc, "fbdisplay0");
		if (!np_fb) {
			DRM_ERROR("could not find tcc fb node number %d\n", mixer_res->id);
			return -ENODEV;
		}

		np_dummy = of_parse_phandle(np_fb, "telechips,rdma", 0);
		if (!np_dummy) {
			DRM_ERROR("could not find telechips,rdma node\n");
			return -ENODEV;
		} else {
			struct device_node *np_scaler = of_find_compatible_node(NULL, NULL, "telechips,scaler");
			if(!np_scaler) {
				DRM_ERROR("could not find telechips,scaler node\n");
				return -ENODEV;
			}

			for (i = 0; i < MIXER_WIN_NR; i++) {
				of_property_read_u32_index(np_fb, "telechips,rdma", i + 1, &index);
				ctx->planes[i].dma_reg = of_iomap(np_dummy, index);

				if((i == MIXER_WIN_UI) || (i == MIXER_WIN_VIDEO)) {
					ctx->planes[i].sc_reg = of_iomap(np_scaler, i);
					DRM_DEBUG_KMS("ctx->planes[%d]->dma_reg:0x%08x\n", i, (unsigned int)ctx->planes[i].sc_reg);
				}
				DRM_DEBUG_KMS("ctx->planes[%d]->dma_reg:0x%08x[%d]\n", i, (unsigned int)ctx->planes[i].dma_reg, index);
			}
		}
		DRM_DEBUG_KMS("DispNum:%d, ddc_reg:0x%p\n", mixer_res->id, mixer_res->ddc_reg);
	} else {
		DRM_ERROR("TCC-DRM do not support display path 1\n");
		return -ENODEV;
	}

	return 0;
}

static int mixer_resources_init(struct mixer_context *ctx)
{
	struct device *dev = &ctx->pdev->dev;
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	struct resource *res = NULL;
	int ret;

	mixer_res->clk = devm_clk_get(dev, "mixer");
	if(IS_ERR(mixer_res->clk)) {
		dev_err(dev, "failed to get clock 'mixer'\n");
		return -ENODEV;
	}

	res = platform_get_resource(ctx->pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "failed to get 'ddc' memory resource \n");
		return -ENXIO;
	}

	mixer_res->ddc_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if(mixer_res->ddc_reg == NULL) {
		dev_err(dev, "failed to map 'ddc' register \n");
		return -ENXIO;
	}

	res = platform_get_resource(ctx->pdev, IORESOURCE_MEM, 1);
	if (res == NULL) {
		dev_err(dev, "failed to get 'wmix' memory resource \n");
		return -ENXIO;
	}

	mixer_res->wmix_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if(mixer_res->wmix_reg == NULL) {
		dev_err(dev, "failed to map 'wmix' register \n");
		return -ENXIO;
	}

	res = platform_get_resource(ctx->pdev, IORESOURCE_MEM, 2);
	if (res == NULL) {
		dev_err(dev, "failed to get 'ddi' memory resource \n");
		return -ENXIO;
	}

	mixer_res->ddi_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if(mixer_res->ddi_reg == NULL) {
		dev_err(dev, "failed to map 'ddi' register \n");
		return -ENXIO;
	}

	res = platform_get_resource(ctx->pdev, IORESOURCE_MEM, 3);
	if (res == NULL) {
		dev_err(dev, "failed to get 'vioc' memory resource \n");
		return -ENXIO;
	}

	mixer_res->vioc_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if(mixer_res->vioc_reg == NULL) {
		dev_err(dev, "failed to map 'vioc' register \n");
		return -ENXIO;
	}

	res = platform_get_resource(ctx->pdev, IORESOURCE_IRQ, 0);
	if(res == NULL) {
		dev_err(dev, "failed to get interrupt resource \n");
		return -ENXIO;
	}

	ret = devm_request_irq(dev, res->start, mixer_irq_handler,
						IRQF_SHARED, "drm_mixer", ctx);
	if (ret) {
		dev_err(dev, "failed to request interrupt \n");
		return ret;
	}

	mixer_res->irq = res->start;

	return 0;
}

static int mixer_initialize(struct mixer_context *ctx,
			struct drm_device *dev)
{
	int ret;
	struct tcc_drm_private *priv;
	priv = dev->dev_private;

	ctx->drm_dev = dev;
	ctx->pipe = priv->pipe++;

	/* acquire resources: regs, irqs, clocks */
	ret = mixer_resources_init(ctx);
	if (ret)
		DRM_ERROR("mixer_resources_init failed ret=%d\n", ret);

	ret = mixer_parse_dt(ctx);
	if (ret)
		DRM_ERROR("mixer_parse_dt faile ret=%d\n", ret);

	return ret;
}

static void mixer_ctx_remove(struct mixer_context *ctx)
{
	/* TODO */
}

static int mixer_enable_vblank(struct tcc_drm_crtc *crtc)
{
	struct mixer_context *ctx = crtc->ctx;
	struct mixer_resources *mixer_res = &ctx->mixer_res;

	DRM_DEBUG_KMS(" \r\n");

	if (!ctx->powered)
		return -EPERM;

	if (!test_and_set_bit(0, &ctx->irq_flags)) {
		iowrite32(((VIOC_DISP_INTR_DISPLAY)&VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg + 0x50));

		iowrite32(ioread32((void *)(mixer_res->ddc_reg+0x54)) & ~(VIOC_DISP_INTR_DISPLAY & VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg+0x54));
		iowrite32((1 << mixer_res->id), (void *)(mixer_res->vioc_reg+0x20));
	}
	return 0;
}

static void mixer_disable_vblank(struct tcc_drm_crtc *crtc)
{
	struct mixer_context *ctx = crtc->ctx;
	struct mixer_resources *mixer_res = &ctx->mixer_res;

	DRM_DEBUG_KMS(" \r\n");

	if (test_and_clear_bit(0, &ctx->irq_flags)) {
		iowrite32(ioread32((void *)(mixer_res->ddc_reg+0x54)) | (VIOC_DISP_INTR_DISPLAY & VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg+0x54));
		if((ioread32((void *)(mixer_res->ddc_reg+0x54))&VIOC_DISP_INT_MASK) == VIOC_DISP_INT_MASK)
			iowrite32((1 << mixer_res->id), (void *)(mixer_res->vioc_reg+0x20));

		iowrite32(((VIOC_DISP_INTR_DISPLAY)&VIOC_DISP_INT_MASK), (void *)(mixer_res->ddc_reg + 0x50));
	}
}

static void mixer_video_buffer(struct mixer_context *ctx, int win)
{
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	struct tcc_drm_plane *plane;
	if(ctx) {
		plane = &ctx->planes[win];
		if(plane) {
			unsigned int addr_Y = 0, addr_U = 0, addr_V = 0;
			if(plane->pixel_format == DRM_FORMAT_NV12) {
				tccxxx_GetAddress(TCC_LCDC_IMG_FMT_YUV420ITL0, (unsigned int)plane->dma_addr[0],
					plane->fb_width, plane->fb_height,
					0, 0, &addr_Y, &addr_U, &addr_V);
			} else {
				addr_Y = (unsigned int)plane->dma_addr[0];
				addr_U = addr_V = 0;
			}

			VIOC_RDMA_SetImageBase(plane->dma_reg, addr_Y, addr_U, addr_V);
			VIOC_RDMA_SetImageSize(plane->dma_reg, plane->fb_width, plane->fb_height);

			if(plane->pixel_format == DRM_FORMAT_NV12) {
				VIOC_RDMA_SetImageOffset(plane->dma_reg, TCC_LCDC_IMG_FMT_YUV420ITL0, plane->fb_width);
				VIOC_RDMA_SetImageFormat(plane->dma_reg, TCC_LCDC_IMG_FMT_YUV420ITL0);
				VIOC_RDMA_SetImageY2RMode(plane->dma_reg, 0x2);
				VIOC_RDMA_SetImageY2REnable(plane->dma_reg, 1);
			} else {
				if (plane->bpp == 16) {
					VIOC_RDMA_SetImageOffset(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB565, plane->fb_width);
					VIOC_RDMA_SetImageFormat(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB565);
				} else {
					VIOC_RDMA_SetImageOffset(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB888, plane->fb_width);
					VIOC_RDMA_SetImageFormat(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB888);
				}
				VIOC_RDMA_SetImageY2REnable(plane->dma_reg, 0);
			}

			if(plane->sc_reg) {
				unsigned int width, height;
				VIOC_DISP_GetSize(mixer_res->ddc_reg, &width, &height);
				if(width < (plane->crtc_x + plane->crtc_width))
					width -= plane->crtc_x;
				else
					width = plane->crtc_width;

				if(height < (plane->crtc_y + plane->crtc_height))
					height -= plane->crtc_y;
				else
					height = plane->crtc_height;

				VIOC_SC_SetBypass(plane->sc_reg, 0/* OFF */);
				VIOC_SC_SetDstSize(plane->sc_reg, width, height);
				VIOC_SC_SetOutSize(plane->sc_reg, width, height);
				VIOC_SC_SetOutPosition(plane->sc_reg, 0, 0);
				VIOC_SC_SetUpdate(plane->sc_reg);
			}
			VIOC_RDMA_SetImageEnable(plane->dma_reg);

			VIOC_WMIX_SetPosition(mixer_res->wmix_reg, win, plane->crtc_x, plane->crtc_y);
			VIOC_WMIX_SetUpdate(mixer_res->wmix_reg);
		}
	}
}

static void mixer_grap_buffer(struct mixer_context *ctx, int win)
{
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	struct tcc_drm_plane *plane = NULL;
	if(ctx) {
		unsigned int offset;
		plane = &ctx->planes[win];
		if(plane) {
			offset = plane->src_x * (plane->bpp >> 3);
			offset += plane->src_y * plane->pitch;

			VIOC_RDMA_SetImageBase(plane->dma_reg, (unsigned int)(plane->dma_addr[0] + offset), 0, 0);
			VIOC_RDMA_SetImageSize(plane->dma_reg, plane->fb_width, plane->fb_height);

			if (plane->bpp == 16) {
				VIOC_RDMA_SetImageOffset(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB565, plane->fb_width);
				VIOC_RDMA_SetImageFormat(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB565);
			} else {
				VIOC_RDMA_SetImageOffset(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB888, plane->fb_width);
				VIOC_RDMA_SetImageFormat(plane->dma_reg, TCC_LCDC_IMG_FMT_RGB888);

				VIOC_RDMA_SetImageAlphaSelect(plane->dma_reg, 1);
				VIOC_RDMA_SetImageAlphaEnable(plane->dma_reg, 1);
			}

			if(plane->sc_reg) {
				unsigned int width, height;
				VIOC_DISP_GetSize(mixer_res->ddc_reg, &width, &height);
				if(width < (plane->crtc_x + plane->crtc_width))
					width -= plane->crtc_x;
				else
					width = plane->crtc_width;

				if(height < (plane->crtc_y + plane->crtc_height))
					height -= plane->crtc_y;
				else
					height = plane->crtc_height;

				VIOC_SC_SetBypass(plane->sc_reg, 0/* OFF */);
				VIOC_SC_SetDstSize(plane->sc_reg, width, height);
				VIOC_SC_SetOutSize(plane->sc_reg, width, height);
				VIOC_SC_SetOutPosition(plane->sc_reg, 0, 0);
				VIOC_SC_SetUpdate(plane->sc_reg);
			} else {
				VIOC_RDMA_SetImageSize(plane->dma_reg,
					plane->fb_width == plane->crtc_width ? plane->fb_width : plane->crtc_width,
					plane->fb_height == plane->crtc_height ? plane->fb_height : plane->crtc_height);
			}
			VIOC_RDMA_SetImageEnable(plane->dma_reg);

			VIOC_WMIX_SetPosition(mixer_res->wmix_reg, win, plane->crtc_x, plane->crtc_y);
			VIOC_WMIX_SetUpdate(mixer_res->wmix_reg);
		}
	}
}

static void mixer_win_commit(struct tcc_drm_crtc *crtc, unsigned int win)
{
	struct mixer_context *ctx = crtc->ctx;
	struct mixer_resources *mixer_res = &ctx->mixer_res;

	DRM_DEBUG_KMS("win: %d\n", win);

	mutex_lock(&ctx->mixer_mutex);
	if (!ctx->powered) {
		mutex_unlock(&ctx->mixer_mutex);
		return;
	}
	mutex_unlock(&ctx->mixer_mutex);

	if (win < 0 || win >= MIXER_WIN_NR) {
		DRM_ERROR("out of win (%d)\r\n", win);
		return;
	}

	if(ctx->planes[win].sc_reg) {
		if(VIOC_DISP_Get_TurnOnOff(mixer_res->ddc_reg)) {
			VIOC_PlugInOutCheck plugin_state;
			VIOC_CONFIG_Device_PlugState(win, &plugin_state);
			if(!plugin_state.enable || plugin_state.connect_statue != VIOC_PATH_CONNECTED)
				VIOC_CONFIG_PlugIn(win, win);
		}
	}

	if(win == MIXER_WIN_VIDEO)
		mixer_video_buffer(ctx, win);
	else
		mixer_grap_buffer(ctx, win);

	DRM_DEBUG_KMS("base addr = 0x%08x, size = 0x%08x\n",
			(unsigned int)ctx->planes[win].dma_addr[0],
			(unsigned int)(ctx->planes[win].fb_width * ctx->planes[win].fb_height * (ctx->planes[win].bpp >> 3)));
	DRM_DEBUG_KMS("ovl_x = %d, ovl_y = %d, ovl_width = %d, ovl_height = %d\n",
			ctx->planes[win].crtc_x, ctx->planes[win].crtc_y,
			ctx->planes[win].crtc_width, ctx->planes[win].crtc_height);

	ctx->planes[win].enabled = true;
}

static void mixer_win_disable(struct tcc_drm_crtc *crtc, unsigned int win)
{
	struct mixer_context *ctx = crtc->ctx;
	struct mixer_resources *mixer_res = &ctx->mixer_res;

	DRM_DEBUG_KMS("win: %d\n", win);

	mutex_lock(&ctx->mixer_mutex);
	if (!ctx->powered) {
		mutex_unlock(&ctx->mixer_mutex);
		ctx->planes[win].resume = false;
		return;
	}
	mutex_unlock(&ctx->mixer_mutex);

	if(ctx->planes[win].dma_reg) {
		VIOC_RDMA_SetImageDisable(ctx->planes[win].dma_reg);
		if(ctx->planes[win].sc_reg) {
			VIOC_PlugInOutCheck plugin_state;
			VIOC_CONFIG_Device_PlugState(win, &plugin_state);
			if(plugin_state.enable || plugin_state.connect_statue == VIOC_PATH_CONNECTED) {
				VIOC_CONFIG_PlugOut(win);
				VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_SCALER, win, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(mixer_res->vioc_reg, VIOC_CONFIG_SCALER, win, VIOC_CONFIG_CLEAR);
			}
		}
		ctx->planes[win].enabled = false;
	}
}

static void mixer_wait_for_vblank(struct tcc_drm_crtc *crtc)
{
	struct mixer_context *ctx = crtc->ctx;
	int err;

	mutex_lock(&ctx->mixer_mutex);
	if (!ctx->powered) {
		mutex_unlock(&ctx->mixer_mutex);
		return;
	}
	mutex_unlock(&ctx->mixer_mutex);

	err = drm_vblank_get(ctx->drm_dev, ctx->pipe);
	if (err < 0) {
		DRM_DEBUG_KMS("Failed to acquire vblank counter\n");
		return;
	}

	atomic_set(&ctx->wait_vsync_event, 1);

	/*
	 * wait for MIXER to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (!wait_event_timeout(ctx->wait_vsync_queue,
				!atomic_read(&ctx->wait_vsync_event),
				HZ/20))
		DRM_DEBUG_KMS("vblank wait timed out.\n");

	drm_vblank_put(ctx->drm_dev, ctx->pipe);
}

static void mixer_window_suspend(struct mixer_context *ctx)
{
	struct tcc_drm_plane *plane;
	int i;

	for (i = 0; i < MIXER_WIN_NR; i++) {
		plane = &ctx->planes[i];
		plane->resume = plane->enabled;
		mixer_win_disable(ctx->crtc, i);
	}
	mixer_wait_for_vblank(ctx->crtc);
}

static void mixer_window_resume(struct mixer_context *ctx)
{
	int i;
	struct tcc_drm_plane *plane;

	DRM_DEBUG_KMS(" \r\n");

	for (i = 0; i < MIXER_WIN_NR; i++) {
		plane = &ctx->planes[i];
		plane->enabled = plane->resume;
		plane->resume = false;
		if (plane->enabled)
			mixer_win_commit(ctx->crtc, i);
	}
}

static void mixer_poweron(struct mixer_context *ctx)
{
	struct mixer_resources *mixer_res = &ctx->mixer_res;
	volatile unsigned long bits, reg_val;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	mutex_lock(&ctx->mixer_mutex);
	if (ctx->powered) {
		mutex_unlock(&ctx->mixer_mutex);
		return;
	}
	mutex_unlock(&ctx->mixer_mutex);

	ctx->powered = true;

	pm_runtime_get_sync(ctx->dev);

	ret = clk_prepare_enable(mixer_res->clk);
	if (ret < 0) {
		DRM_ERROR("Failed to prepare_enable the bus clk [%d]\n", ret);
		return;
	}

	bits = ioread32((void*)(mixer_res->ddc_reg + 0x00));
	reg_val = (bits & ~1);
	iowrite32(reg_val, (void*)(mixer_res->ddc_reg + 0x00));

	mixer_path_reset(ctx);

	/* if vblank was enabled status, enable it again. */
	if (test_and_clear_bit(0, &ctx->irq_flags)) {
		ret = mixer_enable_vblank(ctx->crtc);
		if (ret) {
			DRM_ERROR("Failed to re-enable vblank [%d]\n", ret);
			return;
		}
	}

	mixer_window_resume(ctx);
}

static void mixer_poweroff(struct mixer_context *ctx)
{
	struct mixer_resources *mixer_res = &ctx->mixer_res;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	mutex_lock(&ctx->mixer_mutex);
	if (!ctx->powered) {
		mutex_unlock(&ctx->mixer_mutex);
		return;
	}
	mutex_unlock(&ctx->mixer_mutex);

	mixer_window_suspend(ctx);

	mutex_lock(&ctx->mixer_mutex);
	ctx->powered = false;
	mutex_unlock(&ctx->mixer_mutex);

	clk_disable_unprepare(mixer_res->clk);

	pm_runtime_put_sync(ctx->dev);
}

static void mixer_dpms(struct tcc_drm_crtc *crtc, int mode)
{
	switch (mode) {
	case DRM_MODE_DPMS_ON:
		mixer_poweron(crtc->ctx);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		mixer_poweroff(crtc->ctx);
		break;
	default:
		DRM_DEBUG_KMS("unknown dpms mode: %d\n", mode);
		break;
	}
}

/* Only valid for Mixer version 16.0.33.0 */
int mixer_check_mode(struct drm_display_mode *mode)
{
	u32 w, h;

	w = mode->hdisplay;
	h = mode->vdisplay;

	DRM_DEBUG_KMS("xres=%d, yres=%d, refresh=%d, intl=%d\n",
				  mode->hdisplay, mode->vdisplay, mode->vrefresh,
				  (mode->flags & DRM_MODE_FLAG_INTERLACE) ? 1 : 0);

	// Limit 1080p
	if ((w >= 464 && w <= 720 && h >= 261 && h <= 576) ||
		(w >= 1024 && w <= 1280 && h >= 576 && h <= 720) ||
		(w >= 1664 && w <= 1920 && h >= 936 && h <= 1080))
		return 0;

	return -EINVAL;
}

static const struct tcc_drm_crtc_ops mixer_crtc_ops = {
	.dpms			= mixer_dpms,
	.commit			= mixer_commit,
	.disable		= mixer_disable,
	.prepare		= mixer_prepare,
	.enable_vblank		= mixer_enable_vblank,
	.disable_vblank		= mixer_disable_vblank,
	.wait_for_vblank	= mixer_wait_for_vblank,
	.win_commit		= mixer_win_commit,
	.win_disable		= mixer_win_disable,
};

static int mixer_bind(struct device *dev, struct device *manager, void *data)
{
	struct mixer_context *ctx = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct tcc_drm_plane *tcc_plane;
	enum drm_plane_type type;
	unsigned int zpos;
	int ret;

	ret = mixer_initialize(ctx, drm_dev);
	if (ret)
		return ret;

	for (zpos = 0; zpos < MIXER_WIN_NR; zpos++) {
		type = (zpos == MIXER_DEFAULT_WIN) ? DRM_PLANE_TYPE_PRIMARY :
			DRM_PLANE_TYPE_OVERLAY;
		ret = tcc_plane_init(drm_dev, &ctx->planes[zpos],
							 1 << ctx->pipe, type, zpos);
		if (ret)
			return ret;
	}

	tcc_plane = &ctx->planes[MIXER_DEFAULT_WIN];
	ctx->crtc = tcc_drm_crtc_create(drm_dev, &tcc_plane->base,
									ctx->pipe, TCC_DISPLAY_TYPE_HDMI,
									&mixer_crtc_ops, ctx);
	if (IS_ERR(ctx->crtc)) {
		mixer_ctx_remove(ctx);
		ret = PTR_ERR(ctx->crtc);
		goto free_ctx;
	}

	return 0;

free_ctx:
	devm_kfree(dev, ctx);
	return ret;
}

static void mixer_unbind(struct device *dev, struct device *master, void *data)
{
	struct mixer_context *ctx = dev_get_drvdata(dev);

	mixer_ctx_remove(ctx);
}

static const struct component_ops mixer_component_ops = {
	.bind	= mixer_bind,
	.unbind	= mixer_unbind,
};

static int mixer_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mixer_context *ctx;
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		DRM_ERROR("failed to alloc mixer context.\n");
		return -ENOMEM;
	}

	mutex_init(&ctx->mixer_mutex);

	ctx->pdev = pdev;
	ctx->dev = dev;
	init_waitqueue_head(&ctx->wait_vsync_queue);
	atomic_set(&ctx->wait_vsync_event, 0);

	platform_set_drvdata(pdev, ctx);

	ret = tcc_drm_component_add(&pdev->dev, TCC_DEVICE_TYPE_CRTC,
								TCC_DISPLAY_TYPE_HDMI);
	if (ret)
		return ret;


	ret = component_add(&pdev->dev, &mixer_component_ops);
	if (ret) {
		tcc_drm_component_del(&pdev->dev, TCC_DEVICE_TYPE_CRTC);
		return ret;
	}

	pm_runtime_enable(dev);

	dev_info(dev, "tcc mixer registered successfully.\n");

	return ret;
}

static int mixer_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	component_del(&pdev->dev, &mixer_component_ops);
	tcc_drm_component_del(&pdev->dev, TCC_DEVICE_TYPE_CRTC);

	return 0;
}

/**
 * @short of_device_id structure
 */
static const struct of_device_id tcc_mixer[] = {
	{ .compatible = "telechips,tcc-mixer" },
};
MODULE_DEVICE_TABLE(of, tcc_mixer);


struct platform_driver mixer_driver = {
	.driver = {
		.name = "tcc-mixer",
		.owner = THIS_MODULE,
		.of_match_table = tcc_mixer,

	},
	.probe = mixer_probe,
	.remove = mixer_remove,
};
