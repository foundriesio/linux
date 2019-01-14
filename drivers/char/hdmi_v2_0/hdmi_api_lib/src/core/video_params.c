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


int videoParams_GetCeaVicCode(int hdmi_vic_code)
{
        int cea_code = -1;
	switch(hdmi_vic_code)
	{
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

	switch(cea_code)
	{
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

void video_params_reset(struct hdmi_tx_dev *dev, videoParams_t * params)
{


	dtd_t dtd = {0};

        if(params != NULL) {
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

#if 0
/*
 * This API is deprecated
 */
u16 *videoParams_GetCscA(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	videoParams_UpdateCscCoefficients(dev, params);
	return params->mCscA;
}

void videoParams_SetCscA(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value[4])
{
	u16 i;
	for (i = 0; i < sizeof(params->mCscA) / sizeof(params->mCscA[0]); i++) {
		params->mCscA[i] = value[i];
	}
}

u16 *videoParams_GetCscB(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	videoParams_UpdateCscCoefficients(dev, params);
	return params->mCscB;
}

void videoParams_SetCscB(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value[4])
{
	u16 i;
	for (i = 0; i < sizeof(params->mCscB) / sizeof(params->mCscB[0]); i++) {
		params->mCscB[i] = value[i];
	}
}

u16 *videoParams_GetCscC(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	videoParams_UpdateCscCoefficients(dev, params);
	return params->mCscC;
}

void videoParams_SetCscC(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value[4])
{
	int i;
	for (i = 0; i < sizeof(params->mCscC) / sizeof(params->mCscC[0]); i++) {
		params->mCscC[i] = value[i];
	}
}

void videoParams_SetCscScale(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value)
{
	params->mCscScale = value;
}
#endif

/* [0.01 MHz] */
u32 videoParams_GetPixelClock(struct hdmi_tx_dev *dev, videoParams_t * params)
{
        u32 pixelclock = 0;

        if(params != NULL) {
                pixelclock = params->mDtd.mPixelClock * 1000;
        	if (params->mHdmiVideoFormat == 2) {
        		if (params->m3dStructure == 0) {
        			pixelclock = (2 * params->mDtd.mPixelClock)*1000;
        		}
        	}

                if(params->mEncodingOut == YCC420) {
                        pixelclock >>= 1;
                }
        }
	return pixelclock;
}

/* 0.01 */
unsigned videoParams_GetRatioClock(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	unsigned ratio = 100;

	if (params->mEncodingOut != YCC422) {
                /*
                 * Mask this condition to remove Redundant Condition
		if (params->mColorResolution == 8) {
			ratio = 100;
		} else */
		if (params->mColorResolution == 10) {
			ratio = 125;
		} else if (params->mColorResolution == 12) {
			ratio = 150;
		} else if (params->mColorResolution == 16) {
			ratio = 200;
		}
	}
	return ratio * (params->mPixelRepetitionFactor + 1);
}

int videoParams_IsColorSpaceConversion(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	return params->mEncodingIn != params->mEncodingOut;
}

int videoParams_IsColorSpaceDecimation(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	return params->mEncodingOut == YCC422 && (params->mEncodingIn == RGB
			|| params->mEncodingIn ==
					YCC444);
}

int videoParams_IsColorSpaceInterpolation(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	return params->mEncodingIn == YCC422 && (params->mEncodingOut == RGB
			|| params->mEncodingOut ==
					YCC444);
}

int videoParams_IsPixelRepetition(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	return (params->mPixelRepetitionFactor > 0) || (params->mDtd.mPixelRepetitionInput > 0);
}
#if 0
/*
 * This API is deprecated
 */
void videoParams_UpdateCscCoefficients(struct hdmi_tx_dev *dev, videoParams_t * params)
{
	u16 i = 0;
	if (!videoParams_IsColorSpaceConversion(dev, params)) {
		for (i = 0; i < 4; i++) {
			params->mCscA[i] = 0;
			params->mCscB[i] = 0;
			params->mCscC[i] = 0;
		}
		params->mCscA[0] = 0x2000;
		params->mCscB[1] = 0x2000;
		params->mCscC[2] = 0x2000;
		params->mCscScale = 1;
	} else if (videoParams_IsColorSpaceConversion(dev, params) && params->mEncodingOut == RGB) {
		if (params->mColorimetry == ITU601) {
			params->mCscA[0] = 0x2000;
			params->mCscA[1] = 0x6926;
			params->mCscA[2] = 0x74fd;
			params->mCscA[3] = 0x010e;

			params->mCscB[0] = 0x2000;
			params->mCscB[1] = 0x2cdd;
			params->mCscB[2] = 0x0000;
			params->mCscB[3] = 0x7e9a;

			params->mCscC[0] = 0x2000;
			params->mCscC[1] = 0x0000;
			params->mCscC[2] = 0x38b4;
			params->mCscC[3] = 0x7e3b;

			params->mCscScale = 1;
		} else if (params->mColorimetry == ITU709) {
			params->mCscA[0] = 0x2000;
			params->mCscA[1] = 0x7106;
			params->mCscA[2] = 0x7a02;
			params->mCscA[3] = 0x00a7;

			params->mCscB[0] = 0x2000;
			params->mCscB[1] = 0x3264;
			params->mCscB[2] = 0x0000;
			params->mCscB[3] = 0x7e6d;

			params->mCscC[0] = 0x2000;
			params->mCscC[1] = 0x0000;
			params->mCscC[2] = 0x3b61;
			params->mCscC[3] = 0x7e25;

			params->mCscScale = 1;
		}
	} else if (videoParams_IsColorSpaceConversion(dev, params) && params->mEncodingIn == RGB) {
		if (params->mColorimetry == ITU601) {
			params->mCscA[0] = 0x2591;
			params->mCscA[1] = 0x1322;
			params->mCscA[2] = 0x074b;
			params->mCscA[3] = 0x0000;

			params->mCscB[0] = 0x6535;
			params->mCscB[1] = 0x2000;
			params->mCscB[2] = 0x7acc;
			params->mCscB[3] = 0x0200;

			params->mCscC[0] = 0x6acd;
			params->mCscC[1] = 0x7534;
			params->mCscC[2] = 0x2000;
			params->mCscC[3] = 0x0200;

			params->mCscScale = 0;
		} else if (params->mColorimetry == ITU709) {
			params->mCscA[0] = 0x2dc5;
			params->mCscA[1] = 0x0d9b;
			params->mCscA[2] = 0x049e;
			params->mCscA[3] = 0x0000;

			params->mCscB[0] = 0x62f0;
			params->mCscB[1] = 0x2000;
			params->mCscB[2] = 0x7d11;
			params->mCscB[3] = 0x0200;

			params->mCscC[0] = 0x6756;
			params->mCscC[1] = 0x78ab;
			params->mCscC[2] = 0x2000;
			params->mCscC[3] = 0x0200;

			params->mCscScale = 0;
		}
	}
	/* else use user coefficients */
}
#endif

/*
void videoParams_SetYcc420Support(hdmi_tx_dev_t *dev, dtd_t * paramsDtd, shortVideoDesc_t * paramsSvd)
{
	paramsDtd->mLimitedToYcc420 = paramsSvd->mLimitedToYcc420;
	paramsDtd->mYcc420 = paramsSvd->mYcc420;
	LOGGER(SNPS_DEBUG,"set ParamsDtd->limite %d", paramsDtd->mLimitedToYcc420);
	LOGGER(SNPS_DEBUG,"set ParamsDtd->supports %d", paramsDtd->mYcc420);
	LOGGER(SNPS_DEBUG,"set Paramssvd->limite %d", paramsSvd->mLimitedToYcc420);
	LOGGER(SNPS_DEBUG,"set ParamsSvd->supports %d", paramsSvd->mYcc420);
}
*/


char * getEncodingString(encoding_t encoding)
{
	switch (encoding){
		case 	RGB: return "RGB";
		case	YCC444: return "YCbCr-444";
		case	YCC422: return "YCbCr-422";
		case	YCC420: return "YCbCr-420";
		default :break;
	}
	return "Undefined";
}
