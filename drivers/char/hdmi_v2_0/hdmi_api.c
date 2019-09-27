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
****************************************************************************/
#define pr_fmt(fmt) "\x1b[1;38m HDMI_API: \x1b[0m" fmt

#include <include/hdmi_includes.h>
#include <include/irq_handlers.h>
#include <include/hdmi_ioctls.h>
#include <include/hdmi_access.h>
#include <include/video_params.h>
#include <include/audio_params.h>
#include <include/hdmi_drm.h>
#include <core/packets.h>
#include <api/api.h>
#include <phy/phy.h>
#include <linux/of_gpio.h>


#if 0//defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_v_dv.h>
#endif
#include <core/fc_video.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <hdmi_api_lib/src/core/main_controller/main_controller_reg.h>
#include <hdmi_api_lib/src/core/video/video_packetizer_reg.h>


#include "include/hdmi_drm.h"

#define HDMI_API_DEBUG 0
#define dpr_info(msg, ...) if(HDMI_API_DEBUG) { pr_info(msg, ##__VA_ARGS__); }

static struct {
        struct hdmi_tx_dev *dev;
        void (*callback)(int , int);
        int callback_hdmi_stage;
        int callback_hdmi_fbi;
} hdmi_apis = {
        .dev = NULL,
        .callback = NULL,
        .callback_hdmi_stage = 0,
        .callback_hdmi_fbi = 0,
};

extern void dwc_hdmi_power_on(struct hdmi_tx_dev *dev);
extern void dwc_hdmi_power_off(struct hdmi_tx_dev *dev);

void dwc_hdmi_api_register(struct hdmi_tx_dev *dev){
        hdmi_apis.dev = dev;
}
EXPORT_SYMBOL(dwc_hdmi_api_register);

struct hdmi_tx_dev *dwc_hdmi_api_get_dev(void){
        return hdmi_apis.dev;
}
EXPORT_SYMBOL(dwc_hdmi_api_get_dev);

int dwc_hdmi_get_video_dtd(dtd_t *hdmi_dtd, uint32_t code, uint32_t hz){
        return hdmi_dtd_fill(hdmi_dtd, code, hz);
}
EXPORT_SYMBOL(dwc_hdmi_get_video_dtd);

void hdmi_start(void){
        // Must Implement It..!!
}
EXPORT_SYMBOL(hdmi_start);

void hdmi_stop(void){
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.dev != NULL) {
		if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_apis.dev->status)) {
	                if(test_bit(HDMI_TX_STATUS_OUTPUT_ON, &hdmi_apis.dev->status)) {
	                        //pr_info("%s avmute\r\n", __func__);
	                        hdmi_api_avmute(hdmi_apis.dev, 1);
	                        mdelay(85);
	                        //pr_info("%s hdmi api disable\r\n", __func__);
	                        hdmi_api_Disable(hdmi_apis.dev);
	                }
		}
        }
}
EXPORT_SYMBOL(hdmi_stop);

int hdmi_get_VBlank(void){
        int vblank = 0;
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.dev != NULL) {
                if(test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                        vblank = hdmi_dev_read(hdmi_apis.dev, FC_INVBLANK);
                        vblank &= 0xFF;
                }
        }
        return vblank;
}
EXPORT_SYMBOL(hdmi_get_VBlank);

unsigned int hdmi_get_refreshrate(void)
{
        unsigned int refreshrate = 0;
        videoParams_t *videoParams =
                (hdmi_apis.dev != NULL)?(videoParams_t*)hdmi_apis.dev->videoParam:NULL;
	dpr_info("%s\r\n", __func__);;
        if(videoParams != NULL) {
                refreshrate = hdmi_dtd_get_refresh_rate(&videoParams->mDtd);
                if(refreshrate > 1000) {
                        refreshrate /= 1000;
                }
        }
        return refreshrate;
}
EXPORT_SYMBOL(hdmi_get_refreshrate);

void hdmi_set_activate_callback(void(*callback)(int, int), int fb, int stage)
{
	dpr_info("%s\r\n", __func__);;
        hdmi_apis.callback_hdmi_fbi = fb;
        hdmi_apis.callback_hdmi_stage = stage;
        hdmi_apis.callback = callback;
}
EXPORT_SYMBOL(hdmi_set_activate_callback);

void hdmi_activate_callback(void)
{
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.callback != NULL) {
                hdmi_apis.callback(hdmi_apis.callback_hdmi_fbi, hdmi_apis.callback_hdmi_stage);
        }
}
EXPORT_SYMBOL(hdmi_activate_callback);

void hdmi_set_drm(DRM_Packet_t * drmparm)
{
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.dev != NULL) {
                hdmi_update_drm_configure(hdmi_apis.dev, drmparm);
        }
}
EXPORT_SYMBOL(hdmi_set_drm);

int hdmi_get_hotplug_status(void)
{
        int hotplug_status = 0;
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.dev != NULL) {
                hotplug_status = hdmi_apis.dev->hotplug_status;
        }
        return hotplug_status;
}
EXPORT_SYMBOL(hdmi_get_hotplug_status);

unsigned int hdmi_get_pixel_clock(void)
{
        unsigned int pixel_clock = 0;
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.dev != NULL) {
                pixel_clock = hdmi_apis.dev->hdmi_tx_ctrl.pixel_clock;
        }

        return pixel_clock;
}
EXPORT_SYMBOL(hdmi_get_pixel_clock);

void hdmi_api_AvMute(int enable)
{
	dpr_info("%s\r\n", __func__);;
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }
                mutex_lock(&hdmi_apis.dev->mutex);
                hdmi_api_avmute(hdmi_apis.dev, enable);
                mutex_unlock(&hdmi_apis.dev->mutex);
        } while(0);
}
EXPORT_SYMBOL(hdmi_api_AvMute);

int hdmi_api_vsif_update(productParams_t *productParams)
{
        int ret = -1;
	dpr_info("%s\r\n", __func__);;
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }
                if(productParams == NULL) {
                        pr_err("%s productParams is NULL\r\n", __func__);
                        break;
                }

                mutex_lock(&hdmi_apis.dev->mutex);
                if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                mutex_unlock(&hdmi_apis.dev->mutex);
                                break;
                        }
                        ret = vendor_Configure(hdmi_apis.dev, productParams);
                } else {
                        pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&hdmi_apis.dev->mutex);
        }while(0);
        return ret;
}
EXPORT_SYMBOL(hdmi_api_vsif_update);

int hdmi_api_vsif_update_for_hdr_10p(productParams_t *productParams)
{
	int ret = -1;
	dpr_info("%s\r\n", __func__);;
	do {
		if(hdmi_apis.dev == NULL) {
			pr_err("%s device is not ready(NULL)\r\n", __func__);
			break;
		}
		if(productParams == NULL) {
			pr_err("%s productParams is NULL\r\n", __func__);
			break;
		}

		mutex_lock(&hdmi_apis.dev->mutex);
		if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
			if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
				pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
				mutex_unlock(&hdmi_apis.dev->mutex);
				break;
			}
			if(!test_bit(HDMI_TX_VSIF_UPDATE_FOR_HDR_10P, &hdmi_apis.dev->status)) {
				dpr_info("%s has not permissions\r\n", __func__);
				mutex_unlock(&hdmi_apis.dev->mutex);
				break;
			}
			ret = vendor_Configure(hdmi_apis.dev, productParams);
		} else {
			pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
		}
		mutex_unlock(&hdmi_apis.dev->mutex);
	}while(0);
	return ret;
}
EXPORT_SYMBOL(hdmi_api_vsif_update_for_hdr_10p);

int hdmi_api_vsif_update_by_index(int index)
{
        int ret = -1;
        #if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
        videoParams_t *videoParams;
	dpr_info("%s\r\n", __func__);;
        unsigned char* base_vsif = NULL;
        productParams_t productParams;
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }

                videoParams = (videoParams_t*)hdmi_apis.dev->videoParam;

                if(videoParams == NULL) {
                        pr_err("%s videoParams is NULL\r\n", __func__);
                        break;
                }

                if(hdmi_apis.dev->dolbyvision_visf_list == NULL) {
                        pr_err("%s dolbyvision vsif is empty\r\n", __func__);
                        break;
                }

                if(index < 0 || index > 5) {
                        pr_err("%s index(%d) is out of range\r\n", __func__, index);
                        break;
                }

                base_vsif = (hdmi_apis.dev->dolbyvision_visf_list + (50 * index));

                mutex_lock(&hdmi_apis.dev->mutex);
                if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                mutex_unlock(&hdmi_apis.dev->mutex);
                                break;
                        }
                        /* Copy VSIF Packet */
                        productParams.mOUI = base_vsif[0] | (base_vsif[1] << 8) | (base_vsif[2] << 16);
                        productParams.mVendorPayloadLength = base_vsif[3];
                        memcpy(productParams.mVendorPayload, &base_vsif[4], productParams.mVendorPayloadLength);

                        if(productParams.mOUI == 0x00030C00) {
                                switch(videoParams->mDtd.mCode) {
                                        case 93: // 3840x2160p24Hz
                                                productParams.mVendorPayload[1] = 3;
                                                break;
                                        case 94: // 3840x2160p25Hz
                                                productParams.mVendorPayload[1] = 2;
                                                break;
                                        case 95: // 3840x2160p30Hz
                                                productParams.mVendorPayload[1] = 1;
                                                break;
                                        case 98: // 4096x2160p24Hz
                                                productParams.mVendorPayload[1] = 4;
                                                break;
                                }

                                if(productParams.mVendorPayload[1] > 0) {
                                        if(productParams.mVendorPayloadLength == 1) {
                                                productParams.mVendorPayloadLength = 2;
                                        }
                                        productParams.mVendorPayload[0] = 1 << 5;
                                }
                        }
                        ret = vendor_Configure(hdmi_apis.dev, &productParams);
                } else {
                        pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&hdmi_apis.dev->mutex);
        }while(0);
        #endif
        return ret;
}
EXPORT_SYMBOL(hdmi_api_vsif_update_by_index);

/**
 * @short This api is controls the hdmi power.
 * @param[in] enable Parameter to enable or disable hdmi power.
 *            0: disable, 1: enable
 * @return none
 */
void hdmi_api_power_control(int enable)
{
	dpr_info("%s\r\n", __func__);;
        if(hdmi_apis.dev != NULL) {
                mutex_lock(&hdmi_apis.dev->mutex);
                if(enable) {
                        clear_bit(HDMI_TX_STATUS_PHY_ALIVE, &hdmi_apis.dev->status);
                        dwc_hdmi_power_on(hdmi_apis.dev);
                } else {
                        clear_bit(HDMI_TX_STATUS_PHY_ALIVE, &hdmi_apis.dev->status);
                        dwc_hdmi_power_off(hdmi_apis.dev);
                }
                mutex_unlock(&hdmi_apis.dev->mutex);
        }
}
EXPORT_SYMBOL(hdmi_api_power_control);


static int hdmi_api_update_avi_infoframe(videoParams_t *videoParam)
{

        int ret = -1;
        int mc_timeout = 100;
        volatile unsigned int mc_reg_val;
	dpr_info("%s\r\n", __func__);;
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }
                if(videoParam == NULL) {
                        pr_err("%s videoParam is NULL\r\n", __func__);
                        break;
                }
		if(hdmi_apis.dev->videoParam == NULL) {
			pr_err("%s videoParam of dev is NULL\r\n", __func__);
                        break;
		}

                if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                break;
                        }

			if(packets_Configure(hdmi_apis.dev, videoParam, NULL) < 0) {
				pr_err("%s failed packets_Configure <%d>\r\n", __func__, __LINE__);
                                break;
			}
			/* Update Video parameters */
			memcpy(hdmi_apis.dev->videoParam, videoParam, sizeof(videoParams_t));

			/* Reset the main controller */
			hdmi_dev_write(hdmi_apis.dev, MC_SWRSTZREQ, 0);

	                /* wait main controller to resume */
	                do {
	                        usleep_range(10, 20);
	                        mc_reg_val = hdmi_dev_read(hdmi_apis.dev, MC_SWRSTZREQ);
	                }
	                while(mc_timeout-- && mc_reg_val != 0xDF);
			if(mc_timeout < 1) {
				pr_info("%s main controller timeout \r\n",__func__);
			}

	                fc_video_VSyncPulseWidth(hdmi_apis.dev, videoParam->mDtd.mVSyncPulseWidth);

			ret = 0;
                } else {
                        pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                }
        }while(0);
        return ret;
}


/**
 * @short This function sets colorimetry and stores it to device context.
 * @param[in] colorimetry Stores colorimetry value
 * @param[in] ext_colorimetry Store extended colorimetry value
 *            If colorimetry is not EXTENDED_COLORIMETRY, this value is ignored.
 * @return Return the result of this function
 * @retval 0 if this function succeeded \n
 * @retval -1 otherwise
 */
int hdmi_api_update_colorimetry(
	colorimetry_t colorimetry,
	ext_colorimetry_t ext_colorimetry)
{
        int ret = -1;
	videoParams_t videoParam;
	dpr_info("%s\r\n", __func__);;
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }
		if(hdmi_apis.dev->videoParam == NULL) {
			pr_err("%s videoParam of dev is NULL\r\n", __func__);
                        break;
		}
                mutex_lock(&hdmi_apis.dev->mutex);
                if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                mutex_unlock(&hdmi_apis.dev->mutex);
                                break;
                        }
			memcpy(&videoParam, hdmi_apis.dev->videoParam, sizeof(videoParams_t));
			videoParam.mColorimetry = colorimetry;
			videoParam.mExtColorimetry = ext_colorimetry;

			ret = hdmi_api_update_avi_infoframe(&videoParam);
                } else {
                        pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&hdmi_apis.dev->mutex);
        }while(0);
        return ret;
}
EXPORT_SYMBOL(hdmi_api_update_colorimetry);

/**
 * @short This function sets quantization and stores it to device context.
 * @param[in] quantization_range Stores quantization range value
 *           [0] For RGB, This value means that the quantization range corresponds
 *               to the default RGB quantization range.
 *               For YUV, this value means that the quantization range corresponds
 *               to the  limited quantization range
 *           [1] For RGB, This value means that the quantization range corresponds
 *               to the limited quantization range.
 *               For YUV, this value means that the quantization range corresponds
 *               to the  limited quantization range
 *           [2] For RGB, This value means that the quantization range corresponds
 *               to the full quantization range.
 *               For YUV, this value means that the quantization range corresponds
 *               to the full quantization range
 * @return Return the result of this function
 * @retval 0 if this function succeeded \n
 * @retval -1 otherwise
 */
int hdmi_api_update_quantization(int quantization_range)
{
        int ret = -1;
	videoParams_t videoParam;
	dpr_info("%s\r\n", __func__);;
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }

		if(hdmi_apis.dev->videoParam == NULL) {
			pr_err("%s videoParam of dev is NULL\r\n", __func__);
                        break;
		}
                mutex_lock(&hdmi_apis.dev->mutex);
                if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                mutex_unlock(&hdmi_apis.dev->mutex);
                                break;
                        }
			memcpy(&videoParam, hdmi_apis.dev->videoParam, sizeof(videoParams_t));

			switch(quantization_range) {
				case 0:
					/* RGB : default range for the transmitted Video Format.
					 * YuV : Limited Range */
					videoParam.mRgbQuantizationRange = 0;
					videoParam.mYuvQuantizationRange = 0;
					break;
				case 1:
					/* Limited Range */
					videoParam.mRgbQuantizationRange = 1;
					videoParam.mYuvQuantizationRange = 0;
					break;
				case 2:
					/* Full Range */
					videoParam.mRgbQuantizationRange = 2;
					videoParam.mYuvQuantizationRange = 1;
					break;
			}

			ret = hdmi_api_update_avi_infoframe(&videoParam);
                } else {
                        pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&hdmi_apis.dev->mutex);
        }while(0);
        return ret;
}
EXPORT_SYMBOL(hdmi_api_update_quantization);

int hdmi_api_dump_regs(void)
{
	unsigned int i, reg_val;
	do {
		if(hdmi_apis.dev == NULL) {
			pr_err("%s device is not ready(NULL)\r\n", __func__);
			break;
		}
		mutex_lock(&hdmi_apis.dev->mutex);
                if(!dwc_hdmi_is_suspended(hdmi_apis.dev)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                mutex_unlock(&hdmi_apis.dev->mutex);
                                break;
                        }
			pr_info("\r\nDUMP IRQ\r\n");
			for(i = 0x00000400; i <= 0x000007FC; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP Video Sampler\r\n");
			for(i = 0x00000800; i <= 0x0000081C; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP Video Packetizer\r\n");
			for(i = 0x00002000; i <= 0x0000201C; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP Frame Composer\r\n");
			for(i = 0x00004000; i <= 0x00004C00; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP PHY Configure\r\n");
			for(i = 0x0000C000; i <= 0x0000C0E0; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP Audio Sample\r\n");
			for(i = 0x0000C400; i <= 0x0000C410; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			printk("\r\n");
			pr_info("\r\nDUMP Audio Packetizer\r\n");
			for(i = 0x0000C800; i <= 0x0000C81C; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP Audio Sample SPDIF\r\n");
			for(i = 0x0000CC00; i <= 0x0000CC10; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			pr_info("\r\nDUMP MainController\r\n");
			for(i = 0x0001000; i <= 0x0001028; i+=4) {
				reg_val = hdmi_dev_read(hdmi_apis.dev, i);
				if((i & 0xF) == 0) {
					printk("\r\n[0x%08x] %02x ", i, reg_val);
				} else {
					printk("%02x ", reg_val);
				}
			}
			printk("\r\n");
		} else {
			pr_err("## Failed to dump because hdmi linke was suspended\r\n");
		}
	}while(0);
	return 0;
}
EXPORT_SYMBOL(hdmi_api_dump_regs);


