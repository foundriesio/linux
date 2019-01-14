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
#include <core/video.h>
#include <core/csc.h>
#include <core/fc_video.h>
#include <core/main_controller.h>
#include <core/video_packetizer.h>
#include <core/video_sampler.h>
#include <core/fc_debug.h>
#include <scdc/scdc.h>
#include <include/hdmi_access.h>
#include <scdc/scrambling.h>

int video_Configure(struct hdmi_tx_dev *dev, videoParams_t * video)
{
        LOG_TRACE();
        /* DVI mode does not support pixel repetition */
        if ((video->mHdmi == DVI) && videoParams_IsPixelRepetition(dev, video)) {
                LOGGER(SNPS_ERROR,"DVI mode with pixel repetition: video not transmitted");
                return -1;
        }
        printk(" VIDEO VIC[%d] %s - %dBIT\r\n", video->mDtd.mCode, (video->mEncodingOut==RGB)?"RGB":(video->mEncodingOut==YCC444)?"YCC444":(video->mEncodingOut==YCC422)?"YCC422":(video->mEncodingOut==YCC420)?"YCC420":"UNKNOWN", (video->mColorResolution * 3));

        //fc_force_output(dev, 1);

        if (fc_video_config(dev, video) < 0)
                return -1;
        if (video_VideoPacketizer(dev, video) < 0)
                return -1;
        if (video_ColorSpaceConverter(dev, video) < 0)
                return -1;
        if (video_VideoSampler(dev, video) < 0)
                return -1;
        return 0;
}

int video_ColorSpaceConverter(struct hdmi_tx_dev *dev, videoParams_t * video)
        {
        unsigned interpolation = 0;
        unsigned decimation = 0;
        unsigned color_depth = 0;

        LOG_TRACE();

        if (videoParams_IsColorSpaceInterpolation(dev, video)) {
                if (video->mCscFilter > 1) {
                        LOGGER(SNPS_ERROR,"invalid chroma interpolation filter: %d", video->mCscFilter);
                        return -1;
                }
                interpolation = 1 + video->mCscFilter;
        }
        else if (videoParams_IsColorSpaceDecimation(dev, video)) {
                if (video->mCscFilter > 2) {
                        LOGGER(SNPS_ERROR,"invalid chroma decimation filter: %d", video->mCscFilter);
                        return -1;
                }
                decimation = 1 + video->mCscFilter;
        }

        if ((video->mColorResolution == COLOR_DEPTH_8) || (video->mColorResolution == 0))
                color_depth = 4;
        else if (video->mColorResolution == COLOR_DEPTH_10)
                color_depth = 5;
        else if (video->mColorResolution == COLOR_DEPTH_12)
                color_depth = 6;
        else if (video->mColorResolution == COLOR_DEPTH_16)
                color_depth = 7;
        else {
                LOGGER(SNPS_ERROR,"invalid color depth: %d", video->mColorResolution);
        return -1;
        }

        csc_config(dev, video, interpolation, decimation, color_depth);

        return 0;
}

int video_VideoPacketizer(struct hdmi_tx_dev *dev, videoParams_t * video)
{
        unsigned remap_size;
        unsigned color_depth;
        unsigned output_select = 0;

        LOG_TRACE();
        if ((video->mEncodingOut == RGB) || (video->mEncodingOut == YCC444) || (video->mEncodingOut == YCC420)) {
                remap_size = 0;
                if (video->mColorResolution == 0) {
                        color_depth = 0;
                        output_select = 3;
                }
                else if (video->mColorResolution == COLOR_DEPTH_8) {
                        color_depth = 0;
                        output_select = 3;
                } else if (video->mColorResolution == COLOR_DEPTH_10)
                        color_depth = 5;
                else if (video->mColorResolution == COLOR_DEPTH_12)
                        color_depth = 6;
                else if (video->mColorResolution == COLOR_DEPTH_16)
                        color_depth = 7;
                else {
                        LOGGER(SNPS_ERROR,"invalid color depth: %d", video->mColorResolution);
                        return -1;
                }
        }
        else if (video->mEncodingOut == YCC422) {
                color_depth = 0;
                if ((video->mColorResolution == COLOR_DEPTH_8) || (video->mColorResolution == 0))
                        remap_size = 0;
                else if (video->mColorResolution == COLOR_DEPTH_10)
                        remap_size = 1;
                else if (video->mColorResolution == COLOR_DEPTH_12)
                        remap_size = 2;
                else {
                        LOGGER(SNPS_ERROR,"invalid color remap size: %d", video->mColorResolution);
                        return -1;
                }
                output_select = 1;
        }
        else {
                LOGGER(SNPS_ERROR,"invalid output encoding type: %d", video->mEncodingOut);
                return -1;
        }

        vp_PixelRepetitionFactor(dev, video->mPixelRepetitionFactor);
        vp_ColorDepth(dev, color_depth);
        vp_PixelPackingDefaultPhase(dev, video->mPixelPackingDefaultPhase);
        vp_Ycc422RemapSize(dev, remap_size);
        vp_OutputSelector(dev, output_select);
        return 0;
}

int video_VideoSampler(struct hdmi_tx_dev *dev, videoParams_t * video)
{
	int ret = 0;
	unsigned map_code = 0;

	LOG_TRACE();

        switch(video->mEncodingIn)
        {
                case RGB:
                        if ((video->mColorResolution == COLOR_DEPTH_8) || (video->mColorResolution == 0))
                                map_code = 1;
                        else if (video->mColorResolution == COLOR_DEPTH_10)
                                map_code = 3;
                        else if (video->mColorResolution == COLOR_DEPTH_12)
                                map_code = 5;
                        else if (video->mColorResolution == COLOR_DEPTH_16)
                                map_code = 7;
                        else {
                                LOGGER(SNPS_ERROR,"invalid color depth: %d", video->mColorResolution);
                                ret = -1;
                        }
                        break;

                case YCC422:
                        if (video->mColorResolution == COLOR_DEPTH_12)
                        {
				#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
								map_code = 0x1B;
				#else
                                if((video->mDolbyVision & 0x7) > 0) {
                                        map_code = 0x1B;
                                }
                                else  {
                                        map_code = 0x12;
                                }
				#endif
                        }
                        else if (video->mColorResolution == COLOR_DEPTH_10)
                                map_code = 0x14;
                        else if ((video->mColorResolution == COLOR_DEPTH_8) || (video->mColorResolution == 0))
                                map_code = 0x16;
                        else {
                                LOGGER(SNPS_ERROR,"invalid color remap size: %d", video->mColorResolution);
                                ret = -1;
                        }
                        break;
                case YCC420:
                case YCC444:
                        if ((video->mColorResolution == COLOR_DEPTH_8) || (video->mColorResolution == 0))
                                map_code = 0x9;
                        else if (video->mColorResolution == COLOR_DEPTH_10)
                                map_code = 0xB;
                        else if (video->mColorResolution == COLOR_DEPTH_12)
                                map_code = 0xD;
                        else if (video->mColorResolution == COLOR_DEPTH_16)
                                map_code = 0xF;
                        else {
                                LOGGER(SNPS_ERROR,"invalid color depth: %d", video->mColorResolution);
                                ret = -1;
                        }
                        break;
                default:
                        LOGGER(SNPS_ERROR,"invalid input encoding type: %d", video->mEncodingIn);
                        ret = -1;
                        break;
        }

        if(!ret)
	        video_sampler_config(dev, map_code);

	return ret;
}
