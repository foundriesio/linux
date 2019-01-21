/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

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
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <include/hdmi_ioctls.h>
#include <include/video_params.h>
#include <include/hdmi_drm.h>

#include <api/api.h>
#include <core/main_controller.h>
#include <core/video.h>
#include <core/audio.h>
#include <core/packets.h>
#include <core/irq.h>

#include <core/product.h>

#include <core/fc_debug.h>
#include <core/fc_video.h>
#include <core/interrupt/interrupt_reg.h>
#include <phy/phy.h>

#include <phy/phy_reg.h>
#include <hdmi_api_lib/src/core/main_controller/main_controller_reg.h>
#include <scdc/scdc.h>
#include <scdc/scrambling.h>
#include <hdcp/hdcp.h>


extern void dwc_hdmi_hw_reset(struct hdmi_tx_dev *dev, int reset_on) ;

static void hdmi_api_wait_phylock(struct hdmi_tx_dev *dev)
{
        int loop, phy_lock = 0;
        volatile unsigned int val = 0;

        hdmi_phy_enable_hpd_sense(dev);
        // PLL_LOCK
        dwc_hdmi_phy_clear_status_ready(dev);
        for(loop = 10, val = 0; (loop > 0) ; loop--){
                val = dwc_hdmi_phy_status_ready(dev);
                if(val) {
                        phy_lock |= (1 << 0);
                        break;
                }
                mdelay(10);
        }
        if(!val)
        goto END_PROCESS;

        // PHY_CLK_READY
        dwc_hdmi_phy_clear_status_clock_ready(dev);
        for(loop = 10, val = 0; (loop > 0) ; loop--){
                val = dwc_hdmi_phy_status_clock_ready(dev);
                if(val) {
                        phy_lock |= (1 << 1);
                        break;
                }
                mdelay(10);
        }
        if(!val)
        goto END_PROCESS;

        //  PHY_READY
        dwc_hdmi_phy_clear_status_pll_lock(dev);
        for(loop = 10, val = 0; (loop > 0) ; loop--){
                val = dwc_hdmi_phy_status_pll_lock(dev);
                if(val) {
                        phy_lock |= (1 << 2);
                        break;
                }
                mdelay(10);
        }
        if(!val)
        goto END_PROCESS;

        END_PROCESS:
        printk("HDMI_LOCK=0x%x\r\n", phy_lock);

}

int hdmi_api_Configure(struct hdmi_tx_dev *dev)
{
        int ret = -1;
        int mc_timeout = 100;
        unsigned int mc_reg_val;

        videoParams_t * video;
        audioParams_t * audio;
        productParams_t * product;

        do {

                if(dev == NULL) {
                        pr_err("%s dev is NULL\r\n", __func__);
                        break;
                }
                video = (videoParams_t*)dev->videoParam;
                audio = (audioParams_t*)dev->audioParam;
                product = (productParams_t*)dev->productParam;

                if(video == NULL || product == NULL) {
                        pr_err("%s invalid parameters: video = %p, product = %p \r\n", __func__, video, product);
                        break;
                }

                // Reset HDMI
                dwc_hdmi_hw_reset(dev, 1);
                dwc_hdmi_hw_reset(dev, 0);

                /* Initialize scdc status */
                dev->prev_scdc_status = -1;

                mc_disable_all_clocks(dev);
                if(dev->hdmi_tx_ctrl.sink_is_vizio == 1) {
                        /*
                         * In general, HDCP Keepout is set to 1 when TMDS frequencyrk is higher than
                         * 340 MHz or when HDCP is enabled.
                         * When HDCP Keepout is set to 1, the control period configuration is changed.
                         * Exceptionally, if HDCP keepout is set to 0 for VIZIO TV, there is a problem
                         * of swinging HPD.
                         * hdmi driver reads the EDID of the SINK and sets HDCP keepout to always 1
                         * if this SINK is a VIZIO TV. */
                        fc_video_hdcp_keepout(dev, 1);
                        pr_info("NOTIFY: VIZIO TV\r\n");
                } else {
                        dwc_hdmi_set_hdcp_keepout(dev);
                }

                hdmi_api_avmute(dev, TRUE);

                ret = video_Configure(dev, video);
                if (ret < 0) {
                        pr_err("%s:Could not configure video", __func__);
                        break;
                }

                /* Setting audio_on through hdmi mode */
                dev->hdmi_tx_ctrl.audio_on = (video->mHdmi == DVI)?0:1;

                if(audio == NULL) {
                        pr_info("%s there is no audio packet\r\n", __func__);
                }else {
                        audio_Initialize(dev);
                        ret = audio_Configure(dev, audio);
                        if(ret == FALSE){
                                pr_err("%s Audio not configured\r\n", __func__);
                                ret = -1;
                                break;
                        }
                }

                /**
                  * Source Product Description
                  * If the vendor and product names are not set, the kernel uses predefined names. */
                if(product->mVendorNameLength == 0 && product->mProductNameLength == 0) {
                        strncpy(product->mVendorName, dev->vendor_name, sizeof(product->mVendorName));
                        product->mVendorNameLength = strlen(product->mVendorName);
                        if(product->mVendorNameLength > 8) product->mVendorNameLength = 8;

                        strncpy(product->mProductName, dev->product_description, sizeof(product->mProductName));
                        product->mProductNameLength = strlen(product->mProductName);
                        if(product->mProductNameLength > 16) product->mProductNameLength = 16;

                        product->mSourceType = dev->source_information;
                }

                // Packets
                ret = packets_Configure(dev, video, product);
                if (ret == FALSE) {
                        pr_err("%s Could not configure packets\r\n", __func__);
                        ret = -1;
                        break;
                }

                mc_enable_all_clocks(dev);

		/* Configure the hdmi phy */
                if(dwc_hdmi_phy_config(dev, video) < 0) {
                        pr_err("%s Cann't settig HDMI PHY\r\n", __func__);
                        ret = -1;
                        break;
                }
                hdmi_api_wait_phylock(dev);

		/* Reset the main controller */
		hdmi_dev_write(dev, MC_SWRSTZREQ, 0);
		
                /* wait main controller to resume */
                do {
                        usleep_range(10, 20);
                        mc_reg_val = hdmi_dev_read(dev, MC_SWRSTZREQ);
                }
                while(mc_timeout-- && mc_reg_val != 0xDF);
		if(mc_timeout < 1) {
			pr_info("%s main controller timeout \r\n",__func__);
		}

                fc_video_VSyncPulseWidth(dev, video->mDtd.mVSyncPulseWidth);

                hdmi_hpd_enable(dev);

                set_bit(HDMI_TX_STATUS_OUTPUT_ON, &dev->status);

                // Initialize the drm info frame.
                hdmi_clear_drm(dev);

                ret = 0;
        }while(0);


        return ret;
}

int hdmi_api_Disable(struct hdmi_tx_dev *dev)
{
        int ret = -1;

        videoParams_t *videoParams = (videoParams_t *)(dev!=NULL)?dev->videoParam:NULL;

        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                /* Disable HDMI PHY clock */
                dwc_hdmi_phy_standby(dev);
                clear_bit(HDMI_TX_STATUS_OUTPUT_ON, &dev->status);
        }
	ret = 0;

        if(dev != NULL) {
                hdcp_statusinit(dev);
        }

        return ret ;
}


void hdmi_api_avmute(struct hdmi_tx_dev *dev, int enable)
{
        if(dev != NULL) {
                packets_AvMute(dev, enable);
        }
}
EXPORT_SYMBOL(hdmi_api_avmute);
