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
#include <include/hdmi_ioctls.h>
#include <include/hdmi_access.h>
#include <include/hdmi_drm.h>
#include <core/interrupt/interrupt_reg.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>

static void hdmi_reset_drmparm(DRM_Packet_t * drmparm)
{
        if(drmparm != NULL) {
                memset(drmparm, 0, sizeof(DRM_Packet_t));
                drmparm->mInfoFrame.version = 1;
                drmparm->mInfoFrame.length = 0x1A; // 26
        }
}

static int drm_infoframe_verification(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm)
{
        int valid = 0;

        do {
                if(drmparm == NULL) {
                        pr_err("%s drmparm is NULL\r\n", __func__);
                        break;
                }
                if(drmparm->mInfoFrame.version != 1) {
                        printk("drm_infoframe_verification: versio is mismatch\r\n");
                        break;
                }
                if(drmparm->mInfoFrame.length < 1) {
                        printk("drm_infoframe_verification: length is mismatch\r\n");
                        break;
                }
                if(drmparm->mDescriptor_type1.EOTF > 3) {
                        printk("drm_infoframe_verification: eotf is mismatch\r\n");
                        break;
                }
                if(drmparm->mDescriptor_type1.Descriptor_ID > 0) {
                        printk("drm_infoframe_verification: id is mismatch\r\n");
                     break;
                }
                valid = 1;
        }while(0);
        return valid;
}

static void drm_tx_enable(struct hdmi_tx_dev *dev)
{
        if(dev != NULL) {
	        hdmi_dev_write_mask(dev,FC_PACKET_TX_EN,FC_PACKET_TX_EN_DRM_TX_EN_MASK,1);
        }
}

static void drm_update(struct hdmi_tx_dev *dev)
{
        if(dev != NULL) {
	        hdmi_dev_write_mask(dev,FC_DRM_UP,FC_DRM_UP_PACKET_UPDATE_MASK,1);
        }
}

static void drm_configure(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm)
{
        if(dev == NULL) {
                pr_err("%s dev is NULL\r\n", __func__);
                return;
        }
        if(drmparm == NULL) {
                pr_err("%s drmparm is NULL\r\n", __func__);
                return;
        }
	hdmi_dev_write(dev,FC_DRM_HB0_DATA_BYTE, drmparm->mInfoFrame.version);
	hdmi_dev_write(dev,FC_DRM_HB1_DATA_BYTE, drmparm->mInfoFrame.length);

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
 * @short This function changes EOTF of drm packet from any HDR mode to SDR mode.
 * @param[in] dev Device context for HDMI driver
 * @return Always 0
 * @retval 0 this function succeeded
 */
int hdmi_clear_drm(struct hdmi_tx_dev *dev)
{
        DRM_Packet_t drmparm;
        hdmi_reset_drmparm(&drmparm);
        if(dev != NULL) {
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                // This Code is required to CTS HDMI2.0
                                drm_configure(dev,(DRM_Packet_t*)&drmparm);
                                drm_tx_enable(dev);
                                drm_update(dev);
                        }
                }
        }
        return 0;
}

/**
 * @short This function applies drm data stored in drmParm of device context to HDMI drm packet.
 * @param[in] dev Device context for HDMI driver
 * @return Always 0
 * @retval 0 this function succeeded
 */
int hdmi_apply_drm(struct hdmi_tx_dev *dev)
{
	int ret = -1;
	do {
		if(dev == NULL) {
                        pr_err("%s dev is NULL\r\n", __func__);
                        break;
                }

		if(dev->drmParm == NULL) {
                        pr_err("%s drmParm is NULL\r\n", __func__);
                        break;
                }

		 if(!dwc_hdmi_is_suspended(dev)) {
			if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
				// This Code is required to CTS HDMI2.0
				drm_configure(dev,(DRM_Packet_t*)dev->drmParm);
				drm_tx_enable(dev);
				drm_update(dev);
			}
		}
		ret = 0;
        }while(0);
        return ret;
}

/**
 * @short This function stores drm data to drmParm of device context
 * @param[in] dev Device context for HDMI driver
 * @param[in] drmparam Drm Data to store
 * @return Return the result of this api
 * @retval 0 this function succeeded
 * @retval -1 otherwise
 */
int hdmi_update_drm_configure(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm)
{
        int ret = -1;
        do {
                if(dev == NULL) {
                        pr_err("%s dev is NULL\r\n", __func__);
                        break;
                }

                if(drmparm == NULL) {
                        pr_err("%s drmparm is NULL\r\n", __func__);
                        break;
                }

		if(dev->drmParm == NULL) {
                        pr_err("%s dev->drmParm is NULL\r\n", __func__);
                        break;
                }

                if(drmparm->mInfoFrame.length == 0) {
                        pr_info("%s remove drm meta data\r\n", __func__);
                        hdmi_reset_drmparm(dev->drmParm);

                        clear_bit(HDMI_TX_HDR_VALID, &dev->status);
                        clear_bit(HDMI_TX_HLG_VALID, &dev->status);
                        wake_up_interruptible(&dev->poll_wq);
                } else {
                        pr_info("%s valid drm meta data\r\n", __func__);
                        memcpy(dev->drmParm, drmparm, sizeof(DRM_Packet_t));
                        if(drm_infoframe_verification(dev, (DRM_Packet_t*)dev->drmParm)) {
                                // HDR 0, 1, 2
                                if(drmparm->mDescriptor_type1.EOTF < 3) {
                                        set_bit(HDMI_TX_HDR_VALID, &dev->status);
					clear_bit(HDMI_TX_HLG_VALID, &dev->status);
                                }
                                // HLG 3
                                else if(drmparm->mDescriptor_type1.EOTF < 4) {
                                        set_bit(HDMI_TX_HLG_VALID, &dev->status);
					clear_bit(HDMI_TX_HDR_VALID, &dev->status);
                                } else {
		                        clear_bit(HDMI_TX_HDR_VALID, &dev->status);
		                        clear_bit(HDMI_TX_HLG_VALID, &dev->status);
				}
                                wake_up_interruptible(&dev->poll_wq);
                                //pr_info("%s eotf=%d\r\n", __func__, drmparm->mDescriptor_type1.EOTF);
                        } else {
                        	clear_bit(HDMI_TX_HDR_VALID, &dev->status);
				clear_bit(HDMI_TX_HLG_VALID, &dev->status);
			}
                }
                ret = 0;
        }while(0);
        return ret;
}



