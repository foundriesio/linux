/*
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi_1_4_api.h>
#include <hdmi.h>

#include <regs-hdmi.h>

#define SRC_VERSION     "4.14_1.0.0"

#define HDMI_DEBUG 	0
#define HDMI_DEBUG_TIME 0

#if HDMI_DEBUG
#define dlog(args,...)   { pr_err("[ERROR][HDMI_V14] hdmi1.4: " args, ##__VA_ARGS__); }
#else
#define dlog(args,...)
#endif

#define HDMI_IOCTL_DEBUG 0
#if HDMI_IOCTL_DEBUG
#define io_dlog(args,...) {pr_err("[ERROR][HDMI_V14] hdmi1.4: " args, ##__VA_ARGS__); }
#else
#define io_dlog(args,...)
#endif

#if HDMI_DEBUG_TIME
unsigned long jstart, jend;
unsigned long ji2cstart, ji2cend;
#endif

static irqreturn_t hdmi_irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	unsigned int hdmi_reg, irq_flag;

	struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)dev_id;

	if(dev != NULL) {
		irq_flag = hdmi_reg_read(dev, HDMI_SS_INTC_FLAG);

		if (irq_flag & (1<<HDMI_IRQ_SPDIF)) {
			hdmi_reg = hdmi_reg_read(dev, HDMI_SS_SPDIF_IRQ_STATUS);

			hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_IRQ_STATUS);

			switch(dev->audioParam.mCodingType) {
				case NLPCM:
					// for NLPCM
					switch(irq_flag) {
						// normal
						case SPDIF_STATUS_RECOVERED_MASK:
						case SPDIF_HEADER_DETECTED_MASK:
							//pr_info("[INFO][HDMI_V14]Normal Procedure\r\n");
							break;
						case (SPDIF_HEADER_DETECTED_MASK | SPDIF_STATUS_RECOVERED_MASK):
							hdmi_reg = hdmi_reg_read(dev, HDMI_SS_SPDIF_IRQ_MASK);
							hdmi_reg &= ~(SPDIF_HEADER_DETECTED_MASK | SPDIF_STATUS_RECOVERED_MASK);

							hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_IRQ_MASK);

							//pr_info("[INFO][HDMI_V14]%s Audio start \r\n")
							hdmi_reg = SPDIF_RUNNING;
							hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_OP_CTRL);
							break;
						// error state
						default :
							    //pr_err("[ERROR][HDMI_V14]Fail! Audio Restart\n");
							    hdmi_reg = SPDIF_SIGNAL_RESET;
							    hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_OP_CTRL);
							    hdmi_reg = SPDIF_BUFFER_OVERFLOW_MASK |
							    		SPDIF_ABNORMAL_PD_MASK |
							    		SPDIF_PAPD_NOT_DETECTED_MASK |
							    		SPDIF_HEADER_DETECTED_MASK |
							    		SPDIF_HEADER_NOT_DETECTED_MASK |
									SPDIF_PREAMBLE_DETECTED_MASK |
									SPDIF_STATUS_RECOVERED_MASK |
									SPDIF_CLK_RECOVERY_FAIL_MASK;
							    hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_IRQ_MASK);
							    hdmi_reg = SPDIF_SIGNAL_DETECT;
							    hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_OP_CTRL);
							break;
					}
					break;

				case LPCM:
					// for LPCM
					switch(irq_flag) {
						// normal
						case SPDIF_STATUS_RECOVERED_MASK:
						// TODO: check if it is bug or not even if SPDIF_HEADER_NOT_DETECTED_MASK occurs, it works well.
						case (SPDIF_STATUS_RECOVERED_MASK | SPDIF_HEADER_NOT_DETECTED_MASK):
							hdmi_reg = hdmi_reg_read(dev, HDMI_SS_SPDIF_IRQ_MASK);
							hdmi_reg &= ~(SPDIF_STATUS_RECOVERED_MASK | SPDIF_HEADER_NOT_DETECTED_MASK);
							hdmi_reg_write(dev,  hdmi_reg, HDMI_SS_SPDIF_IRQ_MASK);

							//pr_info("[INFO][HDMI_V14]Succeed! Audio Start\n");
							hdmi_reg = SPDIF_RUNNING;
							hdmi_reg_write(dev, SPDIF_RUNNING, HDMI_SS_SPDIF_OP_CTRL); // 0b11 // run
							break;
						// error state
						default :
						    	// initialize history
//	                                            	pr_err("[ERROR][HDMI_V14]Fail! Audio Restart\n");
							hdmi_reg = SPDIF_SIGNAL_RESET;
							hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_OP_CTRL);
							hdmi_reg = SPDIF_BUFFER_OVERFLOW_MASK |
									SPDIF_PREAMBLE_DETECTED_MASK |
									SPDIF_STATUS_RECOVERED_MASK |
									SPDIF_CLK_RECOVERY_FAIL_MASK;
						    	hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_IRQ_MASK);
							hdmi_reg = SPDIF_SIGNAL_DETECT;
							hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_OP_CTRL);
							break;
					}
					break;

				default:
					break;
			}
			ret = IRQ_HANDLED;
		}

#if defined(CONFIG_TCC_HDCP1_CORE_DRIVER)
		if (irq_flag & (1<<HDMI_IRQ_HDCP)) {
			unsigned int hdmi_status;

			// check HDCP Status
			hdmi_status = hdmi_reg_read(dev, HDMI_STATUS);

			if (hdmi_status)
				hdmi_api_run_hdcp_callback(hdmi_status);

			ret = IRQ_HANDLED;
		}
#endif
	}

	return ret;
}

static void tcc_ddi_pwdn_hdmi(struct tcc_hdmi_dev *dev, char onoff)
{
	unsigned int val;

	if(dev != NULL) {
		// HDMI Power-on
		val = ddi_reg_read(dev, DDICFG_PWDN);
		if(onoff) {
			val &= ~PWDN_HDMI;
		} else {
			val |= PWDN_HDMI;
		}
		ddi_reg_write(dev, val, DDICFG_PWDN);
	}
}


/* SPDIF SEL */
static int tcc_ddi_hdmi_spdif_sel(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	unsigned int hdmi_reg, hdmi_val;
	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}
		if(dev->spdif_audio < 0) {
			pr_err("[ERROR][HDMI_V14]%s SPDIF block is not valid\r\n", __func__);
			break;
		}
		hdmi_val = (((dev->spdif_audio == 3)?0:1) & HDMICTRL_SPDIF_MASK) << HDMICTRL_SPDIF_SEL_SHIFT;
		hdmi_reg = ddi_reg_read(dev, DDICFG_HDMICTRL);
		hdmi_reg &= ~(HDMICTRL_SPDIF_MASK << HDMICTRL_SPDIF_SEL_SHIFT);
		hdmi_reg |= hdmi_val;
		ddi_reg_write(dev, hdmi_reg, DDICFG_HDMICTRL);
		ret = 0;
	}while(0);

	return ret;
}


static void tcc_ddi_hdmi_ctrl(struct tcc_hdmi_dev *dev, unsigned int index, char onoff)
{
	unsigned int val;
	unsigned int offset = 0x1000;
	if(dev != NULL) {
		val = ddi_reg_read(dev, DDICFG_HDMICTRL);

		switch(index) {
			case HDMICTRL_RESET_HDMI:
			      if(onoff) {
					val &= ~HDMICTRL_RESET_HDMI;
				} else {
					val |= HDMICTRL_RESET_HDMI;
				}
				offset = DDICFG_HDMICTRL;
				break;

			case HDMICTRL_RESET_SPDIF:
				if(onoff) {
					val &= ~HDMICTRL_RESET_SPDIF;
				} else {
					val |= HDMICTRL_RESET_SPDIF;
				}
				offset = DDICFG_HDMICTRL;
				break;

			case HDMICTRL_RESET_TMDS:
				if(onoff) {
					val &= ~HDMICTRL_RESET_TMDS;
				} else {
					val |= HDMICTRL_RESET_TMDS;
				}
				offset = DDICFG_HDMICTRL;
				break;

			case HDMICTRL_HDMI_ENABLE:
				if(onoff) {
					val |= HDMICTRL_HDMI_ENABLE;
				} else {
					val &= ~HDMICTRL_HDMI_ENABLE;
				}
				offset = DDICFG_HDMICTRL;
				break;
		}

		if(offset < 0x1000) {
			ddi_reg_write(dev, val, DDICFG_HDMICTRL);
		}
	}
}

static void hdmi_avi_update_checksum(struct tcc_hdmi_dev *dev)
{
	unsigned char data;
	unsigned char index, checksum;

	checksum = AVI_HEADER;

	if(dev != NULL) {
		hdmi_reg_write(dev, AVI_HEADER_BYTE0, HDMI_AVI_HEADER0);
		hdmi_reg_write(dev, AVI_HEADER_BYTE1, HDMI_AVI_HEADER1);
		hdmi_reg_write(dev, AVI_HEADER_BYTE2, HDMI_AVI_HEADER2);

		for (index = 0; index < AVI_PACKET_BYTE_LENGTH; index++) {
			data = hdmi_reg_read(dev, HDMI_AVI_BYTE1 + 4*index);
			checksum += data;
			//pr_info("[INFO][HDMI_V14]avi_byte_%d = 0x%x\r\n", index+1, data);
		}
		checksum = ~checksum +1;
		hdmi_reg_write(dev, checksum, HDMI_AVI_CHECK_SUM);
	}
}

static int hdmi_set_hdmimode(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		switch(dev->videoParam.mHdmi){
			case HDMI:
				hdmi_reg_write(dev, HDMI_MODE_SEL_HDMI,HDMI_MODE_SEL);
				hdmi_reg_write(dev, HDMICON2_HDMI,HDMI_CON_2);
				ret = 0;
				break;
			case DVI:
				hdmi_reg_write(dev, HDMI_MODE_SEL_DVI,HDMI_MODE_SEL);
				hdmi_reg_write(dev, HDMICON2_DVI,HDMI_CON_2);
				ret = 0;
				break;
			default:
				pr_info("[INFO][HDMI_V14]%s invlaid hdmi mode\r\n", __func__);
				break;
		}
	}while(0);
	return ret;
}

static int hdmi_set_video_timing(struct tcc_hdmi_dev *dev)
{
	// basic video parametres
	int ret = -1;
	int frame_packing = 0;
	dtd_t *detailed_timing;
	unsigned int htotal, vtotal, video_val;

	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		detailed_timing = &dev->videoParam.mDtd;

		if(dev->videoParam.mHdmiVideoFormat == 2 && dev->videoParam.m3dStructure == 0) {
			frame_packing = 1;
		}

		if(detailed_timing->mInterlaced && frame_packing) {
			pr_err("[ERROR][HDMI_V14]%s hdmi does not support Framepacking with interlaced mode\r\n", __func__);
			break;
		}

		htotal = detailed_timing->mHBlanking +detailed_timing->mHActive;

		/* Calculate vtotal */
		vtotal = detailed_timing->mVBlanking +detailed_timing->mVActive;

		if(detailed_timing->mInterlaced) {
			vtotal = (vtotal << 1) +1;
		}

		if(frame_packing) {
			vtotal = vtotal << 1;
		}

		// HBlank
		video_val = detailed_timing->mHBlanking;
		hdmi_reg_write(dev, video_val & 0xFF, HDMI_H_BLANK_0 );
		hdmi_reg_write(dev, (video_val >>8) & 0xFF, HDMI_H_BLANK_1 );

		// V1 Blank
		video_val = detailed_timing->mVBlanking;
		hdmi_reg_write(dev, video_val & 0xFF, HDMI_V1_BLANK_0 );
		hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V1_BLANK_1 );

		// HTotal
		video_val = htotal;
		hdmi_reg_write(dev, video_val & 0xFF, HDMI_H_LINE_0 );
		hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_H_LINE_1 );

		// VTotal
		hdmi_reg_write(dev, vtotal & 0xFF, HDMI_V_LINE_0 );
		hdmi_reg_write(dev, (vtotal >> 8) & 0xFF, HDMI_V_LINE_1 );

		// H POL 0: active high, 1: active low
		video_val = detailed_timing->mHSyncPolarity?0:1;
		hdmi_reg_write(dev, video_val, HDMI_HSYNC_POL );

		// V POL 0: active high, 1: active low
		video_val = detailed_timing->mVSyncPolarity?0:1;
		hdmi_reg_write(dev, video_val, HDMI_VSYNC_POL );

		// HSYNC Front
		video_val = detailed_timing->mHSyncOffset -2;
		hdmi_reg_write(dev, video_val & 0xFF, HDMI_H_SYNC_START_0 );
		hdmi_reg_write(dev, (video_val >>8 ) & 0xFF, HDMI_H_SYNC_START_1 );

		// HSYNC End
		video_val = (detailed_timing->mHSyncOffset) -2 + detailed_timing->mHSyncPulseWidth;
		hdmi_reg_write(dev, (video_val) & 0xFF , HDMI_H_SYNC_END_0 );
		hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_H_SYNC_END_1 );


		// VSYNC Front
		video_val = detailed_timing->mVSyncOffset;
		hdmi_reg_write(dev, video_val & 0xFF, HDMI_V_SYNC_LINE_BEF_1_0 );
		hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V_SYNC_LINE_BEF_1_1 );

		// VSYNC End
		video_val = detailed_timing->mVSyncOffset + detailed_timing->mVSyncPulseWidth;
		hdmi_reg_write(dev, (video_val) & 0xFF, HDMI_V_SYNC_LINE_BEF_2_0 );
		hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V_SYNC_LINE_BEF_2_1 );

		if ( detailed_timing->mInterlaced) {
			// for interlace
			hdmi_reg_write(dev, 0x1, HDMI_INT_PRO_MODE );

			if( detailed_timing->mCode == 39) {
 				/* 1920x1080@50i 1250vtotal - V TOP and V BOT are same */
				// V2 BLANK
				video_val = vtotal >> 1;
				// V TOP and V BOT are same
				hdmi_reg_write(dev, video_val & 0xFF, HDMI_V2_BLANK_0 );
				hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V2_BLANK_1 );

				// VBLANK_F0
				hdmi_reg_write(dev, (video_val += detailed_timing->mVBlanking) & 0xFF, HDMI_V_BLANK_F0_0 );
				hdmi_reg_write(dev, ((video_val += detailed_timing->mVBlanking) >> 8) & 0xFF, HDMI_V_BLANK_F0_1 );

				// VSYNC_LINE_AFT1
				video_val += detailed_timing->mVSyncOffset -1;
				hdmi_reg_write(dev, video_val & 0xFF , HDMI_V_SYNC_LINE_AFT_1_0);
				hdmi_reg_write(dev, ((video_val >> 8) & 0xFF), HDMI_V_SYNC_LINE_AFT_1_1);

				// VSYNC_LINE_AFT2
				video_val += detailed_timing->mVSyncPulseWidth;
				hdmi_reg_write(dev, video_val & 0xFF, HDMI_V_SYNC_LINE_AFT_2_0);
				hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V_SYNC_LINE_AFT_2_1);
			}
			else {
				/* V TOP and V BOT are not same */
				// V2 BLANK
				video_val = ((vtotal - (detailed_timing->mVBlanking << 1) -1) >> 1) + detailed_timing->mVBlanking;
				hdmi_reg_write(dev, video_val & 0xFF, HDMI_V2_BLANK_0 );
				hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V2_BLANK_1 );

				// VBLANK_F0
				hdmi_reg_write(dev, ((vtotal + (detailed_timing->mVBlanking << 1) + 1) >> 1) & 0xFF, HDMI_V_BLANK_F0_0);
				hdmi_reg_write(dev, (((vtotal + (detailed_timing->mVBlanking << 1) + 1) >> 1) >> 8) & 0xFF, HDMI_V_BLANK_F0_1);

				// VSYNC_LINE_AFT1
				video_val += detailed_timing->mVSyncOffset;
				hdmi_reg_write(dev, video_val & 0xFF , HDMI_V_SYNC_LINE_AFT_1_0);
				hdmi_reg_write(dev, (video_val >> 8 & 0xFF), HDMI_V_SYNC_LINE_AFT_1_1);

				// VSYNC_LINE_AFT2
				video_val += detailed_timing->mVSyncPulseWidth;
				hdmi_reg_write(dev, video_val & 0xFF, HDMI_V_SYNC_LINE_AFT_2_0);
				hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V_SYNC_LINE_AFT_2_1);
			}

			// VBLANK_F1
			hdmi_reg_write(dev, vtotal & 0xFF, HDMI_V_BLANK_F1_0);
			hdmi_reg_write(dev, (vtotal >> 8) & 0xFF, HDMI_V_BLANK_F1_1);

			video_val = (htotal >> 1) + detailed_timing->mHSyncOffset;

			// VSYNC_LINE_AFT_PXL_1
			hdmi_reg_write(dev, video_val & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_0 );
			hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_1 );
			// VSYNC_LINE_AFT_PXL_2
			hdmi_reg_write(dev, video_val & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_2_0 );
			hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_2_1 );
		}
		else {
			// for progressive
			hdmi_reg_write(dev, 0x0, HDMI_INT_PRO_MODE );

			if(frame_packing) {
				// V2 BLANK
				hdmi_reg_write(dev, vtotal & 0xFF, HDMI_V2_BLANK_0 );
				hdmi_reg_write(dev, (vtotal >> 8) & 0xFF, HDMI_V2_BLANK_1 );

				video_val = (vtotal >> 1);

				// VACT_SPACE1
				hdmi_reg_write(dev, video_val & 0xFF, HDMI_VACT_SPACE1_0);
				hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_VACT_SPACE1_1);

				// VACT_SPACE2
				video_val += detailed_timing->mVBlanking;
				hdmi_reg_write(dev, video_val & 0xFF, HDMI_VACT_SPACE2_0);
				hdmi_reg_write(dev, (video_val >> 8) & 0xFF, HDMI_VACT_SPACE2_1);
			} else {
				// V2 BLANK, same as V total
				hdmi_reg_write(dev, vtotal & 0xFF, HDMI_V2_BLANK_0 );
				hdmi_reg_write(dev, (vtotal >> 8) & 0xFF, HDMI_V2_BLANK_1 );
			}
		}
		ret = 0;
	} while(0);

	return ret;
}

static int hdmi_set_color_space(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	unsigned int hdmi_con_0, val_avi_byte1;


	if(dev != NULL) {
		hdmi_con_0 = hdmi_reg_read(dev, HDMI_CON_0);
		val_avi_byte1 = hdmi_reg_read(dev, HDMI_AVI_BYTE1);

		// clear fields
		hdmi_con_0 &= ~HDMI_YCBCR422_ENABLE;
		val_avi_byte1 &= ~AVI_CS_MASK;

		switch(dev->videoParam.mEncodingOut) {
			case YCC422:
				// set video input interface
				hdmi_con_0 |= HDMI_YCBCR422_ENABLE;
				val_avi_byte1 |= AVI_CS_Y422;
				ret = 0;
				break;

			case YCC444:
				val_avi_byte1 |= AVI_CS_Y444;
				ret = 0;
				break;

			case RGB:
				ret = 0;
				break;
			default:
				pr_err("[ERROR][HDMI_V14]%s invalid encoding output \r\n", __func__);
				break;
		}

		hdmi_reg_write(dev, hdmi_con_0, HDMI_CON_0);
		hdmi_reg_write(dev, val_avi_byte1, HDMI_AVI_BYTE1);
	}
	return ret;
}

static int hdmi_set_color_depth(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	if(dev != NULL) {
		switch (dev->videoParam.mColorResolution) {
			case COLOR_DEPTH_12:
				// set GCP CD
				hdmi_reg_write(dev, GCP_CD_36BPP,HDMI_GCP_BYTE2);
				// set DC_CTRL
				hdmi_reg_write(dev, HDMI_DC_CTL_12,HDMI_DC_CONTROL);
				ret = 0;
				break;
			case COLOR_DEPTH_10:
				// set GCP CD
				hdmi_reg_write(dev, GCP_CD_30BPP,HDMI_GCP_BYTE2);
				// set DC_CTRL
				hdmi_reg_write(dev, HDMI_DC_CTL_10,HDMI_DC_CONTROL);
				ret = 0;
				break;
			case COLOR_DEPTH_8:
				// set GCP CD
				hdmi_reg_write(dev, GCP_CD_24BPP,HDMI_GCP_BYTE2);
				// set DC_CTRL
				hdmi_reg_write(dev, HDMI_DC_CTL_8,HDMI_DC_CONTROL);
				ret = 0;
				break;
			default:
				pr_err("[ERROR][HDMI_V14]%s invalid color depth\r\n", __func__);
				break;
		}
	}
	return ret;
}


static int hdmi_set_colorimetry(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	unsigned int avi_byte_2, avi_byte_3;

	if(dev != NULL) {
		avi_byte_2 = hdmi_reg_read(dev, HDMI_AVI_BYTE2);
		avi_byte_3 = hdmi_reg_read(dev, HDMI_AVI_BYTE3);

		avi_byte_2 &= ~AVI_COLORIMETRY_MASK;
		avi_byte_3 &= ~AVI_COLORIMETRY_EXT_MASK;

		switch (dev->videoParam.mColorimetry) {
			case COLORIMETRY_NODATA:
			case ITU601:
			case ITU709:
				avi_byte_2 |= ((dev->videoParam.mColorimetry << 6) & AVI_COLORIMETRY_MASK);
				ret = 0;
				break;

			case EXTENDED_COLORIMETRY:
				switch(dev->videoParam.mExtColorimetry) {
					case XV_YCC601:
					case XV_YCC709:
					case S_YCC601:
					case ADOBE_YCC601:
					case ADOBE_RGB:
					case BT2020YCCBCR:
					case BT2020YCBCR:
						avi_byte_2 |= ((dev->videoParam.mColorimetry << 6) & AVI_COLORIMETRY_MASK);
						avi_byte_3 |= ((dev->videoParam.mColorimetry << 4) & AVI_COLORIMETRY_EXT_MASK);
						ret = 0;
						break;
				}
				break;
		}

		if(ret < 0) {
			pr_err("[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
		} else {
			hdmi_reg_write(dev, avi_byte_2, HDMI_AVI_BYTE2);
			hdmi_reg_write(dev, avi_byte_3, HDMI_AVI_BYTE3);
		}
	}
	return ret;
}

static int hdmi_set_pixel_limit(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	unsigned int hdmi_con_reg, avi_byte3, avi_byte5;

	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		hdmi_con_reg = hdmi_reg_read(dev, HDMI_CON_1);
		hdmi_con_reg &= ~HDMICON1_LIMIT_MASK;

		avi_byte3 = hdmi_reg_read(dev, HDMI_AVI_BYTE3);
		avi_byte5 = hdmi_reg_read(dev, HDMI_AVI_BYTE5);

		avi_byte3 &= ~AVI_RGB_QUANTIZATION_MASK;
		avi_byte5 &= ~AVI_YUV_QUANTIZATION_MASK;

		switch(dev->videoParam.mEncodingOut) {
			case RGB:
				switch(dev->videoParam.mRgbQuantizationRange) {
					case 0:
						ret = 0;
						break;
					case 1:
						pr_info("[INFO][HDMI_V14]%s rgb limited range\r\n", __func__);
						hdmi_con_reg |= HDMICON1_RGB_LIMIT;
						avi_byte3 |= AVI_RGB_QUANTIZATION_LIMITED;
						avi_byte5 |= AVI_YUV_QUANTIZATION_LIMITED;
						ret = 0;
						break;

					case 2:
						pr_info("[INFO][HDMI_V14]%s rgb full range\r\n", __func__);
						avi_byte3 |= AVI_RGB_QUANTIZATION_FULL;
						avi_byte5 |= AVI_YUV_QUANTIZATION_FULL;
						ret = 0;
						break;
				}
				break;
			case YCC422:
			case YCC444:
				switch(dev->videoParam.mYuvQuantizationRange) {
					case 0:
						pr_info("[INFO][HDMI_V14]%s yuv limited range\r\n", __func__);
						hdmi_con_reg |= HDMICON1_YCBCR_LIMIT;
						avi_byte3 |= AVI_RGB_QUANTIZATION_LIMITED;
						avi_byte5 |= AVI_YUV_QUANTIZATION_LIMITED;
						ret = 0;
						break;

					case 1:
						pr_info("[INFO][HDMI_V14]%s yuv full range\r\n", __func__);
						avi_byte3 |= AVI_RGB_QUANTIZATION_FULL;
						avi_byte5 |= AVI_YUV_QUANTIZATION_FULL;
						ret = 0;
						break;
				}
				break;
		}


		if(ret < 0) {
			pr_err("[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		hdmi_reg_write(dev, hdmi_con_reg, HDMI_CON_1);

		hdmi_reg_write(dev, avi_byte3, HDMI_AVI_BYTE3);

		hdmi_reg_write(dev, avi_byte5, HDMI_AVI_BYTE5);

		#if 1
		pr_info("[INFO][HDMI_V14]%s byte3(0x%x), byte5(0x%x)\r\n", __func__,
			hdmi_reg_read(dev, HDMI_AVI_BYTE3),hdmi_reg_read(dev, HDMI_AVI_BYTE5));
		#endif
	}while(0);

	return ret;
}

static int hdmi_set_pixel_aspect_ratio(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	unsigned int avi_byte_2;
	unsigned int avi_picture_aspect_ratio;
	unsigned short h_imageSize, v_imageSize;
	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		avi_byte_2 = hdmi_reg_read(dev, HDMI_AVI_BYTE2);
		avi_byte_2 &= ~(AVI_PICTURE_ASPECT_RATIO_16_9 | AVI_PICTURE_ASPECT_RATIO_4_3);

		h_imageSize = dev->videoParam.mDtd.mHImageSize;
		v_imageSize = dev->videoParam.mDtd.mVImageSize;

		if(h_imageSize != 0 || v_imageSize != 0) {
			avi_picture_aspect_ratio = (h_imageSize * 10) % v_imageSize;

			if(avi_picture_aspect_ratio > 5) {
				avi_byte_2 |= AVI_PICTURE_ASPECT_RATIO_16_9;
			} else {
				avi_byte_2 |= AVI_PICTURE_ASPECT_RATIO_4_3;
			}
		}

		hdmi_reg_write(dev, avi_byte_2, HDMI_AVI_BYTE2);
		ret = 0;
	}while(0);

	return ret;
}

static int hdmi_set_active_aspect_ratio(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	unsigned int avi_byte_1, avi_byte_2;

	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		avi_byte_1 = hdmi_reg_read(dev, HDMI_AVI_BYTE1);
		avi_byte_1 &= ~AVI_ACTIVE_ASPECT_RATIO_PRESENT;

		avi_byte_2 = hdmi_reg_read(dev, HDMI_AVI_BYTE2);
		avi_byte_2 &= ~AVI_ACTIVE_ASPECT_RATIO_MASK;

		  /* Active Portion Aspect Ratio \n
		   *  8: Same as Picture Aspect Ratio \n
		   *  9: 4:3 (Center) \n
		   * 10: 16:9 (Center) */
		switch(dev->videoParam.mActiveFormatAspectRatio) {
			case  8:
			case  9:
			case 10:
				avi_byte_1 |= AVI_ACTIVE_ASPECT_RATIO_PRESENT;
				avi_byte_2 |= (dev->videoParam.mActiveFormatAspectRatio & AVI_ACTIVE_ASPECT_RATIO_MASK);
			break;
			default:
				pr_info("[INFO][HDMI_V14]%s active aspect ratio is not present\r\n", __func__);
				break;
		};

		hdmi_reg_write(dev, avi_byte_1, HDMI_AVI_BYTE1);
		hdmi_reg_write(dev, avi_byte_2, HDMI_AVI_BYTE2);

		#if 1
		pr_info("[INFO][HDMI_V14]%s byte1(0x%x), byte2(0x%x)\r\n", __func__,
			hdmi_reg_read(dev, HDMI_AVI_BYTE1),hdmi_reg_read(dev, HDMI_AVI_BYTE2));
		#endif
		ret = 0;
	}while(0);

	return ret;
}

static int hdmi_set_avi_vic(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	do {
		if(dev == NULL) {
			pr_info("[INFO][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		hdmi_reg_write(dev, dev->videoParam.mDtd.mCode, HDMI_AVI_BYTE4);
		ret = 0;
	}while(0);

	return ret;
}

static int hdmi_set_pixel_repetition(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	unsigned int hdmi_con_1, avi_byte_5;

	if(!dev) {
		goto out;
	}

	hdmi_con_1 = hdmi_reg_read(dev, HDMI_CON_1);
	hdmi_con_1 &= ~HDMICON1_DOUBLE_PIXEL_REPETITION;

	avi_byte_5 = hdmi_reg_read(dev, HDMI_AVI_BYTE5);
	avi_byte_5 &= ~AVI_PIXEL_REPETITION_MASK;

	if(dev->videoParam.mDtd.mPixelRepetitionInput){
		hdmi_con_1 |= HDMICON1_DOUBLE_PIXEL_REPETITION;
		avi_byte_5 |= AVI_PIXEL_REPETITION_DOUBLE;
	}

	hdmi_reg_write(dev, hdmi_con_1, HDMI_CON_1);
	hdmi_reg_write(dev, avi_byte_5, HDMI_AVI_BYTE5);
	return 0;
out:
	return ret;
}

static void hdmi_enable_bluescreen(struct tcc_hdmi_dev *dev, unsigned char enable)
{
	unsigned int reg;

	if(dev != NULL) {
		reg = hdmi_reg_read(dev, HDMI_CON_0);

		dlog("%s enable = %d\n", __func__, enable);

		if (enable) {
			reg |= HDMI_BLUE_SCR_ENABLE;
		} else {
			reg &= ~HDMI_BLUE_SCR_ENABLE;
		}
		hdmi_reg_write(dev, reg,HDMI_CON_0);
	}
}

/* Global APIS */
unsigned int ddi_reg_read(struct tcc_hdmi_dev *dev, unsigned int offset)
{
	unsigned int val = 0;
	volatile void __iomem *ddibus_io = dev->ddibus_io;;

	if(offset & 0x3){
		return val;
	}

	if(ddibus_io != NULL) {
		val = ioread32((void*)(ddibus_io + offset));
	}
	return val;
}

void ddi_reg_write(struct tcc_hdmi_dev *dev, unsigned int data, unsigned int offset)
{
	volatile void __iomem *ddibus_io = dev->ddibus_io;;

	if(offset & 0x3){
		return;
	}

	if(ddibus_io != NULL) {
		iowrite32(data, (void*)(ddibus_io + offset));
	}
}

void tcc_ddi_swreset_hdmi(struct tcc_hdmi_dev *dev, char onoff)
{
	unsigned int val;

	if(dev != NULL) {
		// HDMI Power-on
		val = ddi_reg_read(dev, DDICFG_PWDN);
		if(onoff) {
			val &= ~SWRESET_HDMI;
		} else {
			val |= SWRESET_HDMI;
		}
		ddi_reg_write(dev, val, DDICFG_PWDN);
	}
}

int hdmi_reg_io(struct tcc_hdmi_dev *dev, unsigned int *offset, volatile void __iomem **hdmi_io)
{
	if (*offset >= HDMIDP_PHYREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_phy_io;
		*offset -= HDMIDP_PHYREG(0);
	} else if (*offset >= HDMIDP_I2SREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_i2s_io;
		*offset -= HDMIDP_I2SREG(0);
	} else if (*offset >= HDMIDP_SPDIFREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_spdif_io;
		*offset -= HDMIDP_SPDIFREG(0);
	} else if (*offset >= HDMIDP_AESREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_aes_io;
		*offset -= HDMIDP_AESREG(0);
	} else if (*offset >= HDMIDP_RGBREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_rgb_io;
		*offset -= HDMIDP_RGBREG(0);
	} else if (*offset >= HDMIDP_HDCPREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_hdcp_io;
		*offset -= HDMIDP_HDCPREG(0);
	} else if (*offset >= HDMIDP_HDMIREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_core_io;
		*offset -= HDMIDP_HDMIREG(0);
	} else if (*offset >= HDMIDP_HDMI_SSREG(0)) {
		*hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
		*offset -= HDMIDP_HDMI_SSREG(0);
	}
}

unsigned int hdmi_reg_read(struct tcc_hdmi_dev *dev, unsigned int offset)
{
	unsigned int val = 0;
	volatile void __iomem *hdmi_io = NULL;

	if(offset & 0x3) {
		return val;
	}

	if(dev != NULL) {
		hdmi_reg_io(dev, &offset, &hdmi_io);
	}
	if(hdmi_io == NULL) {
		pr_err("[ERROR][HDMI_V14]%s offset(0x%x) is out of range\r\n", __func__, offset);
	} else {
		//pr_info("[INFO][HDMI_V14] >> Read (%p)\r\n", (void*)(hdmi_io + offset));
		val = ioread32((void*)(hdmi_io + offset));
	}
	return val;
}

void hdmi_reg_write(struct tcc_hdmi_dev *dev, unsigned int data, unsigned int offset)
{
	volatile void __iomem *hdmi_io = NULL;

	if(offset & 0x3) {
		return;
	}

	if(dev != NULL) {
		hdmi_reg_io(dev, &offset, &hdmi_io);
	}
	if(hdmi_io == NULL) {
		pr_err("[ERROR][HDMI_V14]%s offset(0x%x) is out of range\r\n", __func__, offset);
	} else {
		//pr_info("[INFO][HDMI_V14] >> Write(%p) = 0x%x\r\n", (void*)(hdmi_io + offset), data);
		iowrite32(data, (void*)(hdmi_io + offset));
	}
}

void hdmi_aui_update_checksum(struct tcc_hdmi_dev *dev)
{
	unsigned char index, checksum;

	hdmi_reg_write(dev, AUI_HEADER_BYTE0, HDMI_AUI_HEADER0);
	hdmi_reg_write(dev, AUI_HEADER_BYTE1, HDMI_AUI_HEADER1);
	hdmi_reg_write(dev, AUI_HEADER_BYTE2, HDMI_AUI_HEADER2);

	checksum = AUI_HEADER;
	if(dev != NULL) {
		for (index = 0; index < AUI_PACKET_BYTE_LENGTH; index++)
		{
			checksum += hdmi_reg_read(dev, HDMI_AUI_BYTE1 + 4*index);
		}
		checksum = ~checksum +1;
		hdmi_reg_write(dev, checksum, HDMI_AUI_CHECK_SUM);
	}
}

int hdmi_video_config(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	videoParams_t *videoParam;

	do {

		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			videoParam = &dev->videoParam;
			break;
		}

		/* Enable AvMute */
		hdmi_av_mute(dev, 1);

		/* Set HDMI output mode */
		if(hdmi_set_hdmimode(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set hdmi mode\r\n", __func__);
			break;
		}

		if(hdmi_set_video_timing(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set video timing\r\n", __func__);
			break;
		}

		if(hdmi_set_color_space(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set color space\r\n", __func__);
			break;
		}
		if(hdmi_set_color_depth(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set color depth\r\n", __func__);
			break;
		}

		if(hdmi_set_colorimetry(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set colorimetry\r\n", __func__);
			break;
		}

		if(hdmi_set_pixel_limit(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set quantization\r\n", __func__);
			break;
		}

		if(hdmi_set_pixel_aspect_ratio(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set pixel aspect ratio\r\n", __func__);
			break;
		}
		if(hdmi_set_active_aspect_ratio(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set active aspect ratio\r\n", __func__);
			break;
		}


		if(hdmi_set_avi_vic(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set videi identification code\r\n", __func__);
			break;
		}


		if(hdmi_set_pixel_repetition(dev) < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed to set pixel repetition\r\n", __func__);
			break;

		}
		ret = 0;
	} while(0);
	return ret;
}

int hdmi_set_spd_infoframe(struct tcc_hdmi_dev *dev)
{
	int i, len, ret = -1;
	unsigned int val;
	unsigned char checksum;

	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
				break;
		}

		if(dev->videoParam.mHdmi == DVI) {
			hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_SPD_CON);
			ret = 0;
			break;
		}

		if(dev->packetParam.spd.vendor_name_length == 0 ||
				dev->packetParam.spd.product_name == 0) {
			hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_SPD_CON);
			ret = 0;
			break;
		}

		hdmi_reg_write(dev, 0x83, HDMI_SPD_HEADER0);
		hdmi_reg_write(dev, 0x1, HDMI_SPD_HEADER1);
		hdmi_reg_write(dev, 25, HDMI_SPD_HEADER2);

		len = ARRAY_SIZE(dev->packetParam.spd.vendor_name);
		for (i = 0; i < len; i++) {
			if(i < dev->packetParam.spd.vendor_name_length) {
				val = dev->packetParam.spd.vendor_name[i] & 0xFF;
			} else {
				val = 0;
			}
			hdmi_reg_write(dev, val, HDMI_SPD_DATA01 + (i << 2));
		}

		len = ARRAY_SIZE(dev->packetParam.spd.product_name);
		for (i = 0; i < len; i++) {
			if(i < dev->packetParam.spd.product_name_length) {
				val = dev->packetParam.spd.product_name[i] & 0xFF;
			} else {
				val = 0;
			}
			hdmi_reg_write(dev, val, HDMI_SPD_DATA09 + (i << 2));
		}

		/* Souce Information */
		hdmi_reg_write(dev, dev->packetParam.spd.source_information, HDMI_SPD_DATA25);

		checksum = 0;
		for(i = 0; i < 3; i++) {
			checksum += hdmi_reg_read(dev, HDMI_SPD_HEADER0 + (i << 2));
		}

		for(i = 0; i < 25; i++) {
			checksum += hdmi_reg_read(dev, HDMI_SPD_DATA01 + (i << 2));
		}

		checksum = ~checksum +1;
		hdmi_reg_write(dev, checksum, HDMI_SPD_CHECK_SUM);

		hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_SPD_CON);
		ret = 0;

	} while(0);

	return ret;
}


int hdmi_set_vsif_infoframe(struct tcc_hdmi_dev *dev)
{
	int i, len, ret = -1;
	unsigned int val;
	unsigned char checksum;

	do {
		dlog("%s\r\n", __func__);
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		if(dev->videoParam.mHdmi == DVI) {
			hdmi_reg_write(dev, DO_NOT_TRANSMIT, HDMI_VSI_CON);
			ret = 0;
			break;
		}

		if(dev->packetParam.vsif.oui == 0 ) {
			hdmi_reg_write(dev, DO_NOT_TRANSMIT, HDMI_VSI_CON);
			ret = 0;
			break;
		}

		/* VSI Header */
		hdmi_reg_write(dev, 0x81, HDMI_VSI_HEADER0);
		hdmi_reg_write(dev, 0x1, HDMI_VSI_HEADER1);
		hdmi_reg_write(dev, dev->packetParam.vsif.payload_length + 3, HDMI_VSI_HEADER2);
		dlog("%s VSI_HEADER2 = 0x%x \r\n", __func__,hdmi_reg_read(dev, HDMI_VSI_HEADER2));
		// calc checksum

		/* OUI */
		hdmi_reg_write(dev, (dev->packetParam.vsif.oui >>  0) & 0xFF, HDMI_VSI_DATA01);
		hdmi_reg_write(dev, (dev->packetParam.vsif.oui >>  8) & 0xFF, HDMI_VSI_DATA02);
		hdmi_reg_write(dev, (dev->packetParam.vsif.oui >> 16) & 0xFF, HDMI_VSI_DATA03);

		/* Payload */
		len = ARRAY_SIZE(dev->packetParam.vsif.payload);
		for (i = 0; i < len; i++) {
			if(i < dev->packetParam.vsif.payload_length) {
				val = dev->packetParam.vsif.payload[i] & 0xFF;
			} else {
				val = 0;
			}
			hdmi_reg_write(dev, val, HDMI_VSI_DATA04 + (i << 2));
		}
		checksum = 0;
		for(i = 0; i < 3; i++) {
			checksum += hdmi_reg_read(dev, HDMI_VSI_HEADER0 + (i << 2));
		}
		for(i = 0; i < dev->packetParam.vsif.payload_length +3; i++) {
			dlog("%s HDMI_VSI_DATA01 = 0x%x \r\n", __func__,hdmi_reg_read(dev, HDMI_VSI_HEADER2));
			checksum += hdmi_reg_read(dev, HDMI_VSI_DATA01 + (i << 2));
		}

		checksum = ~checksum +1;
		hdmi_reg_write(dev, checksum & 0xFF, HDMI_VSI_DATA00);
		dlog("%s checksum = 0x%x \r\n", __func__,hdmi_reg_read(dev, HDMI_VSI_DATA00));

		hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_VSI_CON);
		ret = 0;

	} while(0);

	return ret;
}

int hdmi_get_power_status(struct tcc_hdmi_dev *dev)
{
	int power_status = 0;
	if(dev != NULL) {
		power_status = dev->power_status;
	}
	return power_status;
}

int hdmi_get_VBlank_internal(struct tcc_hdmi_dev *dev)
{
	unsigned int VBlankValue = 0;
	if(dev != NULL) {
		VBlankValue = hdmi_reg_read(dev, HDMI_V1_BLANK_0) & 0xFF;
		VBlankValue |= ((hdmi_reg_read(dev, HDMI_V1_BLANK_1) & 0xFF) << 8);
	}

	return VBlankValue;
}

int hdmi_av_mute(struct tcc_hdmi_dev *dev, int av_mute)
{
	int ret = -1;
	unsigned int hdmi_mode_sel, gcp_byte_1;

	if(!dev)
		goto out;

	hdmi_mode_sel = hdmi_reg_read(dev, HDMI_MODE_SEL);
	if (!(hdmi_mode_sel & HDMI_MODE_SEL_HDMI)) {
		ret = 0;
		goto out;
	}
	gcp_byte_1 = hdmi_reg_read(dev, HDMI_GCP_BYTE1);
	gcp_byte_1 &= ~(GCP_AVMUTE_ON|GCP_AVMUTE_OFF);

	if (av_mute) {
		// set AV Mute
		gcp_byte_1 |= GCP_AVMUTE_ON;
	} else {
		gcp_byte_1 |= GCP_AVMUTE_OFF;
	}

	hdmi_reg_write(dev, gcp_byte_1, HDMI_GCP_BYTE1);
	hdmi_reg_write(dev, GCP_TRANSMIT_EVERY_VSYNC,HDMI_GCP_CON);

	return 0;
out:
	return ret;
}

void hdmi_start_internal(struct tcc_hdmi_dev *dev)
{
	unsigned int reg, mode;

	unsigned int avi_byte_1, avi_byte_4, hdmi_dc_control;

	if(dev != NULL) {
		if(!dev->hdmi_started) {
			pr_info("[INFO][HDMI_V14]%s \r\n", __func__);
			hdmi_audio_set_enable(dev, 1);

			// check HDMI mode
			mode = hdmi_reg_read(dev, HDMI_MODE_SEL) & HDMI_MODE_SEL_HDMI;
			reg = hdmi_reg_read(dev, HDMI_CON_0);

			// enable external vido gen.
			hdmi_reg_write(dev, HDMI_EXTERNAL_VIDEO,HDMI_VIDEO_PATTERN_GEN);

			if (mode) {
				// HDMI

				// enable AVI packet: mandatory
				// update avi packet checksum
				hdmi_avi_update_checksum(dev);
				// enable avi packet
				hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_AVI_CON);

				// check if audio is enable
				if (hdmi_reg_read(dev, HDMI_ACR_CON)) {
					// enable aui packet
					hdmi_aui_update_checksum(dev);
					hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_AUI_CON);
					reg |= HDMI_ASP_ENABLE;
				}

				// check if it is deep color mode or not
				if (hdmi_reg_read(dev, HDMI_DC_CONTROL)) {
					hdmi_reg_write(dev, GCP_TRANSMIT_EVERY_VSYNC, HDMI_GCP_CON);
				} else {
					// disable GCP
					hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_GCP_CON);
				}

				// for checking
				udelay(200);

				// enable hdmi

				//-------------------------------------------------------------------------------------------
				// In TCC897x HDMI Link, Encoding option is must disabled.
				//-------------------------------------------------------------------------------------------
				hdmi_reg_write(dev, reg|HDMI_SYS_ENABLE/*|HDMI_ENCODING_OPTION_ENABLE*/, HDMI_CON_0);
				//-------------------------------------------------------------------------------------------
			}
			else // DVI
			{
				// disable all packet
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_AVI_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_AUI_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_GCP_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_GAMUT_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ACP_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ISRC_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_MPG_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_SPD_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ACR_CON);
				hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_VSI_CON);

				// enable hdmi without audio
				reg &= ~HDMI_ASP_ENABLE;

				//-------------------------------------------------------------------------------------------
				// In TCC897x HDMI Link, Encoding option is must disabled.
				//-------------------------------------------------------------------------------------------
				hdmi_reg_write(dev, reg|HDMI_SYS_ENABLE/*|HDMI_ENCODING_OPTION_ENABLE*/, HDMI_CON_0);
				//-------------------------------------------------------------------------------------------
			}

			hdmi_enable_bluescreen(dev, 0);

			/* print Information */
			avi_byte_1 = hdmi_reg_read(dev, HDMI_AVI_BYTE1);
			avi_byte_4 = hdmi_reg_read(dev, HDMI_AVI_BYTE4);
			hdmi_dc_control = hdmi_reg_read(dev, HDMI_DC_CONTROL);

			pr_info("[INFO][HDMI_V14]\r\n VIDEO-%s VIC[%d] %s - %dBIT\r\n", (mode==0)?"DVI":"HDMI", avi_byte_4,
				(avi_byte_1 & AVI_CS_Y444)?"YCbCr444":(avi_byte_1 & AVI_CS_Y422)?"YCbCr422":"RGB",
				(hdmi_dc_control & HDMI_DC_CTL_12)?36:(hdmi_dc_control & HDMI_DC_CTL_10)?30:24);
		}
		dev->hdmi_started = 1;
	}

}

void hdmi_stop_internal(struct tcc_hdmi_dev *dev)
{
	unsigned int hdmi_reg;

	if(dev != NULL) {
		pr_info("[INFO][HDMI_V14]%s \r\n", __func__);
		if(dev->hdmi_started) {
			hdmi_audio_set_enable(dev, 0);
			hdmi_reg = hdmi_reg_read(dev, HDMI_CON_0);
			hdmi_reg &= ~HDMI_SYS_ENABLE;
			hdmi_reg_write(dev, hdmi_reg, HDMI_CON_0);
			tcc_hdmi_phy_power_down(dev);
		}
		dev->hdmi_started = 0;
	}
}

void tcc_hdmi_power_on(struct tcc_hdmi_dev *dev)
{
	unsigned int  val;

	if(dev != NULL) {
		if(++dev->power_enable_count == 1) {
			if(!dev->suspend) {
				#if defined(CONFIG_REGULATOR)
				/*
				if(dev->vdd_hdmi != NULL) {
					ret = regulator_enable(dev->vdd_hdmi);
					if (ret)
						pr_err("[ERROR][HDMI_V14]failed to enable hdmi regulator: %d\n", ret);
				}
				*/
				#endif

				if(!IS_ERR_OR_NULL(dev->hdmi_ref_clk))
					clk_prepare_enable(dev->hdmi_ref_clk);
				if(!IS_ERR_OR_NULL(dev->hdmi_ddibus_clk))
					clk_prepare_enable(dev->hdmi_ddibus_clk);

				udelay(100);

				if(!IS_ERR_OR_NULL(dev->hdmi_ref_clk ))
					clk_set_rate(dev->hdmi_ref_clk, HDMI_LINK_CLK_FREQ);

				if(!IS_ERR_OR_NULL(dev->hdmi_phy_apb_clk)) {
					clk_prepare_enable(dev->hdmi_phy_apb_clk);
					clk_set_rate(dev->hdmi_phy_apb_clk, HDMI_APB_CLK_FREQ);
				}

				if(!IS_ERR_OR_NULL(dev->pmu_iso_clk))
					clk_prepare_enable(dev->pmu_iso_clk);


				/* Audio Clock */
				if(!IS_ERR_OR_NULL(dev->hdmi_audio_clk))
					clk_prepare_enable(dev->hdmi_audio_clk);

				// HDMI Power-on
				tcc_ddi_pwdn_hdmi(dev, 0);

				val = ddi_reg_read(dev, DDICFG_PWDN);
				ddi_reg_write(dev, val & ~(0xf << 8), DDICFG_PWDN);

				hdmi_phy_reset(dev);

				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_HDMI_ENABLE, 1);

				tcc_ddi_hdmi_spdif_sel(dev);

				val = hdmi_reg_read(dev, HDMI_SS_INTC_CON);
				hdmi_reg_write(dev, val & ~(1<<HDMI_IRQ_HDCP), HDMI_SS_INTC_CON);

				// disable SPDIF INT
				val = hdmi_reg_read(dev, HDMI_SS_INTC_CON);
				hdmi_reg_write(dev, val & ~(1<<HDMI_IRQ_SPDIF), HDMI_SS_INTC_CON);
				pr_info("[INFO][HDMI_V14]%s End\n", __FUNCTION__);
			}
			dev->power_status = 1;
		}
	}
}

void tcc_hdmi_power_off(struct tcc_hdmi_dev *dev)
{
	if(dev != NULL) {
		if(dev->power_enable_count == 1) {
			dev->power_status = 0;
			dev->power_enable_count = 0;
			if(!dev->suspend) {
				hdmi_stop_internal(dev);

				// HDMI PHY Reset
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_HDMI, 0);
				udelay(1);
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_HDMI, 1);

				// HDMI SPDIF Reset
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_SPDIF, 0);
				udelay(1);
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_SPDIF, 1);

				// HDMI TMDS Reset
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_TMDS, 0);
				udelay(1);
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_TMDS, 1);

				// swreset DDI_BUS HDMI
				tcc_ddi_swreset_hdmi(dev, 1);
				udelay(1);
				tcc_ddi_swreset_hdmi(dev, 0);

				// disable DDI_BUS HDMI CLK
				tcc_ddi_hdmi_ctrl(dev, HDMICTRL_HDMI_ENABLE, 0);

				// ddi hdmi power down
				tcc_ddi_pwdn_hdmi(dev, 1);

				if(!IS_ERR_OR_NULL(dev->pmu_iso_clk ))
					clk_disable_unprepare(dev->pmu_iso_clk);

				// enable HDMI PHY Power-off
				if(!IS_ERR_OR_NULL(dev->hdmi_phy_apb_clk ))
					clk_disable_unprepare(dev->hdmi_phy_apb_clk); // power down

				// gpio power on
				udelay(100);

				/* Audio Clock */
				if(!IS_ERR_OR_NULL(dev->hdmi_audio_clk ))
					clk_disable_unprepare(dev->hdmi_audio_clk);

				if(!IS_ERR_OR_NULL(dev->hdmi_ref_clk ))
					clk_disable_unprepare(dev->hdmi_ref_clk);
				if(!IS_ERR_OR_NULL(dev->hdmi_ddibus_clk))
					clk_disable_unprepare(dev->hdmi_ddibus_clk);


				#if defined(CONFIG_REGULATOR)
				/*
				if(dev->vdd_hdmi != NULL) {
					regulator_disable(dev->vdd_hdmi);
				}
				*/
				#endif
				pr_info("[INFO][HDMI_V14]%s Finish\n", __FUNCTION__);
			}
		} else if(dev->power_enable_count > 1) {
			dev->power_enable_count--;
		}
	}
}



#ifdef CONFIG_PM
static int hdmi_suspend(struct device *dev)
{
	int suspend_power_enable_count;
	struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;

	if(hdmi_tx_dev != NULL) {
		if(hdmi_tx_dev->runtime_suspend) {
			pr_info("[INFO][HDMI_V14]hdmi runtime suspend\r\n");
		} else {
			pr_info("[INFO][HDMI_V14]hdmi suspend\r\n");
		}
		if(!hdmi_tx_dev->suspend) {
			if(hdmi_tx_dev->power_enable_count > 1) {
				hdmi_stop_internal(hdmi_tx_dev);
				suspend_power_enable_count = hdmi_tx_dev->power_enable_count;
				hdmi_tx_dev->power_enable_count = 1;
				tcc_hdmi_power_off(hdmi_tx_dev);
				hdmi_tx_dev->power_enable_count = suspend_power_enable_count;
			}
			hdmi_tx_dev->suspend = 1 ;
			hdmi_tx_dev->os_suspend = 1;
		}
	}
	return 0;
}

static int hdmi_resume(struct device *dev)
{
	int suspend_power_enable_count;
	struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;

	if(hdmi_tx_dev != NULL) {
		if(hdmi_tx_dev->runtime_suspend) {
			pr_info("[INFO][HDMI_V14]hdmi runtime suspend\r\n");
		} else {
			pr_info("[INFO][HDMI_V14]hdmi suspend\r\n");
		}
		if(hdmi_tx_dev->suspend  && !hdmi_tx_dev->runtime_suspend) {
			hdmi_tx_dev->suspend = 0;
			if(hdmi_tx_dev->power_enable_count > 0) {
				suspend_power_enable_count = hdmi_tx_dev->power_enable_count;
				hdmi_tx_dev->power_enable_count = 0;
				tcc_hdmi_power_on(hdmi_tx_dev);
				hdmi_tx_dev->power_enable_count = suspend_power_enable_count;
			}
		}
	}
	return 0;
}


static int hdmi_runtime_suspend(struct device *dev)
{
	struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
	if(hdmi_tx_dev != NULL) {
		hdmi_tx_dev->runtime_suspend = 1;
		hdmi_suspend(dev);
	}
	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
	if(hdmi_tx_dev != NULL) {
		hdmi_tx_dev->runtime_suspend = 0;
		hdmi_resume(dev);
	}
	return 0;
}

#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume = hdmi_runtime_resume,
	.suspend = hdmi_suspend,
	.resume = hdmi_resume,
};
#endif

static int hdmi_parse_dt(struct tcc_hdmi_dev *dev)
{
	int ret = -ENODEV;
	struct device_node *np, *ddibus_np;

	do {
		if(dev == NULL) {
			break;
		}
		np = dev->pdev->of_node;

		/* Register Mapping */
		dev->hdmi_ctrl_io = of_iomap(np, 0);
		if(dev->hdmi_ctrl_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map Controller register base address\n", __func__);
			break;
		}
		dev->hdmi_core_io = of_iomap(np, 1);
		if(dev->hdmi_core_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map HDMI register base address\n", __func__);
			break;
		}

		dev->hdmi_phy_io = of_iomap(np, 2);
		if(dev->hdmi_phy_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map HDMI PHY configuration register base address\n", __func__);
			break;
		}

		dev->hdmi_hdcp_io = of_iomap(np, 3);
		if(dev->hdmi_hdcp_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map HDCP register base address\n", __func__);
			break;
		}
		dev->hdmi_rgb_io = of_iomap(np, 4);
		if(dev->hdmi_rgb_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map RGB register base address\n", __func__);
			break;
		}
		dev->hdmi_aes_io = of_iomap(np, 5);
		if(dev->hdmi_aes_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map AES register base address\n", __func__);
			break;
		}

		dev->hdmi_spdif_io = of_iomap(np, 6);
		if(dev->hdmi_spdif_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map SPDIF Receiver register base address \n", __func__);
			break;
		}

		dev->hdmi_i2s_io = of_iomap(np, 7);
		if(dev->hdmi_i2s_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map I2S Receiver register base address \n", __func__);
			break;
		}

		//pr_info("[INFO][HDMI_V14]%s %p %p %p\r\n", __func__, dev->hdmi_ctrl_io, dev->hdmi_core_io, dev->hdmi_phy_io);

		ddibus_np = of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
		if(ddibus_np == NULL) {
			pr_err("[ERROR][HDMI_V14]%s:Unable to map ddibus resource\n", __func__);
			break;
		}

		// Map DDI_Bus interface
		dev->ddibus_io = of_iomap(ddibus_np, 0);
		if(dev->ddibus_io == NULL){
			pr_err("[ERROR][HDMI_V14]%s:Unable to map ddibus resource\n", __func__);
			break;
		}

		ret = 0;
	}while(0);
	return ret;
}

#if defined(CONFIG_PLATFORM_AVN)
static void send_hdmi_output_event(struct work_struct *work)
{
	char *u_events[2];
	char u_event_name[16];
	struct tcc_hdmi_dev *dev = container_of(work, struct tcc_hdmi_dev, hdmi_output_event_work);
	if(dev != NULL) {
		snprintf(u_event_name, sizeof(u_event_name), "SWITCH_STATE=%d", test_bit(HDMI_SWITCH_STATUS_ON, &dev->status)?1:0);
		u_events[0] = u_event_name;
		u_events[1] = NULL;
		pr_info("[INFO][HDMI_V14]%s u_event(%s)\r\n", __func__, u_event_name);
		kobject_uevent_env(&dev->pdev->kobj, KOBJ_CHANGE, u_events);
	}
}
#endif


/**
 *      tccfb_blank
 *      @blank_mode: the blank mode we want.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *      Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *      blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *      video mode which doesn't support it. Implements VESA suspend
 *      and powerdown modes on hardware that supports disabling hsync/vsync:
 *      blank_mode == 2: suspend vsync
 *      blank_mode == 3: suspend hsync
 *      blank_mode == 4: powerdown
 *
 *      Returns negative errno on error, or zero on success.
 *
 */
static int hdmi_blank(struct tcc_hdmi_dev *dev, int blank_mode)
{
	int ret = -EINVAL;
	struct device *pdev = NULL;

	pr_info("[INFO][HDMI_V14]%s : blank(mode=%d)\n",__func__, blank_mode);

	if(dev != NULL) {
		pdev = dev->pdev;
	}

	if(pdev != NULL) {
		#ifdef CONFIG_PM
		switch(blank_mode) {
			case FB_BLANK_POWERDOWN:
			case FB_BLANK_NORMAL:
				pm_runtime_put_sync(pdev);
				ret = 0;
				break;
			case FB_BLANK_UNBLANK:
				if(pdev->power.usage_count.counter == 1) {
				/*
				 * usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK ) means that
				 * this driver is stable state when booting. don't call runtime_suspend or resume state  */
				} else {
					pm_runtime_get_sync(dev->pdev);
				}
				ret = 0;
				break;
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_VSYNC_SUSPEND:
				ret = 0;
				break;
			default:
				ret = -EINVAL;
				break;
		}
		#endif
	}
	return ret;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	struct miscdevice *misc = (struct miscdevice *)(file!=NULL)?file->private_data:NULL;
	struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)(misc!=NULL)?dev_get_drvdata(misc->parent):NULL;


	if(dev != NULL) {
		file->private_data = dev;

		if(dev != NULL) {
			dev->open_cnt++;
			dlog("%s (%d)\n", __func__, dev->open_cnt);
		}
		ret = 0;
	}

	return ret;
}

static int hdmi_release(struct inode *inode, struct file *file)
{
	int ret = -1;
	struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)(file!=NULL)?file->private_data:NULL;
	if(dev != NULL) {
		if(dev->open_cnt > 0) {
			dev->open_cnt--;
			dlog("%s (%d)\n", __func__, dev->open_cnt);
		}
		ret = 0;
	}
	return ret;
}

static ssize_t hdmi_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

static ssize_t hdmi_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

static unsigned int hdmi_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;
	struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)(file!=NULL)?file->private_data:NULL;

	if(dev != NULL) {
		switch (cmd) {
			case HDMI_IOC_GET_PWR_STATUS:
				{
					io_dlog("HDMI: ioctl(HDMI_IOC_GET_PWR_STATUS : %d )\n", dev->power_status);
					if(copy_to_user((void __user *)arg, &dev->power_status, sizeof(int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_SET_PWR_CONTROL:
				{

					int cmd;
					if(copy_from_user(&cmd, (void __user *)arg, sizeof(int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					io_dlog("HDMI: ioctl(HDMI_IOC_SET_PWR_CONTROL :  %d )\n", cmd);
					switch(cmd) {
						case 0:
							tcc_hdmi_power_off(dev);
							break;
						case 1:
							tcc_hdmi_power_on(dev);
							break;
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_SET_PWR_V5_CONTROL:
				{

					unsigned int cmd;
					if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					io_dlog("HDMI: ioctl(HDMI_IOC_SET_PWR_V5_CONTROL :  %d )\n", cmd);
					ret = 0;
				}
				break;

			case HDMI_IOC_GET_SUSPEND_STATUS:
				{
					if (dev->os_suspend) {
						if(copy_to_user((void __user *)arg, &dev->os_suspend, sizeof(int))) {
							pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
							break;
						}
						dev->os_suspend = 0;
					} else {
						if(copy_to_user((void __user *)arg, &dev->os_suspend, sizeof(int))) {
							pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
							break;
						}
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_GET_HDMISTART_STATUS:
				{
					if(copy_to_user((void __user *)arg, &dev->hdmi_started, sizeof(int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_SET_BLUESCREEN:
				{
					unsigned char val;
					io_dlog("HDMI: ioctl(HDMI_IOC_SET_BLUESCREEN)\n");
					if(copy_from_user(&val, (void __user *)arg, sizeof(unsigned char))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					hdmi_enable_bluescreen(dev, (val > 0)?1:0);
					ret = 0;
				}
				break;


			case HDMI_IOC_SET_AVMUTE:
				{
					int val;
					io_dlog("HDMI: ioctl(HDMI_IOC_SET_AVMUTE)\n");
					if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					if(!dev->suspend) {
						if(dev->power_status) {
							ret = hdmi_av_mute(dev, val);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
				}
				break;

			case HDMI_IOC_SET_SPEAKER_ALLOCATION:
				{
					unsigned int val;

					io_dlog("HDMI: ioctl(HDMI_IOC_SET_SPEAKER_ALLOCATION)\n");
					if(copy_from_user(&val, (void __user *)arg, sizeof(unsigned int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					if(!dev->suspend) {
						if(dev->power_status) {
							hdmi_reg_write(dev, val,HDMI_AUI_BYTE4);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_GET_SPEAKER_ALLOCATION:
				{
					unsigned int val = 0;

					io_dlog("HDMI: ioctl(HDMI_IOC_GET_SPEAKER_ALLOCATION)\n");
					if(!dev->suspend) {
						if(dev->power_status) {
							val = hdmi_reg_read(dev, HDMI_AUI_BYTE4);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}

					if(copy_to_user((void __user *)arg, &val, sizeof(unsigned int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_GET_PHYREADY:
				{
					unsigned char phy_status = 0;

					if(!dev->suspend) {
						if(dev->power_status) {
							phy_status = hdmi_reg_read(dev, HDMI_SS_PHY_STATUS_0);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}

					if(copy_to_user((void __user *)arg, &phy_status, sizeof(unsigned char))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					ret = 0;
				}
				break;

			 case HDMI_IOC_SET_VIDEO_CONFIG:
				{
					videoParams_t videoParam;

					io_dlog("HDMI: ioctl(HDMI_IOC_SET_VIDEO_CONFIG)\n");

					if(copy_from_user(&videoParam, (void __user *)arg, sizeof(videoParams_t))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					if(!dev->suspend) {
						if(dev->power_status) {
							memcpy(&dev->videoParam, &videoParam, sizeof(videoParams_t));
							hdmi_video_config(dev);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}

					ret = 0;
				}
				break;

			case HDMI_IOC_SET_AUDIO_CONFIG:
				{
					audioParams_t audioParams;

					io_dlog("HDMI: ioctl(HDMI_IOC_SET_AUDIO_CONFIG)\n");

					if(copy_from_user(&audioParams, (void __user *)arg, sizeof(audioParams_t))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					memcpy(&dev->audioParam, &audioParams, sizeof(audioParams_t));
					if(!dev->suspend) {
						if(dev->power_status) {
							if(hdmi_audio_set_mode(dev) < 0) {
								pr_err("[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
								break;
							}
							ret = hdmi_audio_set_enable(dev, 1);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
				}
				break;

			case HDMI_IOC_START_HDMI:
				{
					io_dlog("HDMI: ioctl(HDMI_IOC_START_HDMI)\n");

					if(!dev->suspend) {
						if(dev->power_status) {
							hdmi_start_internal(dev);
							#if defined(CONFIG_PLATFORM_AVN)
							set_bit(HDMI_SWITCH_STATUS_ON, &dev->status);
							schedule_work(&dev->hdmi_output_event_work);
							#endif
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}

					ret = 0;
				}
				break;

			case HDMI_IOC_STOP_HDMI:
				{
					io_dlog("HDMI: ioctl(HDMI_IOC_STOP_HDMI)\n");

					#if defined(CONFIG_PLATFORM_AVN)
					clear_bit(HDMI_SWITCH_STATUS_ON, &dev->status);
					schedule_work(&dev->hdmi_output_event_work);
					#endif
					if(!dev->suspend) {
						if(dev->power_status) {
							hdmi_stop_internal(dev);
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_SET_PHY_FREQ:
				{
					struct hdmi_phy_params phy_params;
					io_dlog("HDMI: ioctl(HDMI_IOC_SET_PHY_FREQ)\n");
					if(copy_from_user(&phy_params, (void __user *)arg, sizeof(struct hdmi_phy_params))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					if(!dev->suspend) {
						if(dev->power_status) {
							if ( tcc_hdmi_phy_config(dev,
								phy_params.pixel_clock, phy_params.bpp) < 0) {
								pr_err("[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
								break;
							}
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_BLANK:
				{
					unsigned int cmd;
					if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					ret = hdmi_blank(dev, cmd);
				}
				break;

			case HDMI_IOC_GET_DTD_FILL:
				{
					struct hdmi_dtd_params dtd_params;
					if(copy_from_user(&dtd_params, (void __user *)arg, sizeof(dtd_params))){
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					ret = hdmi_dtd_fill(&dtd_params.detailed_timing, dtd_params.code, dtd_params.refreshRate);

					if(ret == 0 ) {
						if(copy_to_user((void __user *)arg, &dtd_params, sizeof(dtd_params))) {
							ret = -ENODEV;
							pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
							break;
						}
					}
				}
				break;

			case HDMI_IOC_GET_DTD_DISPLAY_PARAM:
				{
					struct hdmi_dtd_display_params dtd_display_params;
					if(copy_from_user(&dtd_display_params,
						(void __user *)arg, sizeof(dtd_display_params))){
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					ret = hdmi_dtd_get_display_param(
						&dtd_display_params.videoParam,
						&dtd_display_params.lcdc_timimg_parm);

					if(ret == 0 ) {
						if(copy_to_user((void __user *)arg,
							&dtd_display_params, sizeof(dtd_display_params))) {
							ret = -ENODEV;
							pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
							break;
						}
					}
				}
				break;

			case HDMI_IOC_GET_DTD_EXTRA_DATA:
				{
					struct hdmi_dtd_extra_data_params extra_data_params;
					if(copy_from_user(&extra_data_params, (void __user *)arg, sizeof(extra_data_params))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}

					ret = hdmi_dtb_get_extra_data(
						&extra_data_params.videoParam,
						&extra_data_params.fb_extra_data);

					if(ret == 0 ) {
						if(copy_to_user((void __user *)arg, &extra_data_params, sizeof(extra_data_params))) {
							ret = -ENODEV;
							pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
							break;
						}
					}
				}
				break;

			case HDMI_IOC_SET_SPD_INFOFRAME:
				{
					spd_packet_t spd_packet;
					if(copy_from_user(&spd_packet, (void __user *)arg, sizeof(spd_packet_t))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					if(!dev->suspend) {
						if(dev->power_status) {
							memcpy(&dev->packetParam.spd, &spd_packet, sizeof(spd_packet_t));
							if ( hdmi_set_spd_infoframe(dev) < 0) {
								pr_err("[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
								break;
							}
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
					ret = 0;
				}
				break;

			case HDMI_IOC_SET_VSIF_INFOFRAME:
				{
					vendor_packet_t vendor_packet;
					if(copy_from_user(&vendor_packet, (void __user *)arg, sizeof(vendor_packet_t))) {
						pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
						break;
					}
					if(!dev->suspend) {
						if(dev->power_status) {
							memcpy(&dev->packetParam.vsif, &vendor_packet, sizeof(vendor_packet_t));
							if ( hdmi_set_vsif_infoframe(dev) < 0) {
								pr_err("[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
								break;
							}
						} else {
							pr_err("[ERROR][HDMI_V14]%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
						}
					} else {
						pr_err("[ERROR][HDMI_V14]%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
					}
					ret = 0;
				}
				break;

			default:
				break;
		}
	}

	return ret;
}

static const struct file_operations hdmi_fops =
{
	.owner          = THIS_MODULE,
	.open           = hdmi_open,
	.release        = hdmi_release,
	.read           = hdmi_read,
	.write          = hdmi_write,
	.unlocked_ioctl = hdmi_ioctl,
	.poll           = hdmi_poll,
};

static int hdmi_remove(struct platform_device *pdev)
{

	struct tcc_hdmi_dev *dev = NULL;

	if(pdev != NULL) {
		dev = (struct tcc_hdmi_dev *)dev_get_drvdata(pdev->dev.parent);

		if(dev != NULL) {
			/* Deinit Interrupt */
			if(dev->misc != NULL) {
				misc_deregister(dev->misc);
				devm_kfree(dev->pdev, dev->misc);
			}
		}
		proc_interface_remove(dev);
		devm_kfree(dev->pdev, dev);

		#ifdef CONFIG_PM
		pm_runtime_disable(dev->pdev);
		#endif
	}
	return 0;

}

static int hdmi_probe(struct platform_device *pdev)
{
	int ret =  -ENODEV;

	struct tcc_hdmi_dev *dev;
	struct device_node *np_spdif;

	pr_info("[INFO][HDMI_V14]****************************************\n");
	pr_info("[INFO][HDMI_V14]%s:HDMI driver %s\n", __func__, SRC_VERSION);
	pr_info("[INFO][HDMI_V14]****************************************\n");

	dev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hdmi_dev), GFP_KERNEL);
	do {
		if (dev == NULL) {
			ret = -ENOMEM;
			break;
		}

		dev->pdev = &pdev->dev;

		if(hdmi_parse_dt(dev) < 0) {
			break;
		}
		/* HDMI PHY Reference Clock */
		dev->hdmi_ref_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi_ref_clk");

		/* DDIBUS_HDMI */
		dev->hdmi_ddibus_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi_ddibus");

		/* HDMI PHY APB Clock */
		dev->hdmi_phy_apb_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi_phy_apb");

		/* PMU ISO Clock */
		dev->pmu_iso_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi_pmu_iso");

		/* PMU ISO Clock */
		dev->hdmi_audio_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi_audio");

		/* SPDIF */
		if(of_property_read_u32(pdev->dev.of_node, "spdif-audio", &dev->spdif_audio) < 0) {
			pr_err("[ERROR][HDMI_V14]%s can not found spdif-block property\r\n", __func__);
			dev->spdif_audio = -1;
		}

		switch(dev->spdif_audio) {
			case 0:
				pr_info("[INFO][HDMI_V14]%s spdif_block is %d - spdif0\r\n", __func__, dev->spdif_audio);
				np_spdif = of_parse_phandle(pdev->dev.of_node,"spdif-block0", 0);
				break;
			case 3:
				pr_info("[INFO][HDMI_V14]%s spdif_block is %d - spdif1\r\n", __func__, dev->spdif_audio);
				np_spdif = of_parse_phandle(pdev->dev.of_node,"spdif-block1", 0);
				break;
			default:
				dev->spdif_audio = -1;
				np_spdif = NULL;
				pr_info("[INFO][HDMI_V14]%s spdif_block is %d - no spdif\r\n", __func__, dev->spdif_audio);
				break;
		}

		if (np_spdif != NULL) {
 			dev->hdmi_audio_spdif_clk = of_clk_get(np_spdif, 0);
		} else {
			dev->hdmi_audio_spdif_clk = NULL;
		}

		if (IS_ERR_OR_NULL(dev->hdmi_ref_clk)) {
			pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi reference clock\n");
			dev->hdmi_ref_clk = NULL;
			break;
		}

		if (IS_ERR_OR_NULL(dev->hdmi_ddibus_clk)) {
			pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi ddibus clock\n");
			dev->hdmi_ddibus_clk = NULL;
			break;
		}

		if (IS_ERR_OR_NULL(dev->hdmi_phy_apb_clk)) {
			pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi phy apb clock\n");
			dev->hdmi_ddibus_clk = NULL;
			break;
		}

		if (IS_ERR_OR_NULL(dev->pmu_iso_clk)) {
			pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi isol clock\n");
			dev->pmu_iso_clk = NULL;
			break;
		}

		if (IS_ERR_OR_NULL(dev->hdmi_audio_clk)) {
			pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi audio clock\n");
			dev->hdmi_audio_clk = NULL;
			break;
		}

		if (IS_ERR_OR_NULL(dev->hdmi_audio_spdif_clk)) {
			pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi audio spdif clock\n");
			dev->hdmi_audio_spdif_clk = NULL;
			break;
		}

		dev->irq = of_irq_to_resource(pdev->dev.of_node, 0, NULL);
		if (dev->irq < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed get irq for hdmi\r\n", __func__);
			break;
		}

		ret = devm_request_irq(dev->pdev, dev->irq,
			hdmi_irq_handler, IRQF_SHARED, "hdmi-irq", dev);
		if(ret < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed request interrupt for hdmi\r\n", __func__);
			break;
		}

		/** uevent work for dual display */
		#if defined(CONFIG_PLATFORM_AVN)
		INIT_WORK(&dev->hdmi_output_event_work, send_hdmi_output_event);
		#endif

		dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL);
		if(dev->misc == NULL) {
			ret = -ENOMEM;
			break;
		}

		dev->misc->minor = MISC_DYNAMIC_MINOR;
		dev->misc->name = "hdmi";
		dev->misc->fops = &hdmi_fops;
		dev->misc->parent = dev->pdev;
		ret = misc_register(dev->misc);

		if(ret < 0) {
			pr_err("[ERROR][HDMI_V14]%s failed misc_register\r\n", __func__);
			break;
		}

		dev_set_drvdata(dev->pdev, dev);


		proc_interface_init(dev);

		#if defined(CONFIG_REGULATOR)
		/*
		dev->vdd_v5p0 = regulator_get(dev->pdev, "dev->vdd_v5p0");
		if (IS_ERR(dev->vdd_v5p0)) {
			pr_warning("clock_table: failed to obtain dev->vdd_v5p0\n");
			dev->vdd_v5p0 = NULL;
		}

		dev->vdd_hdmi = regulator_get(dev->pdev, "dev->vdd_hdmi");
		if (IS_ERR(dev->vdd_hdmi)) {
			pr_warning("clock_table: failed to obtain dev->vdd_hdmi\n");
			dev->vdd_hdmi = NULL;
		}
		*/
		#endif

		#if defined(CONFIG_TCC_OUTPUT_STARTER)
		tcc_hdmi_power_on(dev);
		#endif

		#ifdef CONFIG_PM
		pm_runtime_set_active(dev->pdev);
		pm_runtime_enable(dev->pdev);
		pm_runtime_get_noresume(dev->pdev);  //increase usage_count
		#endif

		hdmi_api_register_dev(dev);
	} while(0);

	if(ret < 0) {
		if(dev != NULL) {
			devm_kfree(dev->pdev, dev);
		};
	}
	return ret;
}

static struct of_device_id hdmi_of_match[] = {
	{ .compatible = "telechips,tcc897x-hdmi" },
	{}
};
MODULE_DEVICE_TABLE(of, hdmi_of_match);

static struct platform_driver tcc_hdmi = {
	.probe	= hdmi_probe,
	.remove	= hdmi_remove,
	.driver	= {
		.name = "tcc_hdmi",
		.owner = THIS_MODULE,
		#ifdef CONFIG_PM
		.pm = &hdmi_pm_ops,
		#endif
		.of_match_table = of_match_ptr(hdmi_of_match),
	},
};

static __init int hdmi_init(void)
{
	pr_err("%s \r\n", __func__);
	return platform_driver_register(&tcc_hdmi);
}

static __exit void hdmi_exit(void)
{
	platform_driver_unregister(&tcc_hdmi);
}

module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_DESCRIPTION("TCCxxx hdmi driver");
MODULE_LICENSE("GPL");
