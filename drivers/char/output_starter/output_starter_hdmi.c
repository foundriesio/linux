/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        output_starter_hdmi.c
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
#include <include/hdmi_includes.h>
#include <include/hdmi_ioctls.h>
#include <hdmi_api_lib/include/core/main_controller.h>
#include <hdmi_api_lib/include/core/irq.h>
#include <hdmi_api_lib/include/core/fc_video.h>
#include <hdmi_api_lib/include/core/product.h>
#include <include/hdmi_access.h>
#include <hdmi_api_lib/include/core/fc_debug.h>
#include <hdmi_api_lib/include/core/audio.h>
#include <hdmi_api_lib/include/api/api.h>
#include <include/video_params.h>
#include <hdmi_api_lib/include/scdc/scdc.h>
#include <hdmi_api_lib/include/scdc/scrambling.h>
#include <hdmi_api_lib/include/phy/phy.h>

#include <output_starter_hdmi.h>
#include <output_starter_hdmi_edid.h>


// HDMI_V2_0 API
extern struct hdmi_tx_dev *dwc_hdmi_api_get_dev(void);
extern int dwc_hdmi_get_video_dtd(dtd_t *hdmi_dtd, uint32_t code, uint32_t hz);

// HDMI_V2 MISC
extern void dwc_hdmi_power_on(struct hdmi_tx_dev *dev);
extern void dwc_hdmi_power_off(struct hdmi_tx_dev *dev);
extern void dwc_hdmi_hw_reset(struct hdmi_tx_dev *dev, int reset_on);

extern encoding_t hdmi_get_current_output_encoding(struct hdmi_tx_dev *dev);
extern color_depth_t hdmi_get_current_output_depth(struct hdmi_tx_dev *dev, encoding_t encoding);
extern video_mode_t hdmi_get_current_output_mode(struct hdmi_tx_dev *dev);
extern unsigned int hdmi_get_current_output_vic(struct hdmi_tx_dev *dev);

// Output Start API
extern void tcc_output_starter_memclr(int img_width, int img_height);
extern void tccfb_output_starter(char output_type, char lcdc_num, stLTIMING *pstTiming, stLCDCTR *pstCtrl, int specific_pclk);
extern void tccfb_output_starter_extra_data(char output_type, struct tcc_fb_extra_data *tcc_fb_extra_data);

// STATIC API----------------------------------------------------------------------------------
int tcc_output_starter_parse_hdmi_dt(struct device_node *np)
{
        // Nothing..
        return 0;
}

static void hdmi_product_params_reset(productParams_t *product)
{
        if(product != NULL) {
                memset(product, 0, sizeof(productParams_t));
                product->mSourceType = (u8) (-1);
                product->mOUI = (u8) (-1);
        }
}

static void hdmi_api_make_packet(videoParams_t *video, productParams_t *product)
{
        int vendorpayload_length = 0;

        do {
                if(video == NULL || product == NULL) {
                        break;
                }
                hdmi_product_params_reset(product);

                // fc_vsdsize max size is 27
                if((video->mDtd.mCode >= 93 && video->mDtd.mCode <= 95) || video->mDtd.mCode == 98) {
                        product->mOUI = 0x00030C00;
                        product->mVendorPayload[vendorpayload_length++] = 1 << 5;
                        switch(video->mDtd.mCode)
                        {
                                case 93: // 3840x2160p24Hz
                                        product->mVendorPayload[vendorpayload_length++] = 3;
                                        break;
                                case 94: // 3840x2160p25Hz
                                        product->mVendorPayload[vendorpayload_length++] = 2;
                                        break;
                                case 95: // 3840x2160p30Hz
                                        product->mVendorPayload[vendorpayload_length++] = 1;
                                        break;
                                case 98: // 4096x2160p24Hz
                                        product->mVendorPayload[vendorpayload_length++] = 4;
                                        break;
                         }
                }
                product->mVendorPayloadLength = vendorpayload_length;

                /* Support DolbyVision  */
                if(video->mDolbyVision & (2 << 3)) {
                        /* vendor specific infoframe length must 24 on dolbyvision, refre to dolbyvision 6.1.1 */
                        product->mVendorPayloadLength = 21;
                }
                /* Do not send Vendor Specific InfoFrmae except HDMI_VIC, HDMI_3D or DolbyVision */
                if(product->mVendorPayloadLength == 0) {
                        memset(product->mVendorPayload, 0, sizeof(product->mVendorPayload));
                }
        } while(0);
}


static void tcc_output_starter_make_extra_data(videoParams_t *videoParam) {
        struct tcc_fb_extra_data tcc_fb_extra_data = {0};

        // check file
        if (videoParam == NULL) {
                printk("%s videoParam is NULL\r\n", __func__);
                return ;
        }

        switch(videoParam->mEncodingOut)
        {
                case YCC444:
                        // YCC444
                        if(videoParam->mColorResolution > COLOR_DEPTH_8) {
                                tcc_fb_extra_data.pxdw = 23; // RGB101010 Format
                        }
                        else {
                                tcc_fb_extra_data.pxdw = 12;
                        }
                        tcc_fb_extra_data.swapbf = 2;
                        break;
                case YCC422:
                        // YCC422
                        tcc_fb_extra_data.pxdw = 21;
                        tcc_fb_extra_data.swapbf = 5;
                        break;
                case YCC420:
                        // YCC420
                        if(videoParam->mColorResolution > COLOR_DEPTH_8) {
                                tcc_fb_extra_data.pxdw = 27;
                        }
                        else {
                                tcc_fb_extra_data.pxdw = 26;
                        }
                        break;
                case RGB:
                default:
                        if(videoParam->mColorResolution > COLOR_DEPTH_8) {
                                tcc_fb_extra_data.pxdw = 23; // RGB101010 Format
                        }
                        else {
                                tcc_fb_extra_data.pxdw = 12;
                        }
                        break;
        }

        // R2Y
        if(videoParam->mEncodingOut != RGB) {
                tcc_fb_extra_data.r2y = 1;
                switch(videoParam->mColorimetry)
                {
                                case ITU601:
                                default:
                                        tcc_fb_extra_data.r2ymd = 1;
                                        break;
                                case ITU709:
                                        tcc_fb_extra_data.r2ymd = 3;
                                        break;
                                case EXTENDED_COLORIMETRY:
                                        switch(videoParam->mExtColorimetry) {
                                                case XV_YCC601:
                                                case S_YCC601:
                                                case ADOBE_YCC601:
                                                default:
                                                        tcc_fb_extra_data.r2ymd = 1;
                                                        break;
                                                case XV_YCC709:
                                                        tcc_fb_extra_data.r2ymd = 3;
                                                        break;
                                                case BT2020YCCBCR:
                                                case BT2020YCBCR:
                                                        tcc_fb_extra_data.r2ymd = 5;
                                                        break;
                                        }
                                        break;
                }
        }

        tccfb_output_starter_extra_data(TCC_OUTPUT_HDMI, &tcc_fb_extra_data);
}


static void tcc_output_starter_hdmi_start(unsigned int display_device)
{
        videoParams_t *videoParam;
        audioParams_t *audioParam;
        productParams_t *productParam;
        struct hdmi_tx_dev *hdmi_dev = dwc_hdmi_api_get_dev();

        do {
                if(hdmi_dev == NULL) {
                        break;
                }

                videoParam = (videoParams_t*)hdmi_dev->videoParam;
                audioParam = (audioParams_t*)hdmi_dev->audioParam;
                productParam = (productParams_t*)hdmi_dev->productParam;

                // Reset Unused Parameters
                product_reset(hdmi_dev, productParam);
                audio_reset(hdmi_dev, audioParam);

                hdmi_api_make_packet(videoParam, productParam);
                if(hdmi_dev->hdmi_tx_ctrl.pixel_clock >= 340000000) {
                        videoParam->mScrambling = 1;
                }
                else {
                        videoParam->mScrambling = 0;
                }
                if(edid_is_sink_vizio() == 1) {
                        hdmi_dev->hdmi_tx_ctrl.sink_is_vizio = 1;
                } else {
                        hdmi_dev->hdmi_tx_ctrl.sink_is_vizio = 0;
                }
                memcpy(hdmi_dev->videoParam, videoParam, sizeof(videoParams_t));
                memcpy(hdmi_dev->audioParam, audioParam, sizeof(audioParams_t));
                memcpy(hdmi_dev->productParam, productParam, sizeof(productParams_t));
                hdmi_api_Configure(hdmi_dev);
        }while(0);
}

static unsigned int tcc_output_starter_hdmi_get_vactive(videoParams_t *video)
{
        unsigned int vactive = 0;

        do {
                if(video == NULL) {
                        break;
                }
                vactive = video->mDtd.mVActive;
                if(video->mHdmiVideoFormat == 2 && video->m3dStructure == 0) {
                        if(video->mDtd.mInterlaced) {
                                vactive = (vactive << 2) + (3*video->mDtd.mVBlanking+2);
                        } else {
                                vactive = (vactive << 1) + video->mDtd.mVBlanking;
                        }
                } else if(video->mDtd.mInterlaced) {
                        vactive <<= 1;
                }
        } while(0);
        return vactive;
}

int tcc_output_starter_hdmi_v2_0(unsigned int display_device, volatile void __iomem *pRDMA, volatile void __iomem *pDISP)
{
        int ret = 0;
        uint16_t temp_val;

        // Timing Param.
        stLTIMING stHDMI_Timing;
        stLCDCTR stHDMI_Ctrl;

        int hdmi_interlaced_mode;
        unsigned int vactive, displaydevice_width, displaydevice_height;

        struct hdmi_tx_dev *hdmi_dev;
        struct lcdc_timimg_parms_t lcdc_timimg_parms = {0};
        videoParams_t video_param = {0};
        videoParams_t *dev_video_param;

        // for YCC420
        uint16_t mHActive, mHBlanking, mHSyncOffset, mHSyncPulseWidth;

        hdmi_dev = dwc_hdmi_api_get_dev();

        do {
                if(hdmi_dev == NULL) {
                        pr_info("%s hdmi device is NULL\r\n", __func__);
                        break;
                }
                // Initialize Video Param
                video_params_reset(hdmi_dev, &video_param);

                /* Read Edid */
                video_param.mDtd.mCode = HDMI_VIDEO_MODE_VIC;
                edid_get_optimal_settings(hdmi_dev, &video_param.mHdmi, &video_param.mDtd.mCode, &video_param.mEncodingOut, 1) ;
                video_param.mColorResolution = COLOR_DEPTH_8;
                video_param.mEncodingIn = video_param.mEncodingOut;
                video_param.mHdmi20 = edid_get_hdmi20();

                /* Update DTD */
                if(dwc_hdmi_get_video_dtd(&video_param.mDtd, video_param.mDtd.mCode, 0) < 0) {
                        printk("Force 1280x720@60p\r\n");
                        dwc_hdmi_get_video_dtd(&video_param.mDtd, 4, 60000); // FORCE 720P
                }

                /* Update Pixel Clock */
                hdmi_dev->hdmi_tx_ctrl.pixel_clock = hdmi_phy_get_actual_tmds_bit_ratio_by_videoparam(hdmi_dev, &video_param);
                //pr_info("%s previous pixel_clock is %dHz\r\n", __func__, hdmi_dev->hdmi_tx_ctrl.pixel_clock);

                /* Get bootloader settings */
                dev_video_param = (videoParams_t *)hdmi_dev->videoParam;
                dev_video_param->mHdmi = hdmi_get_current_output_mode(hdmi_dev);
                dev_video_param->mDtd.mCode = hdmi_get_current_output_vic(hdmi_dev);
                dev_video_param->mEncodingIn = dev_video_param->mEncodingOut = hdmi_get_current_output_encoding(hdmi_dev);
                dev_video_param->mColorResolution = hdmi_get_current_output_depth(hdmi_dev, dev_video_param->mEncodingOut);

                if(dev_video_param->mHdmi == video_param.mHdmi &&
                   dev_video_param->mDtd.mCode == video_param.mDtd.mCode &&
                   dev_video_param->mEncodingOut == video_param.mEncodingOut &&
                   video_param.mColorResolution == dev_video_param->mColorResolution) {
                        tccfb_output_starter(TCC_OUTPUT_HDMI, display_device, NULL, NULL, 0);
                } else {
                        edid_get_optimal_settings(hdmi_dev, &video_param.mHdmi, &video_param.mDtd.mCode, &video_param.mEncodingOut, 0) ;
                        video_param.mColorResolution = COLOR_DEPTH_8;
                        video_param.mEncodingIn = video_param.mEncodingOut;
                        video_param.mHdmi20 = edid_get_hdmi20();

                        /* Update DTD */
                        if(dwc_hdmi_get_video_dtd(&video_param.mDtd, video_param.mDtd.mCode, 0) < 0) {
                                printk("Force 1280x720@60p\r\n");
                                dwc_hdmi_get_video_dtd(&video_param.mDtd, 4, 60000); // FORCE 720P
                        }

                        /* Update Pixel Clock */
                        hdmi_dev->hdmi_tx_ctrl.pixel_clock = hdmi_phy_get_actual_tmds_bit_ratio_by_videoparam(hdmi_dev, &video_param);
                        //pr_info("%s previous pixel_clock is %dHz\r\n", __func__, hdmi_dev->hdmi_tx_ctrl.pixel_clock);

                        pr_info("HDMI MODE DIFF \r\n"
                                "hdmi mode     : %s > %s \r\n"
                                "hdmi vic      : %d > %d \r\n"
                                "hdmi encoding : %s > %s \r\n"
                                "hdmi depth    : %d > %d \r\n"
                                , dev_video_param->mHdmi?"HDMI":"DVI", video_param.mHdmi?"HDMI":"DVI",
                                dev_video_param->mDtd.mCode, video_param.mDtd.mCode,
                                dev_video_param->mEncodingOut==RGB?"RGB":dev_video_param->mEncodingOut==YCC444?"YCC444":dev_video_param->mEncodingOut==YCC422?"YCC422":"YCC420",
                                video_param.mEncodingOut==RGB?"RGB":video_param.mEncodingOut==YCC444?"YCC444":video_param.mEncodingOut==YCC422?"YCC422":"YCC420",
                                dev_video_param->mColorResolution, video_param.mColorResolution);

                        /* Mute HDMI Output */
                        hdmi_api_avmute(hdmi_dev, 1);

                        /* CEA8610F F.3.6 Video Timing Transition (AVMUTE Recommendation) */
                        msleep(90);

                        mc_disable_all_clocks(hdmi_dev);
                        clear_bit(HDMI_TX_STATUS_OUTPUT_ON, &hdmi_dev->status);

                        //  VIOC Display Device
                        VIOC_DISP_TurnOff(pDISP);
                        VIOC_RDMA_SetImageDisable(pRDMA);

                        dwc_hdmi_power_off(hdmi_dev);

                        msleep(500);

                        displaydevice_width = video_param.mDtd.mPixelRepetitionInput?(video_param.mDtd.mHActive>>1):video_param.mDtd.mHActive;
                        displaydevice_height = video_param.mDtd.mInterlaced?(video_param.mDtd.mVActive << 1):(video_param.mDtd.mVActive);

                        printk("\r\ntcc_output_starter_hdmi_v2_0 %dx%d\r\n", displaydevice_width, displaydevice_height);

                        mHActive = video_param.mDtd.mHActive;
                        mHBlanking = video_param.mDtd.mHBlanking;
                        mHSyncOffset = video_param.mDtd.mHSyncOffset;
                        mHSyncPulseWidth = video_param.mDtd.mHSyncPulseWidth;

                        // Timing
                        memset(&lcdc_timimg_parms, 0, sizeof(lcdc_timimg_parms));
                        lcdc_timimg_parms.iv = video_param.mDtd.mVSyncPolarity?0:1;  /** 0 for Active low, 1 active high */
                        lcdc_timimg_parms.ih = video_param.mDtd.mHSyncPolarity?0:1;  /** 0 for Active low, 1 active high */
                        lcdc_timimg_parms.dp = video_param.mDtd.mPixelRepetitionInput;

                        vactive = tcc_output_starter_hdmi_get_vactive(&video_param);
                        /* 3d data frame packing is transmitted as a progressive format */
                        hdmi_interlaced_mode = (video_param.mHdmiVideoFormat == 2 && video_param.m3dStructure == 0)?0:video_param.mDtd.mInterlaced;

                        if(hdmi_interlaced_mode)
                                lcdc_timimg_parms.tv = 1;
                        else
                                lcdc_timimg_parms.ni = 1;

                        lcdc_timimg_parms.tft = 0;
                        lcdc_timimg_parms.lpw = (mHSyncPulseWidth>0)?(mHSyncPulseWidth-1):0;
                        lcdc_timimg_parms.lpc = (mHActive>0)?(mHActive-1):0;
                        temp_val = (mHBlanking - (mHSyncOffset+mHSyncPulseWidth));
                        lcdc_timimg_parms.lswc = (temp_val>0)?temp_val-1:0;
                        lcdc_timimg_parms.lewc = (mHSyncOffset>0)?(mHSyncOffset-1):0;

                        if(hdmi_interlaced_mode){
                                temp_val = video_param.mDtd.mVSyncPulseWidth << 1;
                                lcdc_timimg_parms.fpw = (temp_val>0)?(temp_val-1):0;
                                temp_val = ((video_param.mDtd.mVBlanking - (video_param.mDtd.mVSyncOffset + video_param.mDtd.mVSyncPulseWidth)) << 1);
                                lcdc_timimg_parms.fswc = (temp_val>0)?(temp_val-1):0;
                                lcdc_timimg_parms.fewc = (video_param.mDtd.mVSyncOffset << 1);
                                lcdc_timimg_parms.fswc2 = lcdc_timimg_parms.fswc+1;
                                lcdc_timimg_parms.fewc2 = (lcdc_timimg_parms.fewc>0)?(lcdc_timimg_parms.fewc-1):0;
                        }
                        else {
                                lcdc_timimg_parms.fpw = (video_param.mDtd.mVSyncPulseWidth>0)?(video_param.mDtd.mVSyncPulseWidth-1):0;
                                temp_val = (video_param.mDtd.mVBlanking - (video_param.mDtd.mVSyncOffset + video_param.mDtd.mVSyncPulseWidth));
                                lcdc_timimg_parms.fswc = (temp_val>0)?(temp_val-1):0;
                                lcdc_timimg_parms.fewc = (video_param.mDtd.mVSyncOffset>0)?(video_param.mDtd.mVSyncOffset-1):0;
                                lcdc_timimg_parms.fswc2 = lcdc_timimg_parms.fswc;
                                lcdc_timimg_parms.fewc2 = lcdc_timimg_parms.fewc;
                        }
                        /* Common Timing Parameters */
                        lcdc_timimg_parms.flc = (vactive>0)?(vactive-1):0;
                        lcdc_timimg_parms.fpw2 = lcdc_timimg_parms.fpw;
                        lcdc_timimg_parms.flc2 = lcdc_timimg_parms.flc;

                        if(display_device) {
                                volatile unsigned long bits;
                                bits = ioread32(hdmi_dev->ddibus_io);
                                set_bit(17, &bits);
                                clear_bit(16, &bits);
                                iowrite32(bits, hdmi_dev->ddibus_io);
                                if(hdmi_dev->verbose >= VERBOSE_IO)
                                        printk("ddibus_io(0x%p) = 0x%x\r\n", hdmi_dev->ddibus_io, ioread32(hdmi_dev->ddibus_io));
                                VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_HDMI, VIOC_OUTCFG_DISP1);
                        }
                        else {
                                volatile unsigned long bits;
                                bits = ioread32(hdmi_dev->ddibus_io);
                                set_bit(16, &bits);
                                clear_bit(17, &bits);
                                iowrite32(bits, hdmi_dev->ddibus_io);
                                if(hdmi_dev->verbose >= VERBOSE_IO)
                                        printk("ddibus_io(0x%p) = 0x%x\r\n", hdmi_dev->ddibus_io, ioread32(hdmi_dev->ddibus_io));
                                VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_HDMI, VIOC_OUTCFG_DISP0);
                        }

                        // Enale AVI MUTE
                        dwc_hdmi_power_on(hdmi_dev);
                        tcc_output_starter_memclr(HDMI_IMG_WIDTH, HDMI_IMG_HEIGHT);

                        // Mapping Display Device for HDMI output.
                        // Must Initialize display_device when HDMI Driver is open.

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
                        #if defined(CONFIG_ARCH_TCC898X)
                        stHDMI_Ctrl.advi = lcdc_timimg_parms.ni;
                        #else
                        if(lcdc_timimg_parms.ni == 0)
                                stHDMI_Ctrl.advi = 1;
                        #endif
                        #endif

                        stHDMI_Ctrl.tv = lcdc_timimg_parms.tv;

                	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
                			stHDMI_Ctrl.y2r = stHDMI_Ctrl.r2y?0:1;
                			stHDMI_Ctrl.r2y = 0;
                	#endif

                        // update video params.
                        memcpy(hdmi_dev->videoParam, &video_param, sizeof(video_param));

                        tccfb_output_starter(TCC_OUTPUT_HDMI, display_device, &stHDMI_Timing, &stHDMI_Ctrl, 0);

                        tcc_output_starter_make_extra_data(&video_param);
                        tcc_output_starter_hdmi_start(display_device);
                        msleep(90); //  CEA8610F F.3.6 Video Timing Transition (AVMUTE Recommendation)
                        // disable av mute
                        hdmi_api_avmute(hdmi_dev, 0);

                        if(edid_get_scdc_present()) {
                                scdc_write_source_version(hdmi_dev, 1);
                                scdc_tmds_config_status(hdmi_dev);
                                scdc_scrambling_status(hdmi_dev);
                        }
                }
        }while(0);
        return ret;
}

int tcc_hdmi_detect_cable(void)
{
        int ret = 0;

        struct hdmi_tx_dev *hdmi_dev = dwc_hdmi_api_get_dev();
        do {
                if(hdmi_dev == NULL) {
                        break;
                }
                if(gpio_is_valid(hdmi_dev->hotplug_gpio)) {
                        ret = hdmi_dev->hotplug_status;
                } else {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_dev->status)){
                                hdmi_phy_enable_hpd_sense(hdmi_dev);
                                ret = hdmi_phy_hot_plug_state(hdmi_dev);
                        } else {
                                pr_err("%s hdmi is not poser on\r\n", __func__);
                        }
                }
        } while(0);

        return ret;
}

int tcc_output_starter_hdmi_disable(void)
{
        struct hdmi_tx_dev *hdmi_dev = dwc_hdmi_api_get_dev();
        do {
                if(hdmi_dev == NULL) {
                        break;
                }

                hdmi_api_avmute(hdmi_dev, 1);
                msleep(90); //  CEA8610F F.3.6 Video Timing Transition (AVMUTE Recommendation)
                mc_disable_all_clocks(hdmi_dev);
                dwc_hdmi_power_off(hdmi_dev);
        } while(0);
        return 0;
}




