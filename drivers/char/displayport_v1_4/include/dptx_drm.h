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
 unsigned int  pixel_clock; /* Peri에서 분주된 Real clock */
};

struct dptx_drm_callback_funcs {
    int (*get_hpd_state)(int dp_id, unsigned char *hpd_state); /* display port의 hotplug상태를 읽어온다.  hpd_state = 0( Unplugged ), 1( Plugged )  */
    int (*get_edid)(int dp_id, unsigned char *edid ); /* display port에 연결된 sink로 부터 edid를 읽어온다. dp_id는 dp topology 상의 연결 순서이다. */
    int (*set_video)( int dp_id, struct dptx_detailed_timing_t *dptx_detailed_timing );
    int (*set_output_enable)(int dp_id, unsigned char enable );
};

struct tcc_drm_dp_callbacks {
	int (*attach)(int dp_id );
	int (*detach)(int dp_id  );
	int (*register_ops)( struct dptx_drm_callback_funcs *dptx_ops);
};


#endif /* __DPTX_DRM_H__  */

