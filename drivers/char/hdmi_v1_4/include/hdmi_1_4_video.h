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

@note Tab size is 8
****************************************************************************/
#ifndef __TCC_HDMI_VIDEO_H__
#define __TCC_HDMI_VIDEO_H__

typedef enum {
        /** Output mode is DVI */
        DVI = 0,
         /** Output mode is HDMI */
        HDMI
} video_mode_t;

typedef enum {
        /** HDMI output color depth is 8-bit */
        COLOR_DEPTH_8 = 8,
        /** HDMI output color depth is 10-bit */
        COLOR_DEPTH_10 = 10,
        /** HDMI output color depth is 12-bit */
        COLOR_DEPTH_12 = 12,
} color_depth_t;

typedef enum {
        /** Encoding is RGB */
        RGB = 0,
        /** Encoding is YCbCr444 */
        YCC444,
        /** Encoding is YCbCr422 */
        YCC422,
} encoding_t;

typedef enum {
        /** No Data */
        COLORIMETRY_NODATA = 0,
        /** SMPTE 170M */
        ITU601,
        /** ITU-R BT.709 */
        ITU709,
        /** Extended Colorimetry Information Valid */
        EXTENDED_COLORIMETRY
} colorimetry_t;

typedef enum {
        /** xvYCC601 */
        XV_YCC601 = 0,
        /** xvYCC709 */
        XV_YCC709,
        /** sYCC601 */
        S_YCC601,
        /** AdobeYCC601 */
        ADOBE_YCC601,
        /** AdobeRGB */
        ADOBE_RGB,
        /** ITU-R BT.2020 YcCbcCrc */
        BT2020YCCBCR,
        /** ITU-R BT.2020 RGB or YCbCr */
        BT2020YCBCR
} ext_colorimetry_t;

typedef struct {
        /** VIC code */
        unsigned int mCode;

        unsigned short mPixelRepetitionInput;

        /** In units of 1MHz */
        unsigned int mPixelClock;

        /** 1 for interlaced, 0 progressive */
        unsigned char mInterlaced;

        unsigned short mHActive;

        unsigned short mHBlanking;

        unsigned short mHBorder;

        unsigned short mHImageSize;

        unsigned short mHSyncOffset;

        unsigned short mHSyncPulseWidth;

        /** 0 for Active low, 1 active high */
        unsigned char mHSyncPolarity;

        unsigned short mVActive;

        unsigned short mVBlanking;

        unsigned short mVBorder;

        unsigned short mVImageSize;

        unsigned short mVSyncOffset;

        unsigned short mVSyncPulseWidth;

        /** 0 for Active low, 1 active high */
        unsigned char mVSyncPolarity;
} dtd_t;


/**
 * @short This structure defines HDMI Video, AVI infoFrame and HDMI 3D */
typedef struct {
        /** HDMI output mode DVI or HDMI */
        video_mode_t mHdmi;
        /** Color space of output video
          * @note mEncodingOuput and mEncodingIn is should be same color space. */
        encoding_t mEncodingOut;
        /** Color depth of video */
        color_depth_t mColorResolution;
        /** Detailed Timing Information  */
        dtd_t mDtd;
        /** RGB quantization range \n
          * 0: CEA default \n
          * 1: Limited Range \n
          * 2: Full Range */
        unsigned char mRgbQuantizationRange;
        /** YUV quantization range \n
         * 0: Limited Range \n
         * 1: Full Range */
        unsigned char mYuvQuantizationRange;
        /** Colorimetry */
        colorimetry_t mColorimetry;
        /** Scan Information */
        unsigned char mScanInfo;
        /** Active Portion Aspect Ratio \n
         *  8: Same as Picture Aspect Ratio \n
         *  9: 4:3 (Center) \n
         * 10: 16:9 (Center) */
        unsigned char mActiveFormatAspectRatio;
        /** Sets Non-Uniform Pictre Scaling */
        unsigned char mNonUniformScaling;
        /** Extended colorimetry  */
        ext_colorimetry_t mExtColorimetry;
        /** Indicate MD0 bit of 'Colorimetry Data Block' */
        unsigned char mColorimetryDataBlock;
        /** IT content
          * [7:4] IT Content Type
          * [3:0] IT Content */
        unsigned char mItContent;
        /** End of Top Bar */
        unsigned short mEndTopBar;
        /** Start of Bottom Bar */
        unsigned short mStartBottomBar;
        /** End of Left Bar */
        unsigned short mEndLeftBar;
        /** Start Right Bar */
        unsigned short mStartRightBar;
        /** HDMI Video Format */
        unsigned char mHdmiVideoFormat;
        /** 3D_Structure */
        unsigned char m3dStructure;
        /** 3D_Ext_Data */
        unsigned char m3dExtData;
} videoParams_t;
/** @} */

#endif /* __TCC_HDMI_VIDEO_H__ */

