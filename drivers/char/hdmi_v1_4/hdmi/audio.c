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
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <asm/mach-types.h>
#include <linux/uaccess.h>


#include <asm/io.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi_1_4_api.h>
#include <hdmi.h>
#include <regs-hdmi.h>

#if defined(CONFIG_ARCH_TCC897X)
#include <linux/clk/tcc.h>
//#include <tcc897x-clks.h>
#include <clk-tcc897x.h>
#endif

#define DPRINTK(args...)
extern int tcc_ckc_set_hdmi_audio_src(unsigned int src_id);

static int tcc_hdmi_audio_set_clock(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
        unsigned int audio_clk_rate;

	do {
		if(dev == NULL) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}
		if(!dev->power_status) {
			break;
		}

		/* i_spdif_clk must have higher frequency than fs * 512 */
		audio_clk_rate = dev->audioParam.mSamplingFrequency * 512;
                switch(dev->audioParam.mInterfaceType) {
                        case I2S:
				if(IS_ERR_OR_NULL(dev->hdmi_audio_clk)) {
					printk(KERN_ERR "[ERROR][HDMI_V14]%s hdmi audio clock is not valid\r\n", __func__);
					break;
				}
				clk_disable_unprepare(dev->hdmi_audio_clk);

                                tcc_ckc_set_hdmi_audio_src(PERI_ADAI0);
                                clk_set_rate(dev->hdmi_audio_clk, audio_clk_rate);
                                clk_prepare_enable(dev->hdmi_audio_clk);
				#if defined(DEBUG_AUDIO_CLOCK)
				printk(KERN_INFO "[INFO][HDMI_V14]%s I2S %dHz(%dHz) - req = %dHz(%dHz) \r\n", __func__,
					clk_get_rate(dev->hdmi_audio_clk) >> 9, clk_get_rate(dev->hdmi_audio_clk),
					dev->audioParam.mSamplingFrequency, audio_clk_rate);
				#endif
				ret = 0;
                                break;

                        case SPDIF:
				if(IS_ERR_OR_NULL(dev->hdmi_audio_spdif_clk)) {
					printk(KERN_ERR "[ERROR][HDMI_V14]%s spdif is not valid\r\n", __func__);
					break;
				}
				clk_set_rate(dev->hdmi_audio_spdif_clk, audio_clk_rate);
				#if defined(DEBUG_AUDIO_CLOCK)
				printk(KERN_INFO "[INFO][HDMI_V14]%s SPDIF CLK %d Hz - req = %dHz \r\n", __func__,
					clk_get_rate(dev->hdmi_audio_spdif_clk) >> 9,
					dev->audioParam.mSamplingFrequency);
				#endif

                                if(IS_ERR_OR_NULL(dev->hdmi_audio_clk)) {
					printk(KERN_ERR "[ERROR][HDMI_V14]%s hdmi audio clock is not valid\r\n", __func__);
					break;
				}
				clk_disable_unprepare(dev->hdmi_audio_clk);
				switch(dev->spdif_audio) {
					case 0:
						tcc_ckc_set_hdmi_audio_src(PERI_SPDIF0);
						ret = 0;
						break;
					case 3:
						tcc_ckc_set_hdmi_audio_src(PERI_SPDIF3);
						ret = 0;
						break;
					default:
						break;
				}
				if(ret < 0) {
					printk(KERN_ERR "[ERROR][HDMI_V14]%s spdif index is not valid\r\n", __func__);
					break;
				}
                                clk_set_rate(dev->hdmi_audio_clk, audio_clk_rate);
                                clk_prepare_enable(dev->hdmi_audio_clk);
				#if defined(DEBUG_AUDIO_CLOCK)
				printk(KERN_INFO "[INFO][HDMI_V14]%s SPDIF %d Hz - req = %dHz \r\n", __func__,
					clk_get_rate(dev->hdmi_audio_clk) >> 9,
					dev->audioParam.mSamplingFrequency);
				#endif
                                break;

			default:
				break;
		}
        }while(0);
	return ret;
}

static int hdmi_audio_reset(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	if(dev != NULL) {
		if(dev->audioParam.mInterfaceType == I2S) {
			hdmi_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_0);
			hdmi_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_1);
			hdmi_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_2);
			hdmi_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_3);
			hdmi_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_4);

			// update CUV
			hdmi_reg_write(dev, 0x01, HDMI_SS_I2S_CH_ST_CON);
		}
		ret = 0;
	}
	return ret;
}

static int hdmi_audio_set_samplingfreq(struct tcc_hdmi_dev *dev)
{
        int ret = -1;
	unsigned int i2s_val, aui_val, acr_n, hdmi_reg;

	do {
		if(dev == NULL) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		ret = 0;
		switch (dev->audioParam.mSamplingFrequency) {
                        case 32000:
                                i2s_val = I2S_CH_ST_3_SF_32KHZ;
				aui_val = HDMI_AUI_SF_SF_32KHZ;
				acr_n  = 4096;
                                break;
                        case 44100:
                                i2s_val = I2S_CH_ST_3_SF_44KHZ;
				aui_val = HDMI_AUI_SF_SF_44KHZ;
				acr_n  = 6272;
                                break;
                        case 88200:
                                i2s_val = I2S_CH_ST_3_SF_88KHZ;
				aui_val = HDMI_AUI_SF_SF_88KHZ;
				acr_n  = 12544;
                                break;
                        case 176000:
                                i2s_val = I2S_CH_ST_3_SF_176KHZ;
				aui_val = HDMI_AUI_SF_SF_176KHZ;
				acr_n  = 25088;
                                break;
                        case 48000:
                                i2s_val = I2S_CH_ST_3_SF_48KHZ;
				aui_val = HDMI_AUI_SF_SF_48KHZ;
				acr_n  = 6144;
                                break;
                        case 96000:
                                i2s_val = I2S_CH_ST_3_SF_96KHZ;
				aui_val = HDMI_AUI_SF_SF_96KHZ;
				acr_n  = 12288;
                                break;
                        case 192000:
                                i2s_val = I2S_CH_ST_3_SF_192KHZ;
				aui_val = HDMI_AUI_SF_SF_192KHZ;
				acr_n  = 24576;
                                break;
                        default:
				ret = -1;
                                break;
                }

		if(ret < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s frequency is not valid\r\n", __func__);
			break;;
		}

		tcc_hdmi_audio_set_clock(dev);
		if(dev->audioParam.mInterfaceType == I2S) {
	                hdmi_reg = hdmi_reg_read(dev, HDMI_SS_I2S_CH_ST_3);
	                hdmi_reg &= ~I2S_CH_ST_3_SF_MASK;
			hdmi_reg |= i2s_val;
			//mdelay(10);
			hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CH_ST_3);
		}

		/* Set ACR packet */
		hdmi_reg = acr_n & 0xFF;
		hdmi_reg_write(dev, hdmi_reg, HDMI_ACR_N0);
        	hdmi_reg = (acr_n >>8) & 0xff;
        	hdmi_reg_write(dev, hdmi_reg, HDMI_ACR_N1);
        	hdmi_reg = (acr_n >>16) & 0xff;
        	hdmi_reg_write(dev, hdmi_reg, HDMI_ACR_N2);

		/* Set Aui packet */
        	hdmi_reg = hdmi_reg_read(dev, HDMI_AUI_BYTE2);
		hdmi_reg &= ~HDMI_AUI_SF_MASK;
		hdmi_reg |= aui_val;
    		hdmi_reg_write(dev, hdmi_reg, HDMI_AUI_BYTE2);

        }while(0);
        return ret;
}

static int hdmi_audio_set_channel(struct tcc_hdmi_dev *dev)
{
    	int ret = 0;
	unsigned int i2s_val, aui_byte1_val, hdmi_reg;
	unsigned int asp_con_val = 0;
	unsigned int aui_byte4_val = 0;

	if(!dev) {
		pr_err("[ERROR][HDMI_V14] dev is NULL\r\n", __func__);
		ret = -1;
		goto out;
	}
	if(
		dev->audioParam.mChannelAllocation < 2 &&
		dev->audioParam.mChannelAllocation > 8) {
		pr_err(
			"[ERROR][HDMI_V14]%s channel is invalid\r\n",
			__func__);
		ret = -1;
		goto out;
	}
	i2s_val = dev->audioParam.mChannelAllocation << 4;

	/* AUI BYTEx */
	aui_byte1_val = dev->audioParam.mChannelAllocation-1;
	switch (dev->audioParam.mChannelAllocation) {
	case 2:
		/* Nothing to do */
		break;
	case 3:
	case 4:
		asp_con_val = (ASP_LAYOUT_1|ASP_SP_1);
		break;
	case 5:
	case 6:
		aui_byte4_val = 0x0A;
		asp_con_val = (ASP_LAYOUT_1|ASP_SP_1|ASP_SP_2);
		break;
	case 7:
	case 8:
		aui_byte4_val = 0x12;
		asp_con_val = (ASP_LAYOUT_1|ASP_SP_1|ASP_SP_2|ASP_SP_3);
		break;
	default:
		ret = -1;
		break;
	}

	if(ret < 0)
		goto out;

	hdmi_reg = hdmi_reg_read(dev, HDMI_SS_I2S_CH_ST_2);
	hdmi_reg &= ~I2S_CH_ST_2_CHANNEL_MASK;
	hdmi_reg |= i2s_val;

	hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CH_ST_2);

	hdmi_reg = hdmi_reg_read(dev, HDMI_ASP_CON);
	hdmi_reg &= ~(ASP_MODE_MASK|ASP_SP_MASK);
	hdmi_reg |= asp_con_val;

	hdmi_reg_write(dev, aui_byte1_val, HDMI_AUI_BYTE1);
	hdmi_reg_write(dev, aui_byte4_val, HDMI_AUI_BYTE4);
	hdmi_reg_write(dev, hdmi_reg, HDMI_ASP_CON);
	return 0;
out:
        return ret;
}

static int hdmi_audio_set_coding_type(struct tcc_hdmi_dev *dev)
{
        int ret = -1;
        unsigned int coding_val, hdmi_reg;

        do {
		if(dev == NULL) {
		    	printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
		    	break;
	    	}
		ret = 0;
		if(dev->audioParam.mInterfaceType == I2S) {
			switch(dev->audioParam.mCodingType) {
				case LPCM:
					coding_val =  I2S_CH_ST_0_TYPE_LPCM;
					break;
				case NLPCM:
					coding_val =  I2S_CH_ST_0_TYPE_NLPCM;
					break;
				default:
					ret = -1;
					break;
			}
			if(ret < 0) {
				printk(KERN_ERR "[ERROR][HDMI_V14]%s invalid parameter\r\n", __func__);
				break;
			}

	                hdmi_reg = hdmi_reg_read(dev, HDMI_SS_I2S_CH_ST_0);
			hdmi_reg &= ~I2S_CH_ST_0_TYPE_MASK;
			hdmi_reg |= coding_val;

			hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CH_ST_0);
		}
        }while(0);

        return ret;
}

static int hdmi_audio_set_wordlength(struct tcc_hdmi_dev *dev)
{
        int ret = -1;
        unsigned int wordlength_val, hdmi_reg;

        do {
		if(dev == NULL) {
		    	printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
		    	break;
	    	}

		if(dev->audioParam.mCodingType != LPCM ) {
			ret = 0;
			break;
		}
		if(dev->audioParam.mInterfaceType == SPDIF) {
			ret = 0;
			break;
		}

		switch(dev->audioParam.mWordLength) {
                        case 16:
                                wordlength_val = I2S_CH_ST_4_WL_20_16;
                                break;
                        case 17:
				wordlength_val = I2S_CH_ST_4_WL_20_17;
                                break;
                        case 18:
				wordlength_val = I2S_CH_ST_4_WL_20_18;
                                break;
                        case 19:
				wordlength_val = I2S_CH_ST_4_WL_20_19;
                                break;
                        case 20:
				wordlength_val = I2S_CH_ST_4_WL_24_20;
                                break;
                        case 21:
				wordlength_val = I2S_CH_ST_4_WL_24_21;
                                break;
                        case 22:
				wordlength_val = I2S_CH_ST_4_WL_24_22;
                                break;
                        case 23:
				wordlength_val = I2S_CH_ST_4_WL_24_23;
                                break;
                        case 24:
				wordlength_val = I2S_CH_ST_4_WL_24_24;
                                break;
                        default:
                                wordlength_val = 0;
                                break;
                } // switch

		hdmi_reg = hdmi_reg_read(dev, HDMI_SS_I2S_CH_ST_4);
                hdmi_reg &= ~I2S_CH_ST_4_WL_MASK;
		hdmi_reg |= wordlength_val;

		hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CH_ST_4);
		ret = 0;
	}while(0);
	return ret;
}


static int hdmi_audio_set_i2s_config(struct tcc_hdmi_dev *dev)
{
        int ret = -1;
        unsigned int i2s_val;

        do {
		if(dev == NULL) {
		    	printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
		    	break;
	    	}

                ret = 0;
		if(dev->audioParam.mInterfaceType == SPDIF) {
			break;
		}

                switch(dev->audioParam.mI2S_BitPerChannel) {
                        case 16:
                                i2s_val = I2S_CON_DATA_NUM_16;
                                break;

                        case 20:
                                i2s_val = I2S_CON_DATA_NUM_20;
                                break;

                        case 24:
                                i2s_val = I2S_CON_DATA_NUM_24;
                                break;
                        default:
				ret = -1;
                                break;
                }
                if(ret < 0) {
                        printk(KERN_ERR "[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                // LR clock
                switch(dev->audioParam.mI2S_Frame) {
                        case 32:
                                i2s_val |= I2S_CON_BIT_CH_32;
                                break;

                        case 48:
                                i2s_val |= I2S_CON_BIT_CH_48;
                                break;

                        case 64:
                                i2s_val |= I2S_CON_BIT_CH_64;
                                break;
                        default:
                                ret = -1;
                                break;
                }
                if(ret < 0) {
                        printk(KERN_ERR "[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                // format
                switch(dev->audioParam.mI2S_Format) {
                        case 0:
				/* basic format */
                                i2s_val |= I2S_CON_I2S_MODE_BASIC;
                                break;
                        case 1:
                        	/* left justified format */
                                i2s_val |= I2S_CON_I2S_MODE_LEFT_JUSTIFIED;
                                break;

                        case 2:
				/* right justified format */
                                i2s_val |= I2S_CON_I2S_MODE_RIGHT_JUSTIFIED;
                                break;
                        default:
                                ret = -1;
                                break;
                }
                if(ret < 0) {
                        printk(KERN_ERR "[ERROR][HDMI_V14]%s invalid param at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                hdmi_reg_write(dev, i2s_val , HDMI_SS_I2S_CON_2);
        }while(0);

    return ret;

}

static int hdmi_audio_set_interface(	struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	unsigned int hdmi_reg;

	do {
		if(dev == NULL) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		ret = 0;
                switch (dev->audioParam.mInterfaceType) {
                        case I2S:
                                hdmi_reg = hdmi_reg_read(dev, HDMI_SS_INTC_CON);
                                hdmi_reg &= ~(1<<HDMI_IRQ_SPDIF);

                                hdmi_reg_write(dev, hdmi_reg, HDMI_SS_INTC_CON);

                                // disable DSD
                                hdmi_reg = I2S_DSD_CON_DISABLE;
                                hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_DSD_CON);

                                // I2S control
                                hdmi_reg = I2S_CON_SC_POL_FALLING | I2S_CON_CH_POL_LOW;
                                hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CON_1);

                                // I2S MUX Control
                                hdmi_reg = I2S_IN_MUX_ENABLE |
                                		I2S_IN_MUX_CUV_ENABLE |
                                		I2S_IN_MUX_SELECT_I2S |
                                		I2S_IN_MUX_IN_ENABLE;
                                hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_IN_MUX_CON);

                                // enable all channels
                                hdmi_reg = I2S_MUX_CH_ALL_ENABLE;
                                hdmi_reg_write(dev, hdmi_reg , HDMI_SS_I2S_MUX_CH);

                                // enable CUV from right and left channel
                                hdmi_reg = I2S_MUX_CUV_LEFT_ENABLE| I2S_MUX_CUV_RIGHT_ENABLE;
                                hdmi_reg_write(dev,  hdmi_reg, HDMI_SS_I2S_MUX_CUV);
                                break;

                        case SPDIF:
                                {
                                        // set mux control
                                        hdmi_reg = I2S_IN_MUX_SELECT_SPDIF | I2S_IN_MUX_ENABLE;
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_IN_MUX_CON);

                                        // enable all channels
                                        hdmi_reg = I2S_MUX_CH_ALL_ENABLE;
                                        hdmi_reg_write(dev,  hdmi_reg, HDMI_SS_I2S_MUX_CH);

                                        // enable CUV from right and left channel
                                        hdmi_reg = I2S_MUX_CUV_LEFT_ENABLE| I2S_MUX_CUV_RIGHT_ENABLE;
                                        hdmi_reg_write(dev,  hdmi_reg, HDMI_SS_I2S_MUX_CUV);

					hdmi_reg = 0;
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_CLK_CTRL);

					hdmi_reg = SPDIF_CLK_CTRL_ENABLE;
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_CLK_CTRL);

					hdmi_reg = SPDIF_CONFIG_1_NOISE_FILTER_3_SAMPLES |
							SPDIF_CONFIG_1_UVCP_ENABLE |
							SPDIF_CONFIG_1_WORD_LENGTH_MANUAL |
							SPDIF_CONFIG_1_ALIGN_32BIT |
							SPDIF_CONFIG_1_HDMI_2_BURST;
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_CONFIG_1);

                                        // max 24bits
                                        hdmi_reg = 0xB;
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_USER_VALUE_1);

                                        if (dev->audioParam.mCodingType == NLPCM) {
						hdmi_reg = 0xFF;
                                        } else {
						hdmi_reg = SPDIF_BUFFER_OVERFLOW_MASK |
							SPDIF_PREAMBLE_DETECTED_MASK |
							SPDIF_STATUS_RECOVERED_MASK |
							SPDIF_CLK_RECOVERY_FAIL_MASK;
                                        }
					hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_IRQ_MASK);

                                        // enable SPDIF INT
                                        hdmi_reg = hdmi_reg_read(dev, HDMI_SS_INTC_CON);
                                        hdmi_reg |= (1<<HDMI_IRQ_SPDIF) | (1<<HDMI_IRQ_GLOBAL);
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_INTC_CON);

                                        // start to detect signal
                                        hdmi_reg = SPDIF_SIGNAL_DETECT;
                                        hdmi_reg_write(dev, hdmi_reg, HDMI_SS_SPDIF_OP_CTRL);
                                }
                                break;
                        default:
				ret = -1;
                                break;
                }
        }while(0);

        return ret;
}

static int hdmi_audio_set_packet_type(struct tcc_hdmi_dev *dev)
{
	int ret = -1;
	unsigned int asp_val, hdmi_reg;

	do {
		if(dev == NULL) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		ret = 0;
		switch (dev->audioParam.mPacketType){
                        case AUDIO_SAMPLE:
				/* Clear Sampling frequency in AUI */
				hdmi_reg = hdmi_reg_read(dev, HDMI_AUI_BYTE2);
				hdmi_reg &= ~HDMI_AUI_SF_MASK;
				hdmi_reg_write(dev, hdmi_reg, HDMI_AUI_BYTE2);

                                asp_val = ASP_LPCM_TYPE;
                                break;

                        case HBR_STREAM:
                                hdmi_reg = hdmi_reg_read(dev, HDMI_SS_I2S_CH_ST_3);
				hdmi_reg &= ~I2S_CH_ST_3_SF_MASK;
                                hdmi_reg |= I2S_CH_ST_3_SF_768KHZ;
                                hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CH_ST_3);

                                asp_val = ASP_HBR_TYPE;
                                break;

                        default:
				ret = -1;
                                break;
                }

		hdmi_reg = hdmi_reg_read(dev, HDMI_ASP_CON);
                hdmi_reg &= ~ASP_TYPE_MASK;
		hdmi_reg |= asp_val;
		hdmi_reg_write(dev, hdmi_reg,HDMI_ASP_CON);

		ret = 0;
	} while(0);

	return ret;
}

int hdmi_audio_set_mode(struct tcc_hdmi_dev *dev)
{
	int ret = -1;

	do {
		if(dev == NULL) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		hdmi_audio_reset(dev);

		if(hdmi_audio_set_samplingfreq(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(hdmi_audio_set_channel(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(hdmi_audio_set_coding_type(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(hdmi_audio_set_wordlength(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(hdmi_audio_set_i2s_config(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(hdmi_audio_set_interface(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(hdmi_audio_set_packet_type(dev) < 0) {
			printk(KERN_ERR "[ERROR][HDMI_V14]%s failed at line(%d)\r\n", __func__, __LINE__);
			break;
		}

		if(dev->audioParam.mInterfaceType == I2S) {
			/* update the shadow channel status registers
			   with the values updated in I2S_CH_ST_0 ~ I2S_CH_ST_4 */
			hdmi_reg_write(dev, 0x01, HDMI_SS_I2S_CH_ST_CON);
		}
		ret = 0;
	} while(0);
	return ret;
}

int hdmi_audio_set_enable(struct tcc_hdmi_dev *dev, int enable)
{
	int ret = -1;
	unsigned int hdmi_reg;

	do {
		if(dev == NULL) {
			break;
		}

 		if(dev->videoParam.mHdmi == DVI) {
			printk(KERN_INFO "[INFO][HDMI_V14]%s HDMI output mode is DVI - SKIP\r\n", __func__);
			enable = 0;
		}

		hdmi_reg = hdmi_reg_read(dev, HDMI_CON_0);

		if(enable) {
		        hdmi_aui_update_checksum(dev);

		        hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_AUI_CON);
		        hdmi_reg_write(dev, ACR_MEASURED_CTS_MODE, HDMI_ACR_CON);
			hdmi_reg |= HDMI_ASP_ENABLE;
		        hdmi_reg_write(dev, hdmi_reg, HDMI_CON_0);
			hdmi_reg = I2S_CLK_CON_ENABLE;
		} else {
		        // disable encryption
		        hdmi_reg_write(dev, DO_NOT_TRANSMIT, HDMI_AUI_CON);
		        hdmi_reg_write(dev, DO_NOT_TRANSMIT, HDMI_ACR_CON);
			hdmi_reg &= ~HDMI_ASP_ENABLE;
		        hdmi_reg_write(dev, hdmi_reg, HDMI_CON_0);
			hdmi_reg = I2S_CLK_CON_DISABLE;
		}

		hdmi_reg_write(dev, hdmi_reg, HDMI_SS_I2S_CLK_CON);
		ret = 0;
	}while(0);
	return ret;
}

