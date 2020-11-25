/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

@note Tab size is 8
****************************************************************************/
#ifndef __TCC_HDMI_API_H__
#define __TCC_HDMI_API_H__


/**
  * HPD global  APIs */
int hpd_api_get_status(void);
int hdmi_api_set_spd_infoframe(packetParam_t *packetParam);
int hdmi_api_set_vsif_infoframe(packetParam_t *packetParam);

#include <video/tcc/tccfb_ioctrl.h> /* Defines lcdc_timimg_parms_t and tcc_fb_extra_data */
int hdmi_api_dtd_fill(dtd_t * dtd, unsigned int code, unsigned int refreshRate);
unsigned int hdmi_api_dtd_get_vactive(videoParams_t *videoParam);
int hdmi_api_dtd_get_display_param(videoParams_t *videoParam,
		struct lcdc_timimg_parms_t *lcdc_timimg_parms);
int hdmi_api_dtb_get_extra_data(videoParams_t *videoParam,
		struct tcc_fb_extra_data *tcc_fb_extra_data);
void hdmi_api_power_on(void);
void hdmi_api_power_off(void);
void hdmi_start(void);
void hdmi_stop(void);

/* HDMI Audio */
int hdmi_api_audio_config(audioParams_t* audioParam);

/* HDMI PHY */
int hdmi_api_phy_power_down(void);
int hdmi_api_phy_config(unsigned int pixel_clock, unsigned int bpp);
int hdmi_api_video_config(videoParams_t *videoParam);
int hdmi_api_av_mute(int av_mute);

/* HDMI reg. */
unsigned int hdmi_api_ddi_reg_read(unsigned int offset);
void hdmi_api_ddi_reg_write(unsigned int data, unsigned int offset);
unsigned int hdmi_api_reg_read(unsigned int offset);
void hdmi_api_reg_write(unsigned int data, unsigned int offset);

/* HDCP callback */
typedef int (*hdcp_callback)(unsigned int);
int hdmi_api_reg_hdcp_callback(hdcp_callback status_chk);
int hdmi_api_run_hdcp_callback(unsigned int status);

#endif /* __TCC_HDMI_API_H__ */
