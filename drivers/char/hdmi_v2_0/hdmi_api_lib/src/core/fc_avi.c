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

#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <core/packets.h>
#include <core/fc_packets.h>
#include <core/fc_avi.h>
void fc_RgbYcc(struct hdmi_tx_dev *dev, u8 type)
{
	LOG_TRACE1(type);
	hdmi_dev_write_mask(dev, FC_AVICONF0, FC_AVICONF0_RGC_YCC_INDICATION_MASK, type);
}

void fc_ScanInfo(struct hdmi_tx_dev *dev, u8 left)
{
	LOG_TRACE1(left);
	hdmi_dev_write_mask(dev, FC_AVICONF0, FC_AVICONF0_SCAN_INFORMATION_MASK, left);
}

void fc_Colorimetry(struct hdmi_tx_dev *dev, unsigned cscITU)
{
	LOG_TRACE1(cscITU);
	hdmi_dev_write_mask(dev, FC_AVICONF1, FC_AVICONF1_COLORIMETRY_MASK, cscITU);
}

void fc_PicAspectRatio(struct hdmi_tx_dev *dev, u8 ar)
{
	LOG_TRACE1(ar);
	hdmi_dev_write_mask(dev, FC_AVICONF1, FC_AVICONF1_PICTURE_ASPECT_RATIO_MASK, ar);
}

void fc_ActiveAspectRatioValid(struct hdmi_tx_dev *dev, u8 valid)
{
	LOG_TRACE1(valid);
	hdmi_dev_write_mask(dev, FC_AVICONF0, FC_AVICONF0_ACTIVE_FORMAT_PRESENT_MASK, valid);
}

void fc_ActiveFormatAspectRatio(struct hdmi_tx_dev *dev, u8 left)
{
	LOG_TRACE1(left);
	hdmi_dev_write_mask(dev, FC_AVICONF1, FC_AVICONF1_ACTIVE_ASPECT_RATIO_MASK, left);
}

void fc_IsItContent(struct hdmi_tx_dev *dev, u8 it)
{
	LOG_TRACE1(it);
	hdmi_dev_write_mask(dev, FC_AVICONF2, FC_AVICONF2_IT_CONTENT_MASK, (it ? 1 : 0));
}

void fc_ExtendedColorimetry(struct hdmi_tx_dev *dev, u8 extColor)
{
	LOG_TRACE1(extColor);
	hdmi_dev_write_mask(dev, FC_AVICONF2, FC_AVICONF2_EXTENDED_COLORIMETRY_MASK, extColor);
	hdmi_dev_write_mask(dev, FC_AVICONF1, FC_AVICONF1_COLORIMETRY_MASK, 0x3);
}

void fc_QuantizationRange(struct hdmi_tx_dev *dev, u8 rgb_range, u8 yuv_range)
{
	LOG_TRACE1(range);
        if(rgb_range > 2) {
                /*
                 * 0 : depend on video format
                 * 1 : Limited Range
                 * 2 : Full Range */
                rgb_range = 0;
        }
	hdmi_dev_write_mask(dev, FC_AVICONF2, FC_AVICONF2_QUANTIZATION_RANGE_MASK, rgb_range);
        if(yuv_range > 1) {
                /*
                 * 0 : Limited Range
                 * 1 : Full Range */
                yuv_range = 0;
        }
        hdmi_dev_write_mask(dev, FC_AVICONF3, FC_AVICONF3_YQ_MASK, yuv_range);
}

void fc_NonUniformPicScaling(struct hdmi_tx_dev *dev, u8 scale)
{
	LOG_TRACE1(scale);
	hdmi_dev_write_mask(dev, FC_AVICONF2, FC_AVICONF2_NON_UNIFORM_PICTURE_SCALING_MASK, scale);
}

void fc_VideoCode(struct hdmi_tx_dev *dev, u8 code)
{
	LOG_TRACE1(code);
	hdmi_dev_write(dev, FC_AVIVID, code);
        //hdmi_dev_write_mask(dev, FC_AVIVID, FC_AVIVID_FC_AVIVID_7_MASK, 1);
        //Source Device shall always transmit a version 2 AVI infoFrame.
}

void fc_HorizontalBarsValid(struct hdmi_tx_dev *dev, u8 validity)
{
	hdmi_dev_write_mask(dev, FC_AVICONF0, FC_AVICONF0_BAR_INFORMATION_MASK & 0x8, (validity ? 1 : 0));
}

void fc_HorizontalBars(struct hdmi_tx_dev *dev, u16 endTop, u16 startBottom)
{
	LOG_TRACE2(endTop, startBottom);
	hdmi_dev_write(dev, FC_AVIETB0, (u8) (endTop));
	hdmi_dev_write(dev, FC_AVIETB1, (u8) (endTop >> 8));
	hdmi_dev_write(dev, FC_AVISBB0, (u8) (startBottom));
	hdmi_dev_write(dev, FC_AVISBB1, (u8) (startBottom >> 8));
}

void fc_VerticalBarsValid(struct hdmi_tx_dev *dev, u8 validity)
{
	hdmi_dev_write_mask(dev, FC_AVICONF0, FC_AVICONF0_BAR_INFORMATION_MASK & 0x4, (validity ? 1 : 0));
}

void fc_VerticalBars(struct hdmi_tx_dev *dev, u16 endLeft, u16 startRight)
{
	LOG_TRACE2(endLeft, startRight);
	hdmi_dev_write(dev, FC_AVIELB0, (u8) (endLeft));
	hdmi_dev_write(dev, FC_AVIELB1, (u8) (endLeft >> 8));
	hdmi_dev_write(dev, FC_AVISRB0, (u8) (startRight));
	hdmi_dev_write(dev, FC_AVISRB1, (u8) (startRight >> 8));
}

void fc_OutPixelRepetition(struct hdmi_tx_dev *dev, u8 pr)
{
	LOG_TRACE1(pr);
	hdmi_dev_write_mask(dev, FC_PRCONF, FC_PRCONF_OUTPUT_PR_FACTOR_MASK, pr);
}

u32 fc_GetInfoFrameSatus(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, FC_AVICONF0);
}

int fc_colorimetry_config(struct hdmi_tx_dev *dev, videoParams_t *videoParams)
{
        if (videoParams->mColorimetry == EXTENDED_COLORIMETRY) { /* ext colorimetry valid */
                switch(videoParams->mExtColorimetry) {
                        case XV_YCC601:
                        case XV_YCC709:
                        case S_YCC601:
                        case ADOBE_YCC601:
                        case ADOBE_RGB:
                        case BT2020YCCBCR:
                        case BT2020YCBCR:
                                fc_ExtendedColorimetry(dev, videoParams->mExtColorimetry);
                                fc_Colorimetry(dev, videoParams->mColorimetry); /* EXT-3 */
                                break;
                        default:
                                fc_ExtendedColorimetry(dev, 0);
                                fc_Colorimetry(dev, 0); /* No Data */
                                break;
                }
        } else {
                fc_ExtendedColorimetry(dev, 0);
                fc_Colorimetry(dev, videoParams->mColorimetry); /* NODATA-0/ 601-1/ 709-2/ EXT-3 */
        }
        return 0;
}


void fc_avi_config(struct hdmi_tx_dev *dev, videoParams_t *videoParams)
{
	u16 endTop = 0;
	u16 startBottom = 0;
	u16 endLeft = 0;
	u16 startRight = 0;
	dtd_t *dtd = &videoParams->mDtd;

	LOG_TRACE();

	if (videoParams->mEncodingOut == RGB) {
		LOGGER(SNPS_NOTICE,"%s:rgb", __func__);
		fc_RgbYcc(dev, 0);
	}
	else if (videoParams->mEncodingOut == YCC422) {
                LOGGER(SNPS_NOTICE,"%s:ycc422", __func__);
		#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
                fc_RgbYcc(dev, 0);
		#else
                if((videoParams->mDolbyVision & 0x7) == 2) {
                        fc_RgbYcc(dev, 0);
                }
                else {
                        fc_RgbYcc(dev, 1);
                }
		#endif
	}
	else if (videoParams->mEncodingOut == YCC444) {
		LOGGER(SNPS_NOTICE,"%s:ycc444", __func__);
		fc_RgbYcc(dev, 2);
	}
	else if (videoParams->mEncodingOut == YCC420) {
		LOGGER(SNPS_NOTICE,"%s:ycc420", __func__);
		fc_RgbYcc(dev, 3);
	}

        // Same as Picture Aspect Ratio - Table 10 AVI InfoFrame Data Byte 2 (CEA-861-F)
	fc_ActiveFormatAspectRatio(dev, 0x8);

	LOGGER(SNPS_NOTICE, "%s:infoframe status %x", __func__,
			fc_GetInfoFrameSatus(dev));

	fc_ScanInfo(dev, videoParams->mScanInfo);

	if (dtd->mHImageSize != 0 || dtd->mVImageSize != 0) {
		u8 pic = (dtd->mHImageSize * 10) % dtd->mVImageSize;
		// 16:9 or 4:3
		fc_PicAspectRatio(dev, (pic > 5) ? 2 : 1);
	}
	else {
		// No Data
		fc_PicAspectRatio(dev, 0);
	}

	fc_IsItContent(dev, videoParams->mItContent);

	fc_QuantizationRange(dev, videoParams->mRgbQuantizationRange, videoParams->mYuvQuantizationRange);
	fc_NonUniformPicScaling(dev, videoParams->mNonUniformScaling);
	if (dtd->mCode != (u8) (-1)) {
		if (videoParams->mHdmi20 == 1) {
                        if(videoParams->mHdmiVideoFormat != 2 &&  // 3D
                                (dtd->mCode == 93 || dtd->mCode == 94 || dtd->mCode == 95 || dtd->mCode == 98)) {
                                fc_VideoCode(dev, 0);
                                // hdmi spec..!!
                        }else {
			        fc_VideoCode(dev, dtd->mCode);
                        }
		} else {
			if (dtd->mCode < 90) {
				fc_VideoCode(dev, dtd->mCode);
			} else {
				fc_VideoCode(dev, 0);
			}
		}
	} else {
		fc_VideoCode(dev, 0);
	}

	fc_colorimetry_config(dev, videoParams);

	if (videoParams->mActiveFormatAspectRatio != 0) {
		fc_ActiveFormatAspectRatio(dev, videoParams->mActiveFormatAspectRatio);
		fc_ActiveAspectRatioValid(dev, 1);
	} else {
		fc_ActiveAspectRatioValid(dev, 0);
	}
	if (videoParams->mEndTopBar != (u16) (-1) || videoParams->mStartBottomBar != (u16) (-1)) {
		if (videoParams->mEndTopBar != (u16) (-1)) {
			endTop = videoParams->mEndTopBar;
		}
		if (videoParams->mStartBottomBar != (u16) (-1)) {
			startBottom = videoParams->mStartBottomBar;
		}
		fc_HorizontalBars(dev, endTop, startBottom);
		fc_HorizontalBarsValid(dev, 1);
	} else {
		fc_HorizontalBarsValid(dev, 0);
	}
	if (videoParams->mEndLeftBar != (u16) (-1) || videoParams->mStartRightBar != (u16) (-1)) {
		if (videoParams->mEndLeftBar != (u16) (-1)) {
			endLeft = videoParams->mEndLeftBar;
		}
		if (videoParams->mStartRightBar != (u16) (-1)) {
			startRight = videoParams->mStartRightBar;
		}
		fc_VerticalBars(dev, endLeft, startRight);
		fc_VerticalBarsValid(dev, 1);
	} else {
		fc_VerticalBarsValid(dev, 0);
	}
	fc_OutPixelRepetition(dev, (dtd->mPixelRepetitionInput + 1) * (videoParams->mPixelRepetitionFactor + 1) - 1);
}
