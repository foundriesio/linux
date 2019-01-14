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

#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <hdmi_api_lib/src/core/video/video_packetizer_reg.h>


#include "include/hdmi_drm.h"

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

void dwc_hdmi_api_register(struct hdmi_tx_dev *dev){
        hdmi_apis.dev = dev;
}

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
        if(hdmi_apis.dev != NULL) {
                hdmi_api_Disable(hdmi_apis.dev);
        }
}
EXPORT_SYMBOL(hdmi_stop);

int hdmi_get_VBlank(void){
        int vblank = 0;

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
        hdmi_apis.callback_hdmi_fbi = fb;
        hdmi_apis.callback_hdmi_stage = stage;
        hdmi_apis.callback = callback;
}
EXPORT_SYMBOL(hdmi_set_activate_callback);

void hdmi_activate_callback(void)
{
        if(hdmi_apis.callback != NULL) {
                hdmi_apis.callback(hdmi_apis.callback_hdmi_fbi, hdmi_apis.callback_hdmi_stage);
        }
}
EXPORT_SYMBOL(hdmi_activate_callback);

void hdmi_set_drm(DRM_Packet_t * drmparm)
{
        if(hdmi_apis.dev != NULL) {
                hdmi_update_drm_configure(hdmi_apis.dev, drmparm);
        }
}
EXPORT_SYMBOL(hdmi_set_drm);

int hdmi_get_hotplug_status(void)
{
        int hotplug_status = 0;
        if(hdmi_apis.dev != NULL) {
                hotplug_status = hdmi_apis.dev->hotplug_status;
        }
        return hotplug_status;
}
EXPORT_SYMBOL(hdmi_get_hotplug_status);

unsigned int hdmi_get_pixel_clock(void)
{
        unsigned int pixel_clock = 0;

        if(hdmi_apis.dev != NULL) {
                pixel_clock = hdmi_apis.dev->hdmi_tx_ctrl.pixel_clock;
        }

        return pixel_clock;
}
EXPORT_SYMBOL(hdmi_get_pixel_clock);

void hdmi_api_AvMute(int enable)
{
        do {
                if(hdmi_apis.dev == NULL) {
                        pr_err("%s device is not ready(NULL)\r\n", __func__);
                        break;
                }
                mutex_lock(&hdmi_apis.dev->mutex);
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_apis.dev->status)) {
                        if(!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_apis.dev->status)) {
                                pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                                mutex_unlock(&hdmi_apis.dev->mutex);
                                break;
                        }
                        hdmi_api_avmute(hdmi_apis.dev, enable);
                } else {
                        pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&hdmi_apis.dev->mutex);
        } while(0);
}

int hdmi_api_vsif_update_by_index(int index)
{
        int ret = -1;
        #if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
        videoParams_t *videoParams;

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
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_apis.dev->status)) {
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

