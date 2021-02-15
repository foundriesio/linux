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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi_1_4_api.h>

#include <hdmi.h>
#include <regs-hdmi.h>


static struct tcc_hdmi_dev *tcc_hdmi_api_dev = NULL;


void hdmi_api_register_dev(struct tcc_hdmi_dev *dev)
{
        tcc_hdmi_api_dev = dev;
}

int hdmi_api_get_power_status(void)
{
        return hdmi_get_power_status(tcc_hdmi_api_dev);
}

EXPORT_SYMBOL(hdmi_api_get_power_status);

int hdmi_get_VBlank(void)
{
        int blank = 0;
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        if(tcc_hdmi_api_dev->power_status) {
                                blank = hdmi_get_VBlank_internal(tcc_hdmi_api_dev);
                        } else {
                                pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
        return blank;
}
EXPORT_SYMBOL(hdmi_get_VBlank);


void hdmi_start(void)
{
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        if(tcc_hdmi_api_dev->power_status) {
                                hdmi_start_internal(tcc_hdmi_api_dev);
                        } else {
                                pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
}
EXPORT_SYMBOL(hdmi_start);

void hdmi_stop(void)
{
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        if(tcc_hdmi_api_dev->power_status) {
                                hdmi_stop_internal(tcc_hdmi_api_dev);
                        } else {
                                pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
}
EXPORT_SYMBOL(hdmi_stop);

void hdmi_api_power_on(void)
{
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        tcc_hdmi_power_on(tcc_hdmi_api_dev);
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
}
EXPORT_SYMBOL(hdmi_api_power_on);

void hdmi_api_power_off(void)
{
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        tcc_hdmi_power_off(tcc_hdmi_api_dev);
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
}
EXPORT_SYMBOL(hdmi_api_power_off);


int hdmi_api_set_spd_infoframe(packetParam_t *packetParam)
{
	int ret = -1;
	if(tcc_hdmi_api_dev != NULL) {
		if(!tcc_hdmi_api_dev->suspend) {
			if(packetParam != NULL) {
				memcpy(&tcc_hdmi_api_dev->packetParam.spd, &packetParam->spd, sizeof(spd_packet_t));
				ret = hdmi_set_spd_infoframe(tcc_hdmi_api_dev);
			}
		} else {
			pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
		}
	}
	return ret;
}

int hdmi_api_set_vsif_infoframe(packetParam_t *packetParam)
{
	int ret = -1;
	if(tcc_hdmi_api_dev != NULL) {
		if(!tcc_hdmi_api_dev->suspend) {
			if(packetParam != NULL) {
				memcpy(&tcc_hdmi_api_dev->packetParam.vsif,&packetParam->vsif, sizeof(vendor_packet_t));
				ret = hdmi_set_vsif_infoframe(tcc_hdmi_api_dev);
			}
		} else {
			pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
		}
	}
	return ret;
}
EXPORT_SYMBOL(hdmi_api_set_vsif_infoframe);

int hdmi_api_dtd_fill(dtd_t * dtd, unsigned int code, unsigned int refreshRate)
{
        return hdmi_dtd_fill(dtd, code, refreshRate);
}
EXPORT_SYMBOL(hdmi_api_dtd_fill);

unsigned int hdmi_api_dtd_get_vactive(videoParams_t *videoParam)
{
	return hdmi_dtd_get_vactive(videoParam);
}
EXPORT_SYMBOL(hdmi_api_dtd_get_vactive);


int hdmi_api_dtd_get_display_param(videoParams_t *videoParam,
			struct lcdc_timimg_parms_t *lcdc_timimg_parms)
{
	return hdmi_dtd_get_display_param(videoParam, lcdc_timimg_parms);
}
EXPORT_SYMBOL(hdmi_api_dtd_get_display_param);

int hdmi_api_dtb_get_extra_data(videoParams_t *videoParam,
			struct tcc_fb_extra_data *tcc_fb_extra_data)
{
	return hdmi_dtb_get_extra_data(videoParam, tcc_fb_extra_data);
}
EXPORT_SYMBOL(hdmi_api_dtb_get_extra_data);

int hdmi_api_phy_power_down(void)
{
        int ret = -1;
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        if(tcc_hdmi_api_dev->power_status) {
                                ret =  tcc_hdmi_phy_power_down(tcc_hdmi_api_dev);
                        }
                        else {
                                pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
        return ret;
}
EXPORT_SYMBOL(hdmi_api_phy_power_down);

int hdmi_api_phy_config(unsigned int pixel_clock, unsigned int bpp)
{
        int ret = -1;
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        if(tcc_hdmi_api_dev->power_status) {
                                ret =  tcc_hdmi_phy_config(tcc_hdmi_api_dev, pixel_clock, bpp);
                        }
                        else {
                                pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
        return ret;
}
EXPORT_SYMBOL(hdmi_api_phy_config);

int hdmi_api_video_config(videoParams_t *videoParam)
{
	int ret = -1;

	do {
		if(tcc_hdmi_api_dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s tcc_hdmi_api_dev is NULL\r\n", __func__);
			break;
		}
		if(videoParam == NULL) {
			pr_err("[ERROR][HDMI_V14]%s videoParam is NULL\r\n", __func__);
			break;
		}

		/* Store video paramter to device contxt */
		memcpy(&tcc_hdmi_api_dev->videoParam, videoParam, sizeof(videoParams_t));

		if(hdmi_video_config(tcc_hdmi_api_dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set video configure\r\n", __func__);
			break;
		}


		ret = 0;
	} while(0);
	return ret;
}
EXPORT_SYMBOL(hdmi_api_video_config);


int hdmi_api_av_mute(int av_mute)
{
        int ret = -1;
        if(tcc_hdmi_api_dev != NULL) {
                if(!tcc_hdmi_api_dev->suspend) {
                        if(tcc_hdmi_api_dev->power_status) {
                                ret =  hdmi_av_mute(tcc_hdmi_api_dev, av_mute);
                        }
                        else {
                                pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
        return ret;
}
EXPORT_SYMBOL(hdmi_api_av_mute);

int hdmi_api_audio_config(audioParams_t* audioParam)
{
	int ret = -1;

	do {
		if(tcc_hdmi_api_dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s tcc_hdmi_api_dev is NULL\r\n", __func__);
			break;
		}
		if(audioParam == NULL) {
			pr_err("[ERROR][HDMI_V14]%s audioParam is NULL\r\n", __func__);
			break;
		}

		/* Store video paramter to device contxt */
		memcpy(&tcc_hdmi_api_dev->audioParam, audioParam, sizeof(videoParams_t));
		if(hdmi_audio_set_mode(tcc_hdmi_api_dev) < 0) {
			break;
		}
		ret = hdmi_audio_set_enable(tcc_hdmi_api_dev, 1);
	} while(0);
	return ret;
}
EXPORT_SYMBOL(hdmi_api_audio_config);

unsigned int hdmi_api_ddi_reg_read(unsigned int offset)
{
	unsigned int data = 0;

	data = ddi_reg_read(tcc_hdmi_api_dev, offset);

	return data;
}
EXPORT_SYMBOL(hdmi_api_ddi_reg_read);

void hdmi_api_ddi_reg_write(unsigned int data, unsigned int offset)
{
	ddi_reg_write(tcc_hdmi_api_dev, data, offset);
}
EXPORT_SYMBOL(hdmi_api_ddi_reg_write);

unsigned int hdmi_api_reg_read(unsigned int offset)
{
	unsigned int data = 0;

	data = hdmi_reg_read(tcc_hdmi_api_dev, offset);
	data &= 0xFF;

	return data;
}
EXPORT_SYMBOL(hdmi_api_reg_read);

void hdmi_api_reg_write(unsigned int data, unsigned int offset)
{
	data &= 0xFF;
	hdmi_reg_write(tcc_hdmi_api_dev, data, offset);
}
EXPORT_SYMBOL(hdmi_api_reg_write);

static hdcp_callback hdcp_cb = NULL;
int hdmi_api_reg_hdcp_callback(hdcp_callback status_chk)
{
	hdcp_cb = status_chk;

	return 0;
}
EXPORT_SYMBOL(hdmi_api_reg_hdcp_callback);

int hdmi_api_run_hdcp_callback(unsigned int status)
{
	int ret = -1;

	if (hdcp_cb) {
		(* hdcp_cb)(status);
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(hdmi_api_run_hdcp_callback);
