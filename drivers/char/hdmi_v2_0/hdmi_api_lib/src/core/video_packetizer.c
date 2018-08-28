/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        extenddisplay.cpp
*  \brief       HDMI TX controller driver
*  \details   
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not 
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source 
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular 
purpose or non-infringement of any patent, copyright or other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source code or the use in the 
source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure 
Agreement between Telechips and Company.
*/
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <core/video_packetizer.h>
#include <hdmi_api_lib/src/core/video/video_packetizer_reg.h>


u8 vp_PixelPackingPhase(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	return (u8)(hdmi_dev_read(dev, VP_STATUS) & 0xF);
}

void vp_ColorDepth(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	/* color depth */
	hdmi_dev_write_mask(dev, VP_PR_CD, VP_PR_CD_COLOR_DEPTH_MASK, value);
}

void vp_PixelPackingDefaultPhase(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, VP_STUFF, VP_STUFF_IDEFAULT_PHASE_MASK, bit);
}

void vp_PixelRepetitionFactor(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	/* desired factor */
	hdmi_dev_write_mask(dev, VP_PR_CD, VP_PR_CD_DESIRED_PR_FACTOR_MASK, value);
	/* enable stuffing */
	hdmi_dev_write_mask(dev, VP_STUFF, VP_STUFF_PR_STUFFING_MASK, 1);
	/* enable block */
	hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_PR_EN_MASK, (value > 1) ? 1 : 0);
	/* bypass block */
	hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_BYPASS_SELECT_MASK, (value > 1) ? 0 : 1);
}

void vp_Ycc422RemapSize(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, VP_REMAP, VP_REMAP_YCC422_SIZE_MASK, value);
}

void vp_OutputSelector(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	if (value == 0) {	/* pixel packing */
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_BYPASS_EN_MASK, 0);
		/* enable pixel packing */
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_PP_EN_MASK, 1);
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_YCC422_EN_MASK, 0);
	} else if (value == 1) {	/* YCC422 */
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_BYPASS_EN_MASK, 0);
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_PP_EN_MASK, 0);
		/* enable YCC422 */
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_YCC422_EN_MASK, 1);
	} else if (value == 2 || value == 3) {	/* bypass */
		/* enable bypass */
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_BYPASS_EN_MASK, 1);
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_PP_EN_MASK, 0);
		hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_YCC422_EN_MASK, 0);
	} else {
		LOGGER(SNPS_ERROR,"wrong output option: %d", value);
		return;
	}

	/* YCC422 stuffing */
	hdmi_dev_write_mask(dev, VP_STUFF, VP_STUFF_YCC422_STUFFING_MASK, 1);
	/* pixel packing stuffing */
	hdmi_dev_write_mask(dev, VP_STUFF, VP_STUFF_PP_STUFFING_MASK, 1);

	/* output selector */
	hdmi_dev_write_mask(dev, VP_CONF, VP_CONF_OUTPUT_SELECTOR_MASK, value);
}
