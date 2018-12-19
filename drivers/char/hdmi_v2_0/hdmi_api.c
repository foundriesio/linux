/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        hdmi_api.c
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular
purpose or non-infringement  of  any  patent,  copyright  or  other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source  code or the  use in the
source code.
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure
Agreement between Telechips and Company.
*******************************************************************************/

#include <include/hdmi_includes.h>
#include <include/irq_handlers.h>
#include <include/hdmi_ioctls.h>
#include <include/hdmi_access.h>
#include <include/video_params.h>
#include <include/audio_params.h>
#include <include/hdmi_drm.h>
#include <api/api.h>
#include <phy/phy.h>
#include <linux/of_gpio.h>


#if 0//defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_v_dv.h>
#endif
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <hdmi_api_lib/src/core/video/video_packetizer_reg.h>


#include "include/hdmi_drm.h"

extern void dwc_hdmi_phy_standby(struct hdmi_tx_dev *dev);

static struct hdmi_tx_dev *api_dev = NULL;
static void (*callback_hdmi_activate)(int , int) = NULL;
static int callback_hdmi_stage = 0;
static int callback_hdmi_fbi = 0;

void dwc_hdmi_api_register(struct hdmi_tx_dev *dev){
        api_dev = dev;
}

struct hdmi_tx_dev *dwc_hdmi_api_get_dev(void){
        return api_dev;
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
        if(api_dev != NULL) {
                hdmi_api_Disable(api_dev);
        }
}
EXPORT_SYMBOL(hdmi_stop);

int hdmi_get_VBlank(void){
        int vblank = 0;

        if(!api_dev) {
                pr_err("hdmi driver is not ready..!!\r\n");
                goto end_process;
        }
        if(test_bit(HDMI_TX_STATUS_POWER_ON, &api_dev->status)) {
                vblank = hdmi_dev_read(api_dev, FC_INVBLANK);
        }

end_process:
        return (vblank & 0xFF);
}
EXPORT_SYMBOL(hdmi_get_VBlank);

unsigned int hdmi_get_refreshrate(void) {

        videoParams_t *videoParams;
        unsigned int refreshrate = 0;

        if(!api_dev) {
                pr_err("hdmi driver is not ready..!!\r\n");
                goto end_process;
        }

        videoParams = (videoParams_t*)api_dev->videoParam;

        refreshrate = hdmi_dtd_get_refresh_rate(&videoParams->mDtd);

        if(refreshrate > 1000)
                refreshrate /= 1000;
end_process:
        return refreshrate;

}
EXPORT_SYMBOL(hdmi_get_refreshrate);

void hdmi_phy_standby(void) {
        if(!api_dev) {
                pr_err("hdmi_phy_standby - hdmi driver is not ready..!!\r\n");
        }else if(test_bit(HDMI_TX_STATUS_POWER_ON, &api_dev->status)) {
                dwc_hdmi_phy_standby(api_dev);
        }
}
EXPORT_SYMBOL(hdmi_phy_standby);

void hdmi_set_activate_callback(void(*callback)(int, int), int fb, int stage)
{
		callback_hdmi_fbi = fb;
        callback_hdmi_stage = stage;
        callback_hdmi_activate = callback;
}
EXPORT_SYMBOL(hdmi_set_activate_callback);

void hdmi_activate_callback(void)
{
        if(callback_hdmi_activate != NULL)
                callback_hdmi_activate(callback_hdmi_fbi, callback_hdmi_stage);
}
EXPORT_SYMBOL(hdmi_activate_callback);

void hdmi_set_drm(DRM_Packet_t * drmparm)
{
#if 0//defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH() && (HDR10 != vioc_get_out_type()))
		return;
#endif

        if(!api_dev) {
                pr_err("hdmi driver is not ready..!!\r\n");
        } else {
                hdmi_update_drm_configure(api_dev, drmparm);
        }
}
EXPORT_SYMBOL(hdmi_set_drm);

/* It will be deprecated.
void hdmi_clear_drm(void)
{
        if(!api_dev) {
                pr_err("hdmi driver is not ready..!!\r\n");
        } else {
                mutex_lock(&api_dev->mutex);
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &api_dev->status)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &api_dev->status)) {
                                drm_tx_disable(api_dev);
                        }
                }
                mutex_unlock(&api_dev->mutex);
        }
}
EXPORT_SYMBOL(hdmi_clear_drm);
*/

int hdmi_get_hotplug_status(void)
{
        int hotplug_status = 0;
        if(api_dev != NULL) {
                hotplug_status = api_dev->hotplug_status;
        }
        return hotplug_status;
}
EXPORT_SYMBOL(hdmi_get_hotplug_status);

unsigned int hdmi_get_pixel_clock(void)
{
        unsigned int pixel_clock = 0;

        if(api_dev != NULL) {
                pixel_clock = api_dev->hdmi_tx_ctrl.pixel_clock;
        }

        return pixel_clock;
}
EXPORT_SYMBOL(hdmi_get_pixel_clock);

