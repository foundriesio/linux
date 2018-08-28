/****************************************************************************
Copyright (C) 2013 Telechips Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/
#include "include/hdmi_includes.h"
#include "include/hdmi_ioctls.h"
#include "include/hdmi_access.h"
#include "include/hdmi_drm.h"

#include "hdmi_api_lib/include/core/interrupt/interrupt_reg.h"
#include "hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h"

static void hdmi_reset_drmparm(DRM_Packet_t * drmparm)
{
        memset(drmparm, 0, sizeof(DRM_Packet_t));
        drmparm->mInfoFrame.version = 1;
        drmparm->mInfoFrame.length = 0x1A; // 26 
}

static int drm_infoframe_verification(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm)
{
        int valid = 0;
                
        if(drmparm->mInfoFrame.version != 1) {
                printk("drm_infoframe_verification: versio is mismatch\r\n");
                goto end_process;
        }
        if(drmparm->mInfoFrame.length < 1) {
                printk("drm_infoframe_verification: length is mismatch\r\n");
                goto end_process;
        }
        if(drmparm->mDescriptor_type1.EOTF > 3) {
                printk("drm_infoframe_verification: eotf is mismatch\r\n");
                goto end_process;
        }
        if(drmparm->mDescriptor_type1.Descriptor_ID > 0) {
                printk("drm_infoframe_verification: id is mismatch\r\n");
             goto end_process;   
        }
        valid = 1;
        
end_process:
        return valid;
}

static void drm_tx_enable(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write_mask(dev,FC_PACKET_TX_EN,FC_PACKET_TX_EN_DRM_TX_EN_MASK,1);
}
/*
static void drm_tx_disable(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write_mask(dev,FC_PACKET_TX_EN,FC_PACKET_TX_EN_DRM_TX_EN_MASK,0);
}

static int drm_status_check(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, IH_FC_STAT2, IH_FC_STAT2_DRM_MASK);
}
*/
static void drm_update(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write_mask(dev,FC_DRM_UP,FC_DRM_UP_PACKET_UPDATE_MASK,1);
}

static void drm_configure(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm)
{
	hdmi_dev_write(dev,FC_DRM_HB0_DATA_BYTE,drmparm->mInfoFrame.version);
	hdmi_dev_write(dev,FC_DRM_HB1_DATA_BYTE,drmparm->mInfoFrame.length);

	hdmi_dev_write(dev,FC_DRM_PB0_DATA_BYTE,(drmparm->mDescriptor_type1.EOTF & 0x0007));
	hdmi_dev_write(dev,FC_DRM_PB1_DATA_BYTE,(drmparm->mDescriptor_type1.Descriptor_ID & 0x0007));	

	if(drmparm->mDescriptor_type1.EOTF < 3) {
		// HDR-10
		hdmi_dev_write(dev,FC_DRM_PB2_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_x[0] & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB3_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_x[0] & 0xFF00) >> DRM_BIT_OFFSET);
		hdmi_dev_write(dev,FC_DRM_PB4_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_y[0] & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB5_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_y[0] & 0xFF00) >> DRM_BIT_OFFSET);
		
		hdmi_dev_write(dev,FC_DRM_PB6_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_x[1] & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB7_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_x[1] & 0xFF00) >> DRM_BIT_OFFSET);
		hdmi_dev_write(dev,FC_DRM_PB8_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_y[1] & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB9_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_y[1] & 0xFF00) >> DRM_BIT_OFFSET);
		
		hdmi_dev_write(dev,FC_DRM_PB10_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_x[2] & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB11_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_x[2] & 0xFF00) >> DRM_BIT_OFFSET);
		hdmi_dev_write(dev,FC_DRM_PB12_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_y[2] & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB13_DATA_BYTE,(drmparm->mDescriptor_type1.disp_primaries_y[2] & 0xFF00) >> DRM_BIT_OFFSET);
		
		hdmi_dev_write(dev,FC_DRM_PB14_DATA_BYTE,(drmparm->mDescriptor_type1.white_point_x & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB15_DATA_BYTE,(drmparm->mDescriptor_type1.white_point_x & 0xFF00) >> DRM_BIT_OFFSET);
		hdmi_dev_write(dev,FC_DRM_PB16_DATA_BYTE,(drmparm->mDescriptor_type1.white_point_y & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB17_DATA_BYTE,(drmparm->mDescriptor_type1.white_point_y & 0xFF00) >> DRM_BIT_OFFSET);

		hdmi_dev_write(dev,FC_DRM_PB18_DATA_BYTE,(drmparm->mDescriptor_type1.max_disp_mastering_luminance & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB19_DATA_BYTE,(drmparm->mDescriptor_type1.max_disp_mastering_luminance & 0xFF00) >> DRM_BIT_OFFSET);
		
		hdmi_dev_write(dev,FC_DRM_PB20_DATA_BYTE,(drmparm->mDescriptor_type1.min_disp_mastering_luminance & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB21_DATA_BYTE,(drmparm->mDescriptor_type1.min_disp_mastering_luminance & 0xFF00) >> DRM_BIT_OFFSET);

		hdmi_dev_write(dev,FC_DRM_PB22_DATA_BYTE,(drmparm->mDescriptor_type1.max_content_light_level & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB23_DATA_BYTE,(drmparm->mDescriptor_type1.max_content_light_level & 0xFF00) >> DRM_BIT_OFFSET);

		hdmi_dev_write(dev,FC_DRM_PB24_DATA_BYTE,(drmparm->mDescriptor_type1.max_frame_avr_light_level & 0x00FF));
		hdmi_dev_write(dev,FC_DRM_PB25_DATA_BYTE,(drmparm->mDescriptor_type1.max_frame_avr_light_level & 0xFF00) >> DRM_BIT_OFFSET);
	}
}


/**
 * @short  Change the drm packet to sdr mode while keeping the devparam of hdmi device. 
 * return int
 */
int hdmi_clear_drm(struct hdmi_tx_dev *dev)
{
        DRM_Packet_t drmparm;
        hdmi_reset_drmparm(&drmparm);
        if(!test_bit(HDMI_TX_STATUS_SUSPEND_L0, &dev->status)) {
                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                        // This Code is required to CTS HDMI2.0                                                                                 
                        drm_configure(dev,(DRM_Packet_t*)&drmparm);
                        drm_tx_enable(dev);
                        drm_update(dev);
                }
        }
        return 0;
}

/**
 * @short Apply the drmparam of hdmi device to the drm packet.
 * return int
 */
int hdmi_apply_drm(struct hdmi_tx_dev *dev)
{
        if(!test_bit(HDMI_TX_STATUS_SUSPEND_L0, &dev->status)) {
                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                        // This Code is required to CTS HDMI2.0                                                                                 
                        drm_configure(dev,(DRM_Packet_t*)dev->drmParm);
                        drm_tx_enable(dev);
                        drm_update(dev);
                }
        }
        return 0;
}

/**
 * @short Update the drmparam of hdmi device from input data.
 * return int
 */
int hdmi_update_drm_configure(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm)
{
        int ret = -1;
        if(!dev) {
                pr_err("%s hdmi driver is not ready..!!\r\n", __func__);
        } else {
                if(drmparm->mInfoFrame.length == 0)
                {
                        //pr_info("%s remove drm meta data\r\n", __func__);
                        hdmi_reset_drmparm(dev->drmParm);

                        clear_bit(HDMI_TX_HDR_VALID, &dev->status);
                        clear_bit(HDMI_TX_HLG_VALID, &dev->status);
                        wake_up_interruptible(&dev->poll_wq);
                        //pr_info("%s clean poll \r\n",__func__);
                }
                else
                {                        
                        // Initialize Valid bit. 
                        clear_bit(HDMI_TX_HDR_VALID, &dev->status);
                        clear_bit(HDMI_TX_HLG_VALID, &dev->status);

                        //pr_info("%s valid drm meta data\r\n", __func__);
                        memcpy(dev->drmParm, drmparm, sizeof(DRM_Packet_t));
                        if(drm_infoframe_verification(dev, (DRM_Packet_t*)dev->drmParm)) {
                                // HDR
                                if(drmparm->mDescriptor_type1.EOTF < 3) {
                                        set_bit(HDMI_TX_HDR_VALID, &dev->status);
                                }
                                // HLG
                                else if(drmparm->mDescriptor_type1.EOTF < 4) {
                                        set_bit(HDMI_TX_HLG_VALID, &dev->status);
                                }
                                wake_up_interruptible(&dev->poll_wq);
                                //pr_info("%s eotf=%d\r\n", __func__, drmparm->mDescriptor_type1.EOTF);
                        }

                }
                ret = 0;
        }
        return ret;
}



