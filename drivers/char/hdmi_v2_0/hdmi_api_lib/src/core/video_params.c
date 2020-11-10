// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare HDMI driver
 */
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <include/hdmi_ioctls.h>
#include <include/video_params.h>

int videoParams_GetCeaVicCode(int hdmi_vic_code)
{
	int cea_code = -1;

	switch (hdmi_vic_code) {
	case 1:
		cea_code = 95;
		break;
	case 2:
		cea_code = 94;
		break;
	case 3:
		cea_code = 93;
		break;
	case 4:
		cea_code = 98;
		break;
	}
	return cea_code;
}

int videoParams_GetHdmiVicCode(int cea_code)
{

	int vic_code = -1;

	switch (cea_code) {
	case 95:
		vic_code = 1;
		break;
	case 94:
		vic_code = 2;
		break;
	case 93:
		vic_code = 3;
		break;
	case 98:
		vic_code = 4;
		break;
	}
	return vic_code;
}

void video_params_reset(struct hdmi_tx_dev *dev, videoParams_t *params)
{
	dtd_t dtd = {0};

	if (params != NULL) {
		hdmi_dtd_fill(&dtd, 1, 60000);
		memset(params, 0, sizeof(videoParams_t));

		params->mColorResolution = COLOR_DEPTH_8;
		params->mActiveFormatAspectRatio = 8;
		params->mEndTopBar = (u16)-1;
		params->mStartBottomBar = (u16)-1;
		params->mEndLeftBar = (u16)-1;
		params->mStartRightBar = (u16)-1;

		memcpy(&params->mDtd, &dtd, sizeof(dtd_t));
	}
}

/* [0.01 MHz] */
u32 videoParams_GetPixelClock(struct hdmi_tx_dev *dev, videoParams_t *params)
{
	u32 pixelclock = 0;

	if (params != NULL) {
		pixelclock = params->mDtd.mPixelClock * 1000;
		if (params->mHdmiVideoFormat == 2) {
			if (params->m3dStructure == 0)
				pixelclock =
					(2 * params->mDtd.mPixelClock)*1000;
		}
		if (params->mEncodingOut == YCC420)
			pixelclock >>= 1;
	}
	return pixelclock;
}

/* 0.01 */
unsigned int videoParams_GetRatioClock(
	struct hdmi_tx_dev *dev, videoParams_t *params)
{
	unsigned int ratio = 100;

	if (params->mEncodingOut != YCC422) {
		/*
		 * Mask this condition to remove Redundant Condition
		 * if (params->mColorResolution == 8) {
		 *	ratio = 100;
		 * } else
		 */
		if (params->mColorResolution == 10)
			ratio = 125;
		else if (params->mColorResolution == 12)
			ratio = 150;
		else if (params->mColorResolution == 16)
			ratio = 200;
	}
	return ratio * (params->mPixelRepetitionFactor + 1);
}

int videoParams_IsColorSpaceConversion(
	struct hdmi_tx_dev *dev, videoParams_t *params)
{
	return params->mEncodingIn != params->mEncodingOut;
}

int videoParams_IsColorSpaceDecimation(
	struct hdmi_tx_dev *dev, videoParams_t *params)
{
	return params->mEncodingOut == YCC422 &&
		(params->mEncodingIn == RGB || params->mEncodingIn == YCC444);
}

int videoParams_IsColorSpaceInterpolation(
	struct hdmi_tx_dev *dev, videoParams_t *params)
{
	return params->mEncodingIn == YCC422 &&
		(params->mEncodingOut == RGB || params->mEncodingOut == YCC444);
}

int videoParams_IsPixelRepetition(
	struct hdmi_tx_dev *dev, videoParams_t *params)
{
	return (params->mPixelRepetitionFactor > 0) ||
			(params->mDtd.mPixelRepetitionInput > 0);
}

char *getEncodingString(encoding_t encoding)
{
	switch (encoding) {
	case RGB:
		return "RGB";
	case YCC444:
		return "YCbCr-444";
	case YCC422:
		return "YCbCr-422";
	case YCC420:
		return "YCbCr-420";
	default:
		break;
	}
	return "Undefined";
}
