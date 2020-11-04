// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __DPTX_DRM_H__
#define __DPTX_DRM_H__

struct dptx_detailed_timing_t
{
 unsigned char interlaced;
 unsigned char pixel_repetition_input;
 unsigned int  h_active;
 unsigned int  h_blanking;
 unsigned int  h_sync_offset;
 unsigned int  h_sync_pulse_width;
 unsigned int  h_sync_polarity;
 unsigned int  v_active;
 unsigned int  v_blanking;
 unsigned int  v_sync_offset;
 unsigned int  v_sync_pulse_width;
 unsigned int  v_sync_polarity;
 unsigned int  pixel_clock;
};

struct dptx_drm_helper_funcs {
    int (*get_hpd_state)(int dp_id, unsigned char *hpd_state);
    int (*get_edid)(int dp_id, unsigned char *edid, int buf_length);
    int (*set_video)(
        int dp_id, struct dptx_detailed_timing_t *dptx_detailed_timing);
    int (*set_enable_video)(int dp_id, unsigned char enable);
	int (*set_enable_audio)(int dp_id, unsigned char enable);
};

struct tcc_drm_dp_callback_funcs;

int tcc_dp_register_drm(
    struct drm_encoder *encoder, struct tcc_drm_dp_callback_funcs *callbacks );
#endif /* __DPTX_DRM_H__  */

