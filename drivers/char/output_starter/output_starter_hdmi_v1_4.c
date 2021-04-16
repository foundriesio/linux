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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/device.h> // dev_xet_drv_data
#include <video/tcc/gpio.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_fb.h>

// HDMI Header
//#include <hdmi/regs-hdmi.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi_1_4_api.h>

#include <output_starter_hdmi_v1_4.h>


/* FB External APIs */
extern void tccfb_output_starter(char output_type, char lcdc_num, stLTIMING *pstTiming, stLCDCTR *pstCtrl, unsigned int swapbf);
extern void tcc_output_starter_memclr(int img_width, int img_height);
extern void tccfb_output_starter_extra_data(char output_type, struct tcc_fb_extra_data *tcc_fb_extra_data);

int tcc_output_starter_parse_hdmi_dt(struct device_node *np)
{
        // Nothing..
        return 0;
}

int tcc_hdmi_detect_cable(void)
{
        int hotplug_status = 0;
        hotplug_status = hpd_api_get_status();
        return hotplug_status;
}

static void tcc_output_starter_make_packet(videoParams_t *video, packetParam_t *packetParam)
{
        int vendorpayload_length = 0;

        do {
                if(video == NULL || packetParam == NULL) {
                        break;
                }

                // fc_vsdsize max size is 27
                if((video->mDtd.mCode >= 93 && video->mDtd.mCode <= 95) || video->mDtd.mCode == 98) {
			packetParam->vsif.oui = 0x0C03;
                        packetParam->vsif.payload[vendorpayload_length++] = 1 << 5;
                        switch(video->mDtd.mCode) {
                                case 93: // 3840x2160p24Hz
                                        packetParam->vsif.payload[vendorpayload_length++] = 3;
                                        break;
                                case 94: // 3840x2160p25Hz
                                        packetParam->vsif.payload[vendorpayload_length++] = 2;
                                        break;
                                case 95: // 3840x2160p30Hz
                                        packetParam->vsif.payload[vendorpayload_length++] = 1;
                                        break;
                                case 98: // 4096x2160p24Hz
                                        packetParam->vsif.payload[vendorpayload_length++] = 4;
                                        break;
                         }
                }
                packetParam->vsif.payload_length = vendorpayload_length;
        } while(0);
}

static int tcc_output_starter_make_audio(audioParams_t *new_tcc_auido)
{
	int audio_output_mode = 2; /* I2S PCM 2CH */

	/* default setting */
	new_tcc_auido->mInterfaceType = I2S;
	new_tcc_auido->mPacketType = AUDIO_SAMPLE;
	new_tcc_auido->mCodingType = LPCM;
	new_tcc_auido->mChannelAllocation = 2;
	new_tcc_auido->mWordLength = 16;
	new_tcc_auido->mSamplingFrequency = 32000;

	/* default i2s config */
	new_tcc_auido->mI2S_BitPerChannel = 16;
	new_tcc_auido->mI2S_Frame = 64; /* 64FS */
	new_tcc_auido->mI2S_Format = 0; /* I2S Basic */

	switch(audio_output_mode ) {
		case 0:
			/* SPDIF PCM 2CH */
			new_tcc_auido->mInterfaceType = SPDIF;
			new_tcc_auido->mChannelAllocation = 2;
			break;

		case 1:
			/* SPDIF BITSTREAM 2CH */
			new_tcc_auido->mInterfaceType = SPDIF;
			new_tcc_auido->mChannelAllocation = 2;
			break;
		case 2:
			/* I2S PCM 2CH */
			new_tcc_auido->mChannelAllocation = 2;
			break;
		case 3:
			/* I2S LPCM Multi Channel */
			break;
		case 4:
			/* I2S High Bit Rate 8CH */
			new_tcc_auido->mPacketType = HBR_STREAM;
			new_tcc_auido->mCodingType = NLPCM;
			new_tcc_auido->mChannelAllocation = 8;
			break;
		case 5:
			/* I2S I2C DTS, AC3, DDP, AAC Pass-thru up to 5.1Ch */
			new_tcc_auido->mCodingType = NLPCM;
			new_tcc_auido->mChannelAllocation = 2;
			break;
	}

	return 0;
}

int tcc_output_starter_hdmi_v1_4(unsigned int display_device, volatile void __iomem *pRDMA, volatile void __iomem *pDISP)
{
        int ret = 0;
	videoParams_t videoParam;
	packetParam_t packetParam = {
		.spd = {
			.vendor_name = "TCC",
			.vendor_name_length = 3,
			.product_name = "TCC897x",
			.product_name_length = 7,
			.source_information = 1,
		},
	};
	audioParams_t audioParam;

	// Timing Param.
        stLTIMING stHDMI_Timing;
        stLCDCTR stHDMI_Ctrl;

	struct tcc_fb_extra_data tcc_fb_extra_data;
        struct lcdc_timimg_parms_t lcdc_timimg_parms = {0};

        pr_info("[INFO][HDMI] %s \r\n", __func__);

        do {
		/* Initialzie video parameters */
		memset(&videoParam, 0, sizeof(videoParam));

                hdmi_api_dtd_fill(&videoParam.mDtd, HDMI_VIDEO_MODE_VIC, 0);

		videoParam.mHdmi = HDMI;

		/* Color space */
		videoParam.mEncodingOut = HDMI_VIDEO_FORMAT;

		/* Color depth */
		videoParam.mColorResolution = HDMI_VIDEO_DEPTH;

		/* Limited Range */
		videoParam.mRgbQuantizationRange = 1;
		videoParam.mYuvQuantizationRange = 0;

		/* Colorimetry */
		videoParam.mColorimetry = ITU709;

		/* Same as picture aspect ratio */
		videoParam.mActiveFormatAspectRatio = 8;

                /* Mute HDMI Output */

                /* CEA8610F F.3.6 Video Timing Transition (AVMUTE Recommendation) */
                msleep(90);

                //  VIOC Display Device
                VIOC_DISP_TurnOff(pDISP);
                VIOC_RDMA_SetImageDisable(pRDMA);

                hdmi_api_phy_power_down();

                hdmi_api_power_off();
                /* wait for 500ms */
                msleep(100);

                pr_info("[INFO][HDMI] \r\n%s %dx%d\r\n", __func__,
			videoParam.mDtd.mPixelRepetitionInput?(videoParam.mDtd.mHActive>>1):videoParam.mDtd.mHActive,
			videoParam.mDtd.mInterlaced?(videoParam.mDtd.mVActive << 1):(videoParam.mDtd.mVActive));

		hdmi_api_dtd_get_display_param(&videoParam, &lcdc_timimg_parms);

                // Enale AVI MUTE
                tcc_output_starter_memclr(HDMI_IMG_WIDTH, HDMI_IMG_HEIGHT);

                memset(&stHDMI_Timing, 0x00, sizeof(stHDMI_Timing));
                stHDMI_Timing.lpw = lcdc_timimg_parms.lpw;
                stHDMI_Timing.lpc = lcdc_timimg_parms.lpc + 1;
                stHDMI_Timing.lswc = lcdc_timimg_parms.lswc + 1;
                stHDMI_Timing.lewc = lcdc_timimg_parms.lewc + 1;
                stHDMI_Timing.vdb = lcdc_timimg_parms.vdb;
                stHDMI_Timing.vdf = lcdc_timimg_parms.vdf;
                stHDMI_Timing.fpw = lcdc_timimg_parms.fpw;
                stHDMI_Timing.flc = lcdc_timimg_parms.flc;
                stHDMI_Timing.fswc = lcdc_timimg_parms.fswc;
                stHDMI_Timing.fewc = lcdc_timimg_parms.fewc;
                stHDMI_Timing.fpw2 = lcdc_timimg_parms.fpw2;
                stHDMI_Timing.flc2 = lcdc_timimg_parms.flc2;
                stHDMI_Timing.fswc2 = lcdc_timimg_parms.fswc2;
                stHDMI_Timing.fewc2 = lcdc_timimg_parms.fewc2;

                memset(&stHDMI_Ctrl, 0x00, sizeof(stHDMI_Ctrl));
                stHDMI_Ctrl.id = lcdc_timimg_parms.id;
                stHDMI_Ctrl.iv = lcdc_timimg_parms.iv;
                stHDMI_Ctrl.ih = lcdc_timimg_parms.ih;
                stHDMI_Ctrl.ip = lcdc_timimg_parms.ip;
                stHDMI_Ctrl.clen = 0;
                stHDMI_Ctrl.dp = lcdc_timimg_parms.dp;
                stHDMI_Ctrl.ni = lcdc_timimg_parms.ni;
                #if defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
                        if(lcdc_timimg_parms.ni == 0)
                                stHDMI_Ctrl.advi = 0;
                #else
                if(lcdc_timimg_parms.ni == 0)
                        stHDMI_Ctrl.advi = 1;
                #endif

                stHDMI_Ctrl.tv = lcdc_timimg_parms.tv;

                #if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
                stHDMI_Ctrl.y2r = stHDMI_Ctrl.r2y?0:1;
                stHDMI_Ctrl.r2y = 0;
                #endif

                // update video params.
                tccfb_output_starter(TCC_OUTPUT_HDMI, display_device, &stHDMI_Timing, &stHDMI_Ctrl, 0);

                if(display_device)
                        VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_HDMI, VIOC_OUTCFG_DISP1);
                else
                        VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_HDMI, VIOC_OUTCFG_DISP0);

                hdmi_api_power_on();

		hdmi_api_phy_config(videoParam.mDtd.mPixelClock * 1000, videoParam.mColorResolution);

                //tcc_hdmi_set_phy_config(videoParam.mDtd.mPixelClock * 1000, HDMI_VIDEO_DEPTH);

		hdmi_api_dtb_get_extra_data(&videoParam, &tcc_fb_extra_data);
		tccfb_output_starter_extra_data(TCC_OUTPUT_HDMI, &tcc_fb_extra_data);

		hdmi_api_video_config(&videoParam);

		hdmi_api_set_spd_infoframe(&packetParam);
		tcc_output_starter_make_packet(&videoParam, &packetParam);

		hdmi_api_set_vsif_infoframe(&packetParam);


		tcc_output_starter_make_audio(&audioParam);
		hdmi_api_audio_config(&audioParam);

                hdmi_start();

                msleep(90); //  CEA8610F F.3.6 Video Timing Transition (AVMUTE Recommendation)
                // disable av mute
		hdmi_api_av_mute(0);
        }while(0);
        return ret;
}

int tcc_output_starter_hdmi_disable(void)
{
        return 0;
}

