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
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <include/hdmi_ioctls.h>
#include <core/fc_video.h>



void fc_video_hdcp_keepout(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_HDCP_KEEPOUT_MASK, bit);
}

void fc_video_VSyncPolarity(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_VSYNC_IN_POLARITY_MASK, bit);
}

void fc_video_HSyncPolarity(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_HSYNC_IN_POLARITY_MASK, bit);
}

void fc_video_DataEnablePolarity(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_DE_IN_POLARITY_MASK, bit);
}

void fc_video_DviOrHdmi(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	/* 1: HDMI; 0: DVI */
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_DVI_MODEZ_MASK, bit);
}

void fc_video_VBlankOsc(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_R_V_BLANK_IN_OSC_MASK, bit);
}

void fc_video_Interlaced(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_INVIDCONF, FC_INVIDCONF_IN_I_P_MASK, bit);
}

void fc_video_HActive(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 12-bit width */
	hdmi_dev_write(dev, (FC_INHACTIV0), (u8) (value));
	hdmi_dev_write_mask(dev, FC_INHACTIV1, FC_INHACTIV1_H_IN_ACTIV_MASK |
									  FC_INHACTIV1_H_IN_ACTIV_12_MASK, (u8)(value >> 8));
}

void fc_video_HBlank(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 10-bit width */
	hdmi_dev_write(dev, (FC_INHBLANK0), (u8) (value));
	hdmi_dev_write_mask(dev, FC_INHBLANK1, FC_INHBLANK1_H_IN_BLANK_MASK |
									  FC_INHBLANK1_H_IN_BLANK_12_MASK, (u8)(value >> 8));
}

void fc_video_VActive(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 11-bit width */
	hdmi_dev_write(dev, (FC_INVACTIV0), (u8) (value));
	hdmi_dev_write_mask(dev, FC_INVACTIV1, FC_INVACTIV1_V_IN_ACTIV_MASK |
									  FC_INVACTIV1_V_IN_ACTIV_12_11_MASK, (u8)(value >> 8));
}

void fc_video_VBlank(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 8-bit width */
	hdmi_dev_write(dev, (FC_INVBLANK), (u8) (value));
}

void fc_video_HSyncEdgeDelay(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 11-bit width */
	hdmi_dev_write(dev, (FC_HSYNCINDELAY0), (u8) (value));
	hdmi_dev_write_mask(dev, FC_HSYNCINDELAY1, FC_HSYNCINDELAY1_H_IN_DELAY_MASK |
										  FC_HSYNCINDELAY1_H_IN_DELAY_12_MASK, (u8)(value >> 8));
}

void fc_video_HSyncPulseWidth(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 9-bit width */
	hdmi_dev_write(dev, (FC_HSYNCINWIDTH0), (u8) (value));
	hdmi_dev_write_mask(dev, FC_HSYNCINWIDTH1, FC_HSYNCINWIDTH1_H_IN_WIDTH_MASK, (u8)(value >> 8));
}

void fc_video_VSyncEdgeDelay(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	/* 8-bit width */
	hdmi_dev_write(dev, (FC_VSYNCINDELAY), (u8) (value));
}

void fc_video_VSyncPulseWidth(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_VSYNCINWIDTH, FC_VSYNCINWIDTH_V_IN_WIDTH_MASK, (u8)(value));
}

void fc_video_RefreshRate(struct hdmi_tx_dev *dev, u32 value)
{
	LOG_TRACE1(value);
	/* 20-bit width */
	hdmi_dev_write(dev, (FC_INFREQ0), (u8) (value >> 0));
	hdmi_dev_write(dev, (FC_INFREQ1), (u8) (value >> 8));
	hdmi_dev_write_mask(dev, FC_INFREQ2, FC_INFREQ2_INFREQ_MASK, (u8)(value >> 16));
}

void fc_video_ControlPeriodMinDuration(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, (FC_CTRLDUR), value);
}

void fc_video_ExtendedControlPeriodMinDuration(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, (FC_EXCTRLDUR), value);
}

void fc_video_ExtendedControlPeriodMaxSpacing(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, (FC_EXCTRLSPAC), value);
}

void fc_video_PreambleFilter(struct hdmi_tx_dev *dev, u8 value, unsigned channel)
{
	LOG_TRACE1(value);
	if (channel == 0)
		hdmi_dev_write(dev, (FC_CH0PREAM), value);
	else if (channel == 1)
		hdmi_dev_write_mask(dev, FC_CH1PREAM, FC_CH1PREAM_CH1_PREAMBLE_FILTER_MASK, value);
	else if (channel == 2)
		hdmi_dev_write_mask(dev, FC_CH2PREAM, FC_CH2PREAM_CH2_PREAMBLE_FILTER_MASK, value);
	else
		LOGGER(SNPS_ERROR,"invalid channel number: %d", channel);
}

void fc_video_PixelRepetitionInput(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_PRCONF, FC_PRCONF_INCOMING_PR_FACTOR_MASK, value);
}


void fc_video_timing_dump(struct hdmi_tx_dev *dev)
{
        unsigned int reg;
        unsigned int val, val1, val2, val3;

        printk("Timing Dump\r\n");
        reg = hdmi_dev_read(dev, (FC_INVIDCONF));
        printk(" SYNC VSYNC(%s), HSYNC(%s), DE(%s) %smode\r\n", (reg & (1<<6))?"High":"Low",  (reg & (1<<5))?"High":"Low", (reg & (1<<4))?"High":"Low", (reg & (1<<3))?"HDMI":"DVI");

        // HACTIVE
        val = hdmi_dev_read(dev, (FC_INHACTIV0));
        reg = hdmi_dev_read(dev, (FC_INHACTIV1));
        reg &= 0x3F;
        val |= (reg <<  8) ;

        // HSYNC EDGE DELAY
        val1 = hdmi_dev_read(dev, (FC_HSYNCINDELAY0));
        reg = hdmi_dev_read(dev, (FC_HSYNCINDELAY1));
        reg &= (FC_HSYNCINDELAY1_H_IN_DELAY_MASK | FC_HSYNCINDELAY1_H_IN_DELAY_12_MASK);
        val1 |= (reg << 8);

        // HSYNC WIDTH
        val2 = hdmi_dev_read(dev, (FC_HSYNCINWIDTH0));
        reg = hdmi_dev_read(dev, (FC_HSYNCINWIDTH1));
        reg &= (FC_HSYNCINWIDTH1_H_IN_WIDTH_MASK);
        val2 |= (reg << 8);

        // BLANK
        val3 = hdmi_dev_read(dev, (FC_INHBLANK0));
        reg = hdmi_dev_read(dev, (FC_INHBLANK1));
        reg &= (FC_INHBLANK1_H_IN_BLANK_MASK | FC_INHBLANK1_H_IN_BLANK_12_MASK);
        val3 |= (reg << 8);

        printk("HSYNC ACTIVE(%d), DELAY(%d), SYNC(%d), BLANK(%d)\r\n", val, val1, val2, val3);

        // VACTIVE
        val = hdmi_dev_read(dev, (FC_INVACTIV0));
        reg = hdmi_dev_read(dev, (FC_INVACTIV1));
        reg &= 0x3F;
        val |= (reg <<  8) ;

        // VSYNC EDGE DELAY
        val1 = hdmi_dev_read(dev, (FC_VSYNCINDELAY));

        // VSYNC WIDTH
        val2 = hdmi_dev_read(dev, (FC_VSYNCINWIDTH));
        val2 &= FC_VSYNCINWIDTH_V_IN_WIDTH_MASK;

        // BLANK
        val3 = hdmi_dev_read(dev, (FC_INVBLANK));

        printk("VSYNC ACTIVE(%d), DELAY(%d), SYNC(%d), BLANK(%d)\r\n", val, val1, val2, val3);
}

int fc_video_config_timing(struct hdmi_tx_dev *dev, videoParams_t *video)
{
        const dtd_t *dtd;

        LOG_TRACE();

        if((dev == NULL) || (video == NULL)){
                pr_err("%s Invalid arguments: dev=%p; video=%p", __func__, dev, video);
                return FALSE;
        }

        dtd = &video->mDtd;

        fc_video_VSyncPolarity(dev, dtd->mVSyncPolarity);
        fc_video_HSyncPolarity(dev, dtd->mHSyncPolarity);
        fc_video_DataEnablePolarity(dev, dev->hdmi_tx_ctrl.data_enable_polarity);
        fc_video_DviOrHdmi(dev, video->mHdmi);

        if (video->mHdmiVideoFormat == 2) {
                if (video->m3dStructure == 0) {     /* 3d data frame packing is transmitted as a progressive format */
                        fc_video_VBlankOsc(dev, 0);
                        fc_video_Interlaced(dev, 0);

                        if (dtd->mInterlaced) {
                                fc_video_VActive(dev, (dtd->mVActive << 2) + 3 * dtd->mVBlanking + 2);
                        }
                        else {
                                fc_video_VActive(dev, (dtd->mVActive << 1) + dtd->mVBlanking);
                        }
                }
                else {
                        fc_video_VBlankOsc(dev, dtd->mInterlaced);
                        fc_video_Interlaced(dev, dtd->mInterlaced);
                        fc_video_VActive(dev, dtd->mVActive);
                }
        }
        else {
                fc_video_VBlankOsc(dev, dtd->mInterlaced);
                fc_video_Interlaced(dev, dtd->mInterlaced);
                fc_video_VActive(dev, dtd->mVActive);
        }

        if(video->mEncodingOut == YCC420){
                LOGGER(SNPS_WARN, "Encoding configured to YCC 420\r\n");
                fc_video_HActive(dev, dtd->mHActive/2);
                fc_video_HBlank(dev, dtd->mHBlanking/2);
                fc_video_HSyncPulseWidth(dev, dtd->mHSyncPulseWidth/2);
                fc_video_HSyncEdgeDelay(dev, dtd->mHSyncOffset/2);
        } else {
        fc_video_HActive(dev, dtd->mHActive);
        fc_video_HBlank(dev, dtd->mHBlanking);
                fc_video_HSyncPulseWidth(dev, dtd->mHSyncPulseWidth);
                fc_video_HSyncEdgeDelay(dev, dtd->mHSyncOffset);
        }

        fc_video_VBlank(dev, dtd->mVBlanking);
        fc_video_VSyncEdgeDelay(dev, dtd->mVSyncOffset);
        fc_video_VSyncPulseWidth(dev, dtd->mVSyncPulseWidth);

        fc_video_PixelRepetitionInput(dev, dtd->mPixelRepetitionInput + 1);

        return TRUE;
}


int fc_video_config_default(struct hdmi_tx_dev *dev)
{

        u16 i;
        fc_video_ControlPeriodMinDuration(dev, 12);
        fc_video_ExtendedControlPeriodMinDuration(dev, 32);

        /* spacing < 256^2 * config / tmdsClock, spacing <= 50ms
         * worst case: tmdsClock == 25MHz => config <= 19
         */
        fc_video_ExtendedControlPeriodMaxSpacing(dev, 1);

        for (i = 0; i < 3; i++)
                fc_video_PreambleFilter(dev, (i + 1) * 11, i);
        return TRUE;
}

int fc_video_config(struct hdmi_tx_dev *dev, videoParams_t *video)
{
        if((dev == NULL) || (video == NULL)){
                pr_err("%s Invalid arguments: dev=%p; video=%p", __func__, dev, video);
                return FALSE;
        }

        fc_video_config_timing(dev, video);

        fc_video_config_default(dev);

        return TRUE;
}


