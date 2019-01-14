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
