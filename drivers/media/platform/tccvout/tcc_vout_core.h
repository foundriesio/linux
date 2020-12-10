/*
 * Copyright (C) Telechips, Inc.
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
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __TCC_VOUT_CORE_H__
#define __TCC_VOUT_CORE_H__

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT  (0x8-1)
#define BIT_0      (3)
#define GET_ADDR_YUV42X_spY(Base_addr) \
	(((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) \
	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#endif

extern int vout_get_pmap(struct pmap *pmap);
extern int vout_set_vout_path(struct tcc_vout_device *vout);
extern int vout_set_m2m_path(int deintl_default, struct tcc_vout_device *vout);
extern int vout_vioc_set_default(struct tcc_vout_device *vout);
extern int vout_vioc_init(struct tcc_vout_device *vout);
extern void vout_deinit(struct tcc_vout_device *vout);
extern void vout_disp_ctrl(struct tcc_vout_vioc *vioc, int enable);
extern void vout_rdma_setup(struct tcc_vout_device *vout);
extern void vout_wmix_setup(struct tcc_vout_device *vout);
extern void vout_wmix_getsize(struct tcc_vout_device *vout,
	unsigned int *w, unsigned int *h);
extern void vout_path_reset(struct tcc_vout_vioc *vioc);
/* de-interlace */
extern void m2m_path_reset(struct tcc_vout_vioc *vioc);
extern void m2m_rdma_setup(struct vioc_rdma *rdma);
extern int vout_otf_init(struct tcc_vout_device *vout);
extern void vout_otf_deinit(struct tcc_vout_device *vout);
extern int vout_m2m_init(struct tcc_vout_device *vout);
extern void vout_m2m_ctrl(struct tcc_vout_vioc *vioc, int enable);
#if defined(CONFIG_TCC_DUAL_DISPLAY)
extern int vout_set_vout_dual_path(struct tcc_vout_device *vout);
extern int vout_m2m_dual_init(struct tcc_vout_device *vout);
extern void vout_m2m_dual_deinit(struct tcc_vout_device *vout);
extern void vout_m2m_dual_ctrl(struct tcc_vout_vioc *vioc, int enable, int m2m_dual_index);
#endif
extern void vout_m2m_deinit(struct tcc_vout_device *vout);
/* overlay  */
extern void vout_video_overlay(struct tcc_vout_device *vout);
/* sub-plane */
extern void vout_subplane_deinit(struct tcc_vout_device *vout);
extern void vout_subplane_ctrl(struct tcc_vout_device *vout, int enable);
extern void vout_subplane_m2m_init(struct tcc_vout_device *vout);
extern int vout_subplane_m2m_qbuf(struct tcc_vout_device *vout,
	struct vioc_alpha *alpha);
extern void vout_subplane_onthefly_init(struct tcc_vout_device *vout);
extern int vout_subplane_onthefly_qbuf(struct tcc_vout_device *vout);

/* buffer control */
extern void vout_pop_all_buffer(struct tcc_vout_device *vout);

/* output mode control */
extern void vout_video_set_output_mode(
	struct tcc_vout_device *vout, int mode);

/* last frame control*/
#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
extern int vout_capture_last_frame(struct tcc_vout_device *vout,
	struct v4l2_buffer *buf);
extern void vout_disp_last_frame(struct tcc_vout_device *vout);
extern void vout_video_post_process(struct tcc_vout_device *vout);
#endif

/* streaming */
extern void vout_onthefly_display_update(
	struct tcc_vout_device *vout, struct v4l2_buffer *buf);
extern void vout_m2m_display_update(struct tcc_vout_device *vout,
		struct v4l2_buffer *buf);


#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
extern unsigned int HDMI_video_width;
extern unsigned int HDMI_video_height;
extern struct tcc_dp_device *tca_get_displayType(
	TCC_OUTPUT_TYPE check_type);
extern void tca_edr_el_display_update(
	struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
extern void tca_edr_display_update(struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo);
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
#include "../../../char/hdmi_v2_0/include/hdmi_ioctls.h"
extern void hdmi_set_drm(DRM_Packet_t *drmparm);
extern void hdmi_clear_drm(void);
extern unsigned int hdmi_get_refreshrate(void);
#else
extern unsigned int HDMI_video_hz;
#endif

extern enum OUTPUT_SELECT_MODE Output_SelectMode;

extern void vout_intr_onoff(char on, struct tcc_vout_device *vout);
#ifdef CONFIG_VOUT_USE_VSYNC_INT
extern void vout_ktimer_ctrl(struct tcc_vout_device *vout, unsigned int onoff);
#endif

#endif //__TCC_VOUT_CORE_H__
