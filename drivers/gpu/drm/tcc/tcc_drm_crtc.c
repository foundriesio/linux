/* tcc_drm_crtc.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_edid.h>
#include <drm/tcc_drm.h>

#include <linux/clk.h>
#include <video/display_timing.h>
#include <video/videomode.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>

#include <tcc_drm_crtc.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_plane.h>
#define LOG_TAG "DRMCRTC"

enum tcc_drm_crtc_flip_status {
	TCC_DRM_CRTC_FLIP_STATUS_NONE = 0,
	TCC_DRM_CRTC_FLIP_STATUS_PENDING,
	TCC_DRM_CRTC_FLIP_STATUS_DONE,
};

#define tcc_drm_crtc_dump_event(dev, in_event) \
do { 									\
	struct drm_pending_vblank_event *vblank_event = (in_event); 	\
	if(vblank_event != NULL) {					\
		switch(vblank_event->event.base.type) {			\
			case DRM_EVENT_VBLANK:				\
				dev_info(dev, "[INFO][%s] %s line(%d) DRM_EVENT_VBLANK\r\n", \
					 			LOG_TAG, __func__, __LINE__); \
				break;					\
			case DRM_EVENT_FLIP_COMPLETE:			\
				dev_info(dev, "[INFO][%s] %s line(%d) DRM_EVENT_FLIP_COMPLETE\r\n", \
						LOG_TAG, __func__, __LINE__); \
				break;					\
		}							\
	}								\
} while(0)								\

static void tcc_drm_crtc_atomic_enable(struct drm_crtc *crtc,
					  struct drm_crtc_state *old_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->enable)
		tcc_crtc->ops->enable(tcc_crtc);

	drm_crtc_vblank_on(crtc);

	if (crtc->state->event) {
		unsigned long flags;

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		tcc_crtc->flip_event = crtc->state->event;
		crtc->state->event = NULL;
		//tcc_drm_crtc_dump_event(tcc_crtc->flip_event);

		atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_DONE);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void tcc_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					   struct drm_crtc_state *old_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);


	if (tcc_crtc->ops->disable)
		tcc_crtc->ops->disable(tcc_crtc);

	drm_crtc_vblank_off(crtc);

	if (crtc->state->event) {
		unsigned long flags;
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		crtc->state->event = NULL;
	}
}

static int tcc_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_crtc_state *state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (!state->enable)
		return 0;

	if (tcc_crtc->ops->atomic_check)
		return tcc_crtc->ops->atomic_check(tcc_crtc, state);

	return 0;
}

static void tcc_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->atomic_begin)
		tcc_crtc->ops->atomic_begin(tcc_crtc);
}

static void tcc_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if(!old_crtc_state->active) {
		//printk(KERN_WARNING "[WARN][DRMCRTC] %s old crtc status is not active\r\n", __func__);
		return;
	}

	if (tcc_crtc->ops->atomic_flush)
		tcc_crtc->ops->atomic_flush(tcc_crtc);
}

static enum drm_mode_status tcc_crtc_mode_valid(struct drm_crtc *crtc,
	const struct drm_display_mode *mode)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->mode_valid)
		return tcc_crtc->ops->mode_valid(tcc_crtc, mode);

	return MODE_OK;
}

static void tcc_crtc_set_nofb(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->mode_set_nofb)
		return tcc_crtc->ops->mode_set_nofb(tcc_crtc);
}

static const struct drm_crtc_helper_funcs tcc_crtc_helper_funcs = {
	.mode_valid	= tcc_crtc_mode_valid,
	.mode_set_nofb = tcc_crtc_set_nofb,
	.atomic_check	= tcc_crtc_atomic_check,
	.atomic_begin	= tcc_crtc_atomic_begin,
	.atomic_flush	= tcc_crtc_atomic_flush,
	.atomic_enable	= tcc_drm_crtc_atomic_enable,
	.atomic_disable	= tcc_drm_crtc_atomic_disable,
};

static void tcc_drm_crtc_flip_complete(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
    	unsigned long flags;

    	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_NONE);
	tcc_crtc->flip_async = false;

	if (tcc_crtc->flip_event) {
		drm_crtc_send_vblank_event(crtc, tcc_crtc->flip_event);
		tcc_crtc->flip_event = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void tcc_crtc_handle_event(struct tcc_drm_crtc *tcc_crtc)
{
	unsigned long flags;
	struct drm_crtc *crtc = &tcc_crtc->base;
	struct drm_crtc_state *new_crtc_state = crtc->state;
	struct drm_pending_vblank_event *event = new_crtc_state->event;

	if (!new_crtc_state->active) {
		//dev_debug("[DEBUG][%s] %s line(%d) new crtc state is not active\r\n",
		//	LOG_TAG, __func__, __LINE__);
		return;
	}

	if (event == NULL) {
		pr_info("[INFO][%s] %s line(%d) event is NULL\r\n",
					LOG_TAG, __func__, __LINE__);
		return;
	}

	tcc_crtc->flip_async = !!(new_crtc_state->pageflip_flags
					  & DRM_MODE_PAGE_FLIP_ASYNC);

	if (tcc_crtc->flip_async)
        	WARN_ON(drm_crtc_vblank_get(crtc) != 0);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	tcc_crtc->flip_event = crtc->state->event;
	crtc->state->event = NULL;
	atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_DONE);
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	if (tcc_crtc->flip_async)
		tcc_drm_crtc_flip_complete(crtc);
}

static void tcc_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(tcc_crtc);
}

static int tcc_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->enable_vblank)
		return tcc_crtc->ops->enable_vblank(tcc_crtc);

	return 0;
}

static void tcc_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->disable_vblank)
		tcc_crtc->ops->disable_vblank(tcc_crtc);
}

static const struct drm_crtc_funcs tcc_crtc_funcs = {
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.destroy	= tcc_drm_crtc_destroy,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = tcc_drm_crtc_enable_vblank,
	.disable_vblank = tcc_drm_crtc_disable_vblank,
};

static  u32 tcc_drm_crtc_calc_vactive(struct drm_display_mode *mode)
{
	u32 vactive;

	if(mode == NULL)
		goto err_null_pointer;

	vactive = mode->vdisplay;
	#if defined(CONFIG_DRM_TCC_SUPPORT_3D)
	if(mode->flags & DRM_MODE_FLAG_3D_FRAME_PACKING) {
		u32 vblank = mode->vtotal - mode->vdisplay;
		if(mode->flags & DRM_MODE_FLAG_INTERLACE) {
			vactive = (vactive << 2) + (3*vblank+2);
		} else {
			vactive = (vactive << 1) + vblank;
		}
	} else
	#endif
	if(mode->flags & DRM_MODE_FLAG_INTERLACE) {
		vactive <<= 1;
	}
	return vactive;

err_null_pointer:
	return 0;
}

static int tcc_drm_crtc_check_pixelclock_match(unsigned long res, unsigned long data)
{
	unsigned long bp;
	int match = 0;

	bp = DIV_ROUND_UP(data, 10);
	if(res > (data - bp) && res < (data + bp)) {
		match = 1;
	}
	return match;
}

static void tcc_drm_crtc_force_disable(struct drm_crtc *crtc,
				     struct tcc_hw_device *hw_data)
{
	const struct drm_encoder_helper_funcs *funcs;
        struct drm_encoder *encoder;
	int i;

        drm_for_each_encoder(encoder, crtc->dev)
                if (encoder->crtc == crtc)
                        goto go_find_crtc;

	return;

go_find_crtc:
	dev_info(crtc->dev->dev,
		"[INFO][%s] %s line(%d) Turn off display device on this CRTC\r\n",
		LOG_TAG, __func__, __LINE__);

	/* Disable panels if exist */
	funcs = encoder->helper_private;
	if(funcs->disable != NULL)
		funcs->disable(encoder);

	/* Turn off display device */
	if(VIOC_DISP_Get_TurnOnOff(hw_data->display_device.virt_addr))
		VIOC_DISP_TurnOff(hw_data->display_device.virt_addr);

	/* Disable all planes on this crtc */
	for(i = 0;i < RDMA_MAX_NUM; i++) {
		if(hw_data->rdma[i].virt_addr != NULL)
			VIOC_RDMA_SetImageDisableNW(hw_data->rdma[i].virt_addr);
	}
	//PCK.
}

static int tcc_drm_crtc_check_display_timing(struct drm_crtc *crtc,
				     struct drm_display_mode *mode,
				     struct tcc_hw_device *hw_data)
{
	struct DisplayBlock_Info ddinfo;
	int need_reset = 0;
	u32 vactive;

	VIOC_DISP_GetDisplayBlock_Info(hw_data->display_device.virt_addr, &ddinfo);
	/* Check turn on status of display device */
	if(!ddinfo.enable) {
		dev_info(crtc->dev->dev,
			"[INFO][%s] %s line(%d) display device is disabled\r\n",
			LOG_TAG, __func__, __LINE__);
		need_reset = 1;
		goto out_state_on;
	}

	/* Check width and height */
	vactive = tcc_drm_crtc_calc_vactive(mode);
	if(ddinfo.width != mode->hdisplay || ddinfo.height != vactive) {
		dev_info(crtc->dev->dev,
			"[INFO][%s] %s line(%d) display size is not match %dx%d : %dx%d\r\n",
			LOG_TAG, __func__, __LINE__,
			ddinfo.width, ddinfo.height, mode->hdisplay, vactive);
		need_reset = 1;
		goto out_turnoff;
	}
	/* Check pixel clock */
	if(!tcc_drm_crtc_check_pixelclock_match(clk_get_rate(hw_data->ddc_clock), mode->clock * 1000)) {
		dev_info(crtc->dev->dev,
			"[INFO][%s] %s line(%d) clock is not match %ldHz : %dHz\r\n",
			LOG_TAG, __func__, __LINE__, clk_get_rate(hw_data->ddc_clock), mode->clock * 1000);
		need_reset = 1;
		goto out_turnoff;
	}
	return 0;

out_turnoff:
	tcc_drm_crtc_force_disable(crtc, hw_data);

out_state_on:
	return need_reset;
}

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *primary,
					struct drm_plane *cursor,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *ctx)
{
	struct tcc_drm_crtc *tcc_crtc;
	struct drm_crtc *crtc;
	int ret;
	tcc_crtc = kzalloc(sizeof(*tcc_crtc), GFP_KERNEL);
	if (!tcc_crtc)
		return ERR_PTR(-ENOMEM);

	tcc_crtc->type = type;
	tcc_crtc->ops = ops;
	tcc_crtc->ctx = ctx;

	crtc = &tcc_crtc->base;

	atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_NONE);

	ret = drm_crtc_init_with_planes(drm_dev, crtc, primary, cursor,
					&tcc_crtc_funcs, NULL);
	if (ret < 0)
		goto err_crtc;

	drm_crtc_helper_add(crtc, &tcc_crtc_helper_funcs);

	return tcc_crtc;

err_crtc:
	if(primary != NULL && primary->funcs->destroy != NULL)
		primary->funcs->destroy(primary);
	if(cursor != NULL && cursor->funcs->destroy != NULL)
		cursor->funcs->destroy(cursor);
	kfree(tcc_crtc);
	return ERR_PTR(ret);
}


struct tcc_drm_crtc *tcc_drm_crtc_get_by_type(struct drm_device *drm_dev,
				       enum tcc_drm_output_type out_type)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev)
		if (to_tcc_crtc(crtc)->type == out_type)
			return to_tcc_crtc(crtc);

	return ERR_PTR(-EPERM);
}

int tcc_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum tcc_drm_output_type out_type)
{
	struct tcc_drm_crtc *crtc = tcc_drm_crtc_get_by_type(encoder->dev,
						out_type);

	if (IS_ERR(crtc))
		return PTR_ERR(crtc);

	encoder->possible_crtcs = drm_crtc_mask(&crtc->base);

	return 0;
}

void tcc_drm_crtc_vblank_handler(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	enum tcc_drm_crtc_flip_status status;

	drm_handle_vblank(crtc->dev, drm_crtc_index(crtc));

	status = atomic_read(&tcc_crtc->flip_status);
	if (status == TCC_DRM_CRTC_FLIP_STATUS_DONE) {
		if (!tcc_crtc->flip_async) {
			tcc_drm_crtc_flip_complete(crtc);
		}
	}
}
void tcc_crtc_fill_base_edid(struct edid *base_edid)
{
	/*
	LVDS Sample EDID (Ver 1.3)
	Display Name : BOE WLCD 12.3
	Vendor ID : TCC
	Mode : 1920 x 720 60Hz
	*/

	{// edid header
		base_edid->header[0] = 0x00;
		base_edid->header[1] = 0xff;
		base_edid->header[2] = 0xff;
		base_edid->header[3] = 0xff;
		base_edid->header[4] = 0xff;
		base_edid->header[5] = 0xff;
		base_edid->header[6] = 0xff;
		base_edid->header[7] = 0x00;

		// edid vendor/product
		base_edid->mfg_id[0] = 0x50;
		base_edid->mfg_id[1] = 0x63;// TCC
		// edid product code
		base_edid->prod_code[0] = 0x0;
		base_edid->prod_code[1] = 0x0;
		// edid Serial Number
		base_edid->serial = 0x0;
		// edid mfg_week
		base_edid->mfg_week = 0x1;
		// edid mfg_year
		base_edid->mfg_year = 0x1E;// 2020
		// edid version / revision
		base_edid->version = 0x01;
		base_edid->revision = 0x03;
		base_edid->input = 0x80;
		base_edid->width_cm = 0x50;
		base_edid->height_cm = 0x2D;
		base_edid->gamma = 0x78;
		base_edid->features = 0x1A;
		base_edid->red_green_lo = 0x0D;
		base_edid->black_white_lo = 0xC9;
		base_edid->red_x = 0xA0;
		base_edid->red_y = 0x57;
		base_edid->green_x = 0x47;
		base_edid->green_y = 0x98;
		base_edid->blue_x = 0x27;
		base_edid->blue_y = 0x12;
		base_edid->white_x = 0x48;
		base_edid->white_y = 0x4C;

	/* established timing */
		base_edid->established_timings.t1 = 0x20;
		base_edid->established_timings.t2 = 0x00;
		base_edid->established_timings.mfg_rsvd = 0x00;

	/* standard timing */
		base_edid->standard_timings[0].hsize = 0x01;
		base_edid->standard_timings[0].vfreq_aspect = 0x01;
		base_edid->standard_timings[1].hsize = 0x01;
		base_edid->standard_timings[1].vfreq_aspect = 0x01;
		base_edid->standard_timings[2].hsize = 0x01;
		base_edid->standard_timings[2].vfreq_aspect = 0x01;
		base_edid->standard_timings[3].hsize = 0x01;
		base_edid->standard_timings[3].vfreq_aspect = 0x01;
		base_edid->standard_timings[4].hsize = 0x01;
		base_edid->standard_timings[4].vfreq_aspect = 0x01;
		base_edid->standard_timings[5].hsize = 0x01;
		base_edid->standard_timings[5].vfreq_aspect = 0x01;
		base_edid->standard_timings[6].hsize = 0x01;
		base_edid->standard_timings[6].vfreq_aspect = 0x01;
		base_edid->standard_timings[7].hsize = 0x01;
		base_edid->standard_timings[7].vfreq_aspect = 0x01;
	}

	/* detailed timing */
	base_edid->detailed_timings[0].pixel_clock = 0x1770;
	base_edid->detailed_timings[0].data.pixel_data.hactive_lo = 0x80;
	base_edid->detailed_timings[0].data.pixel_data.hblank_lo = 0x0;
	base_edid->detailed_timings[0].data.pixel_data.hactive_hblank_hi = 0x70;
	base_edid->detailed_timings[0].data.pixel_data.vactive_lo = 0xD0;
	base_edid->detailed_timings[0].data.pixel_data.vblank_lo = 0x0;
	base_edid->detailed_timings[0].data.pixel_data.vactive_vblank_hi = 0x20;
	base_edid->detailed_timings[0].data.pixel_data.hsync_offset_lo = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.hsync_pulse_width_lo = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.vsync_offset_pulse_width_lo = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.hsync_vsync_offset_pulse_width_hi = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.width_mm_lo = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.height_mm_lo = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.width_height_mm_hi = 0x00;
	base_edid->detailed_timings[0].data.pixel_data.hborder = 0x0;
	base_edid->detailed_timings[0].data.pixel_data.vborder = 0x0;
	base_edid->detailed_timings[0].data.pixel_data.misc = 0x18;
	/* Descriptor 2*/
	base_edid->detailed_timings[1].pixel_clock = 0x0;
	base_edid->detailed_timings[1].data.pixel_data.hactive_lo = 0x00;
	base_edid->detailed_timings[1].data.pixel_data.hblank_lo = 0xFC;
	base_edid->detailed_timings[1].data.pixel_data.hactive_hblank_hi = 0x00;
	base_edid->detailed_timings[1].data.pixel_data.vactive_lo = 0x42;
	base_edid->detailed_timings[1].data.pixel_data.vblank_lo = 0x4F;
	base_edid->detailed_timings[1].data.pixel_data.vactive_vblank_hi = 0x45;
	base_edid->detailed_timings[1].data.pixel_data.hsync_offset_lo = 0x20;
	base_edid->detailed_timings[1].data.pixel_data.hsync_pulse_width_lo = 0x57;
	base_edid->detailed_timings[1].data.pixel_data.vsync_offset_pulse_width_lo = 0x4C;
	base_edid->detailed_timings[1].data.pixel_data.hsync_vsync_offset_pulse_width_hi = 0x43;
	base_edid->detailed_timings[1].data.pixel_data.width_mm_lo = 0x44;
	base_edid->detailed_timings[1].data.pixel_data.height_mm_lo = 0x20;
	base_edid->detailed_timings[1].data.pixel_data.width_height_mm_hi = 0x31;
	base_edid->detailed_timings[1].data.pixel_data.hborder = 0x32;
	base_edid->detailed_timings[1].data.pixel_data.vborder = 0x2E;
	base_edid->detailed_timings[1].data.pixel_data.misc = 0x33;
	/* Descriptor 3*/
	base_edid->detailed_timings[2].pixel_clock = 0x0;
	base_edid->detailed_timings[2].data.pixel_data.hactive_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.hblank_lo = 0x10;
	base_edid->detailed_timings[2].data.pixel_data.hactive_hblank_hi = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.vactive_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.vblank_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.vactive_vblank_hi = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.hsync_offset_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.hsync_pulse_width_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.vsync_offset_pulse_width_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.hsync_vsync_offset_pulse_width_hi = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.width_mm_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.height_mm_lo = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.width_height_mm_hi = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.hborder = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.vborder = 0x00;
	base_edid->detailed_timings[2].data.pixel_data.misc = 0x00;
	/* Descriptor 4*/
	base_edid->detailed_timings[3].pixel_clock = 0x0;
	base_edid->detailed_timings[3].data.pixel_data.hactive_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.hblank_lo = 0x10;
	base_edid->detailed_timings[3].data.pixel_data.hactive_hblank_hi = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.vactive_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.vblank_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.vactive_vblank_hi = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.hsync_offset_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.hsync_pulse_width_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.vsync_offset_pulse_width_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.hsync_vsync_offset_pulse_width_hi = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.width_mm_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.height_mm_lo = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.width_height_mm_hi = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.hborder = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.vborder = 0x00;
	base_edid->detailed_timings[3].data.pixel_data.misc = 0x00;
	base_edid->extensions = 0x00;

	base_edid->checksum = 0x59;
}

void tcc_crtc_fill_detailed_edid(struct edid * base_edid, struct drm_display_mode * mode)
{
	base_edid->detailed_timings[0].pixel_clock = (mode -> clock) /10;
	base_edid->detailed_timings[0].data.pixel_data.hactive_lo = mode->hdisplay & 0xff;
	base_edid->detailed_timings[0].data.pixel_data.hblank_lo = (mode->htotal - mode->hdisplay) & 0xff;
	base_edid->detailed_timings[0].data.pixel_data.hactive_hblank_hi = (mode->hdisplay >> 4) & 0xf0;
	base_edid->detailed_timings[0].data.pixel_data.vactive_lo = mode->vdisplay &0xff;
	base_edid->detailed_timings[0].data.pixel_data.vblank_lo = (mode->vtotal - mode->vdisplay) & 0xff;
	base_edid->detailed_timings[0].data.pixel_data.vactive_vblank_hi = (mode->vdisplay >> 4) & 0xf0;

	//horizontal front porch      - lower 8bits
	base_edid->detailed_timings[0].data.pixel_data.hsync_offset_lo = (mode->hsync_start - mode->hdisplay) & 0xff;
	//horizontal sync pulse width hsyncwidth - lower 8bits
	base_edid->detailed_timings[0].data.pixel_data.hsync_pulse_width_lo = (mode->hsync_end - mode->hsync_start) & 0xff;

	//Vertical Front Porch  -  --- stored in Upper Nibble : contains lower 4 bits
	//Vertical Sync Pulse Width in Lines  vsyncwidth  --- stored in Lower Nibble : contains lower 4 bits
	base_edid->detailed_timings[0].data.pixel_data.vsync_offset_pulse_width_lo = (((mode->vsync_start - mode->vdisplay) & 0xf) << 4) | ((mode->vsync_end - mode->vsync_start) & 0xf);
	base_edid->detailed_timings[0].data.pixel_data.hsync_vsync_offset_pulse_width_hi = (((mode->hsync_start - mode->hdisplay) >> 8) << 6) || (((mode->hsync_end - mode->hsync_start) >> 8) << 4) ||
																					(((mode->vsync_start - mode->vdisplay) >> 4 ) << 2) || ((mode->vsync_end - mode->vsync_start) >> 4);

	base_edid->detailed_timings[0].data.pixel_data.width_mm_lo = mode->width_mm & 0xff;
	base_edid->detailed_timings[0].data.pixel_data.height_mm_lo = mode->height_mm & 0xff;
	base_edid->detailed_timings[0].data.pixel_data.width_height_mm_hi = (mode->width_mm >> 8) << 4 || mode->height_mm >> 8;
	base_edid->detailed_timings[0].data.pixel_data.hborder = 0x0;
	base_edid->detailed_timings[0].data.pixel_data.vborder = 0x0;
	base_edid->detailed_timings[0].data.pixel_data.misc = 0x18;
}

int tcc_crtc_edid_checksum(struct edid *base_edid)
{
	u8 data[128];
	u8 csum = 0;
	int i;

	memcpy(data, base_edid, sizeof(data));

	for(i=0;i<EDID_LENGTH-1;i++){
		csum += data[i];
	}
	printk(KERN_INFO "[DEBUG][%d][%s] Sum of EDID is %2x, Checksum is %2x\n", __LINE__,  __FUNCTION__, csum, 0xFF-csum+1);

	base_edid->checksum = 0xFF-csum+1;
	return 0;
}

int tcc_crtc_parse_edid_ioctl(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct edid base_edid[4];
	//u8 test[512];
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_tcc_edid * args = (void __user *)data;

	int i =0;

	pr_info("[DEBUG][%d][%s] Ioctl called \n", __LINE__, __FUNCTION__);

	memset(base_edid, 0, sizeof(base_edid));
	/* get crtc */
	crtc = drm_crtc_find(dev, args->crtc_id);
	if(!crtc){
		pr_err("[ERR][DRMCRTC] %s line(%d) Invalid crtc ID \r\n",  __func__, __LINE__);
		return -ENOENT;
	}
	crtc_state = crtc->state;
	pr_info("[DEBUG][%d][%s] crtc_id : [%d]\n", __LINE__, __FUNCTION__, args->crtc_id);

	// fill edid with base info
	tcc_crtc_fill_base_edid(base_edid);

	/* fill edid with crtc state timing */
	tcc_crtc_fill_detailed_edid(base_edid, &crtc_state->mode);
	tcc_crtc_edid_checksum(base_edid);

	memcpy(args->data, base_edid, sizeof(base_edid));

	return 0;
}

int tcc_drm_crtc_set_display_timing(struct drm_crtc *crtc,
				     struct drm_display_mode *mode,
				     struct tcc_hw_device *hw_data)
{
	#if defined(CONFIG_ARCH_TCC805X)
	stLTIMING stTimingParam;
        stLCDCTR stCtrlParam;
	bool interlace;
	u32 tmp_sync;
	u32 vactive;
	#endif
	struct videomode vm;
	if(crtc == NULL)
		goto err_null_pointer;
	if(crtc->dev == NULL)
		goto err_null_pointer;
	if(mode == NULL)
		goto err_null_pointer;
	if(hw_data == NULL)
		goto err_null_pointer;
	if(crtc->dev->dev == NULL)
		goto err_null_pointer;

	drm_display_mode_to_videomode(mode, &vm);

	#if defined(CONFIG_ARCH_TCC805X)
	if(!tcc_drm_crtc_check_display_timing(crtc, mode, hw_data))
		goto finish;

	/* Display MUX */
	VIOC_CONFIG_LCDPath_Select(get_vioc_index(hw_data->display_device.blk_num), hw_data->lcdc_mux);
	dev_info(crtc->dev->dev,
			"[INFO][%s] %s display device(%d) to connect mux(%d)\r\n",
					LOG_TAG, __func__, get_vioc_index(hw_data->display_device.blk_num), hw_data->lcdc_mux);

	memset(&stCtrlParam, 0, sizeof(stLCDCTR));
	memset(&stTimingParam, 0, sizeof(stLTIMING));
	interlace = vm.flags & DISPLAY_FLAGS_INTERLACED;
	stCtrlParam.iv = (vm.flags & DISPLAY_FLAGS_VSYNC_LOW)?1:0;
	stCtrlParam.ih = (vm.flags & DISPLAY_FLAGS_HSYNC_LOW)?1:0;
	stCtrlParam.dp = (vm.flags & DISPLAY_FLAGS_DOUBLECLK)?1:0;

	if(interlace) {
		stCtrlParam.tv = 1;
		stCtrlParam.advi = 1;
	}
	else {
		stCtrlParam.ni = 1;
	}

	/* Calc vactive */
	vactive = tcc_drm_crtc_calc_vactive(mode);

	stTimingParam.lpc = vm.hactive;
	stTimingParam.lewc = vm.hfront_porch;
	stTimingParam.lpw = (vm.hsync_len>0)?(vm.hsync_len-1):0;
	stTimingParam.lswc = vm.hback_porch;

	if(interlace){
		tmp_sync = vm.vsync_len << 1;
		stTimingParam.fpw = (tmp_sync>0)?(tmp_sync-1):0;
		tmp_sync = vm.vback_porch << 1;
		stTimingParam.fswc = (tmp_sync>0)?(tmp_sync-1):0;
		stTimingParam.fewc = vm.vfront_porch << 1;
		stTimingParam.fswc2 = stTimingParam.fswc+1;
		stTimingParam.fewc2 = (stTimingParam.fewc>0)?(stTimingParam.fewc-1):0;
		if(mode->vtotal == 1250 && vm.hactive == 1920 && vm.vactive == 540) {
			/* VIC 1920x1080@50i 1250 vtotal */
			stTimingParam.fewc -= 2;
		}
	}
	else {
		stTimingParam.fpw = (vm.vsync_len>0)?(vm.vsync_len-1):0;
		stTimingParam.fswc = (vm.vback_porch>0)?(vm.vback_porch-1):0;
		stTimingParam.fewc = (vm.vfront_porch>0)?(vm.vfront_porch-1):0;
		stTimingParam.fswc2 = stTimingParam.fswc;
		stTimingParam.fewc2 = stTimingParam.fewc;
	}

	// Check 3D Frame Packing
	#if defined(CONFIG_DRM_TCC_SUPPORT_3D)
	if(mode->flags & DRM_MODE_FLAG_3D_FRAME_PACKING)
		stTimingParam.framepacking = 1;
	else if (mode->flags & DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF)
		stTimingParam.framepacking = 2;
	else if (mode->flags & DRM_MODE_FLAG_3D_TOP_AND_BOTTOM)
		stTimingParam.framepacking = 3;
	#endif

	/* Common Timing Parameters */
	stTimingParam.flc = (vactive>0)?(vactive-1):0;
	stTimingParam.fpw2 = stTimingParam.fpw;
	stTimingParam.flc2 = stTimingParam.flc;

	/* swreset display device */
	VIOC_CONFIG_SWReset(hw_data->display_device.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(hw_data->display_device.blk_num, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_SWReset(hw_data->wmixer.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(hw_data->wmixer.blk_num, VIOC_CONFIG_CLEAR);

	//vioc_reset_rdma_on_display_path(pDisplayInfo->DispNum);

	VIOC_WMIX_SetOverlayPriority(hw_data->wmixer.virt_addr, 24);
	VIOC_WMIX_SetSize(hw_data->wmixer.virt_addr, vm.hactive, vactive);
	VIOC_WMIX_SetUpdate (hw_data->wmixer.virt_addr);

	VIOC_DISP_SetTimingParam(hw_data->display_device.virt_addr, &stTimingParam);
	VIOC_DISP_SetControlConfigure(hw_data->display_device.virt_addr, &stCtrlParam);

	/* PXDW
	 * YCC420 with stb pxdw is 27
	 * YCC422 with stb is pxdw 21, with out stb is 8
	 * YCC444 && RGB with stb is 23, with out stb is 12
	 * TCCDRM can only support RGB as format of the display device. */
	VIOC_DISP_SetPXDW(hw_data->display_device.virt_addr, 12);

	VIOC_DISP_SetSize (hw_data->display_device.virt_addr, vm.hactive, vactive);
	VIOC_DISP_SetBGColor(hw_data->display_device.virt_addr, 0, 0, 0, 0);

finish:
	#endif

	/* Set pixel clocks */
	if(!tcc_drm_crtc_check_pixelclock_match(clk_get_rate(hw_data->ddc_clock), vm.pixelclock)) {
		dev_info(crtc->dev->dev,
			"[INFO][%s] %s line(%d) clock is not match %ldHz : %dHz\r\n",
			LOG_TAG, __func__, __LINE__, clk_get_rate(hw_data->ddc_clock), mode->clock * 1000);
		clk_set_rate(hw_data->ddc_clock, vm.pixelclock);
	}

	return 0;

err_null_pointer:
	return -1;
}
