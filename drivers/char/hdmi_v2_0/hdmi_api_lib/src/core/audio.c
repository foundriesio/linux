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

#include <core/audio.h>
#include <core/fc_audio.h>
#include <core/fc_audio_info.h>
#include <core/audio_i2s.h>
#include <core/audio_spdif.h>
#include <core/main_controller.h>

#include <hdmi_api_lib/src/core/audio/audio_packetizer_reg.h>

#include <linux/clk.h>
#include <dt-bindings/clock/tcc898x-clks.h>

typedef struct audio_n_computation {
	u32 pixel_clock;
	u32 n;
	u32 cts;
}audio_n_computation_t;

audio_n_computation_t n_values_32kHz_24bit[] = {
	/** for 24bits/Pixel **/
	{0,      4096,  0     },
	{25175,  4576,  28125 },
	{25200,  4096,  25200 },
	{27000,  4096,  27000 },
	{27027,  4096,  27027 },
	{54000,  4096,  54000 },
	{54054,  4096,  54054 },
	{74176,  11648, 210937},
	{74250,  4096,  74250 },
	{148352, 11648, 421875},
	{148500, 4096,  148500},
	{296703, 5824,  421875},
	{297000, 3072,  222750},
	{593406, 5824,  843750},
	{594000, 3072,  445500},
	{0,      0,     0     }
};

audio_n_computation_t n_values_32kHz_30bit[] = {
	/** for 30bits/Pixel **/
	{0,      4096,  0     },
	{31468,  9152,  70312 },
	{31500,  4096,  31500 },
	{33750,  4096,  33750 },
	{33783,  8192,  67567 },
	{67500,  4096,  67500 },
	{67567,  8192,  135135},
	{92719,  11648, 263671},
	{92812,  8192,  185625},
	{185439, 11648, 527343},
	{185625, 4096,  185625},
	{370879, 5824,  527343},
	{371250, 6144,  556875},
	{0,      0,     0     }
};

audio_n_computation_t n_values_32kHz_36bit[] = {
	/** for 36bits/Pixel **/
	{0,      4096,  0     },
	{37762,  9152,  84375 },
	{37800,  4096,  37800 },
	{40500,  4096,  40500 },
	{40540,  8192,  81081 },
	{81000,  4096,  81000 },
	{81081,  4096,  81081 },
	{111263, 11648, 316406},
	{111375, 4096,  111375},
	{222527, 11648, 632812},
	{222750, 4096,  222750},
	{445054, 5824,  632812},
	{445500, 4096,  445500},
	{0,      0,     0     }
};

audio_n_computation_t n_values_44p1kHz_24bit[] = {
	{0,      6272,  0     },
	{25175,  7007,  31250 },
	{25200,  6272,  28000 },
	{27000,  6272,  30000 },
	{27027,  6272,  30030 },
	{54000,  6272,  60000 },
	{54054,  6272,  60060 },
	{74176,  17836, 234375},
	{74250,  6272,  82500 },
	{148352, 8918,  234375},
	{148500, 6272,  165000},
	{296703, 4459,  234375},
	{297000, 4704,  247500},
	{593406, 8918,  937500},
	{594000, 9408,  990000},
	{0,      0,     0     }
};

audio_n_computation_t n_values_44p1kHz_30bit[] = {
	/** for 30bits/Pixel **/
	{0,      6272,  0     },
	{31468,  14014, 78125 },
	{31500,  6272,  35000 },
	{33750,  6272,  37500 },
	{33783,  12544, 75075 },
	{67500,  6272,  75000 },
	{67567,  6272,  75075 },
	{92719,  17836, 292968},
	{92812,  6272,  103125},
	{185439, 17836, 585937},
	{185625, 6272,  206250},
	{370879, 8918,  585937},
	{371250, 4704,  309375},
	{0,      0,     0     }
};

audio_n_computation_t n_values_44p1kHz_36bit[] = {
	/** for 36bits/Pixel **/
	{0,      6272,  0     },
	{37762,  7007,  46875 },
	{37800,  6272,  42000 },
	{40500,  6272,  45000 },
	{40540,  6272,  45045 },
	{81000,  6272,  90000 },
	{81081,  6272,  90090 },
	{111263, 17836, 351562},
	{111375, 6272,  123750},
	{222527, 17836, 703125},
	{222750, 6272,  247500},
	{445054, 8918,  703125},
	{445500, 4704,  371250},
	{0,      0,     0     }
};

audio_n_computation_t n_values_48kHz_24bit[] = {
	{0,      6144,  0     },
	{25175,  6864,  28125 },
	{25200,  6144,  25200 },
	{27000,  6144,  27000 },
	{27027,  6144,  27027 },
	{54000,  6144,  54000 },
	{54054,  6144,  54054 },
	{74176,  11648, 140625},
	{74250,  6144,  74250 },
	{148352, 5824,  140625},
	{148500, 6144,  148500},
	{296703, 5824,  281250},
	{297000, 5120,  247500},
	{593406, 5824,  562500},
	{594000, 6144,  594000},
	{0,      0,     0     }
};

audio_n_computation_t n_values_48kHz_30bit[] = {
	/** for 30bits/Pixel **/
	{0,      6144,  0     },
	{31468,  9152,  46875 },
	{31500,  6144,  31500 },
	{33750,  6144,  33750 },
	{33783,  8192,  45045 },
	{67500,  6144,  67500 },
	{67567,  8192,  90090 },
	{92719,  11648, 175781},
	{92812,  12288, 185625},
	{185439, 11648, 351562},
	{185625, 6144,  185625},
	{370879, 11648, 703125},
	{371250, 5120,  309375},
	{0,      0,     0     }
};

audio_n_computation_t n_values_48kHz_36bit[] = {
	/** for 36bits/Pixel **/
	{0,      6144,  0     },
	{37762,  9152,  56250 },
	{37800,  6144,  37800 },
	{40500,  6144,  40500 },
	{40540,  8192,  54054 },
	{81000,  6144,  81000 },
	{81081,  6144,  81081 },
	{111263, 11648, 210937},
	{111375, 6144,  111375},
	{222527, 11648, 421875},
	{222750, 6144,  222750},
	{445054, 5824,  421875},
	{445500, 5120,  371250},
	{0,      0,     0     }
};

/**********************************************
 * Internal functions
 */
void _audio_clock_n(struct hdmi_tx_dev *dev, u32 value)
{
	LOG_TRACE();
	/* 19-bit width */
	hdmi_dev_write_mask(dev, AUD_N3, AUD_N3_NCTS_ATOMIC_WRITE_MASK,1);
	hdmi_dev_write_mask(dev, AUD_N3, AUD_N3_AUDN_MASK, (u8)(value >> 16));
	hdmi_dev_write(dev, AUD_N2, (u8)(value >> 8));
	hdmi_dev_write(dev, AUD_N1, (u8)(value >> 0));


	/* no shift */
	hdmi_dev_write_mask(dev, AUD_CTS3, AUD_CTS3_N_SHIFT_MASK, 0);

}

void _audio_clock_cts(struct hdmi_tx_dev *dev, u32 value)
{
	LOG_TRACE1(value);
	if(value > 0){
		/* 19-bit width */
		hdmi_dev_write_mask(dev, AUD_N3, AUD_N3_NCTS_ATOMIC_WRITE_MASK,1);
		hdmi_dev_write_mask(dev, AUD_CTS3, AUD_CTS3_CTS_MANUAL_MASK, 1);
		hdmi_dev_write_mask(dev, AUD_CTS3, AUD_CTS3_AUDCTS_MASK, (u8)(value >> 16));
		hdmi_dev_write(dev, AUD_CTS2, (u8)(value >> 8));
		hdmi_dev_write(dev, AUD_CTS1, (u8)(value >> 0));
	}
	else{
		/* Set to automatic generation of CTS values */
		hdmi_dev_write_mask(dev, AUD_CTS3, AUD_CTS3_CTS_MANUAL_MASK, 0);
	}
}

void _audio_clock_cts_auto(struct hdmi_tx_dev *dev, u32 value)
{
	LOG_TRACE();
	hdmi_dev_write(dev, AUD_N1, (u8)(value >> 0));
	hdmi_dev_write(dev, AUD_N2, (u8)(value >> 8));
	hdmi_dev_write_mask(dev, AUD_N3, AUD_N3_AUDN_MASK, (u8)(value >> 16));
	hdmi_dev_write_mask(dev, AUD_CTS3, AUD_CTS3_N_SHIFT_MASK, 0);
	hdmi_dev_write_mask(dev, AUD_CTS3, AUD_CTS3_CTS_MANUAL_MASK, 0);
}


void _audio_clock_f(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE();
	hdmi_dev_write_mask(dev, AUD_INPUTCLKFS, AUD_INPUTCLKFS_IFSFACTOR_MASK, value);
}

int configure_i2s(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
	audio_i2s_configure(dev, audio);

	switch (audio->mClockFsFactor) {
	case 64:
		_audio_clock_f(dev, 4);
		break;
	case 128:
		_audio_clock_f(dev, 0);
		break;
	case 256:
		_audio_clock_f(dev, 1);
		break;
	case 512:
		_audio_clock_f(dev, 2);
		break;
	default:
		_audio_clock_f(dev, 0);
		LOGGER(SNPS_ERROR, "%s:Fs Factor [%d] not supported\n", __func__, audio->mClockFsFactor);
		return FALSE;
		break;
	}

	return TRUE;
}

int configure_spdif(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
	audio_spdif_config(dev, audio);

	switch (audio->mClockFsFactor) {
	case 64:
		_audio_clock_f(dev, 0);
		break;
	case 128:
		_audio_clock_f(dev, 0);
		break;
	case 256:
		_audio_clock_f(dev, 0);
		break;
	case 512:
		_audio_clock_f(dev, 2);
		break;
	default:
		_audio_clock_f(dev, 0);
		LOGGER(SNPS_ERROR, "%s:Fs Factor [%d] not supported\n", __func__, audio->mClockFsFactor);
		return FALSE;
		break;
	}

	return TRUE;
}

void tcc_hdmi_audio_select_source(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
	volatile void __iomem *tcc_io_bus = (volatile void __iomem *)dev->io_bus;


	if(dev->hdmi_audio_if_sel_ofst != 0xff) {
	    iowrite32(audio->mIecSourceNumber, (void*)(tcc_io_bus + dev->hdmi_audio_if_sel_ofst));
	}

	if(dev->hdmi_rx_tx_chmux != 0xff && audio->mInterfaceType == SPDIF) {
	    iowrite32(1 << (audio->mIecSourceNumber*8), (void*)(tcc_io_bus + dev->hdmi_rx_tx_chmux));
	}

}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
#warning This device should impliment a tcc_ckc_set_hdmi_audio_src
#else
extern int tcc_ckc_set_hdmi_audio_src(unsigned int src_id);
#endif
void tcc_hdmi_spdif_clock_config(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
    unsigned int clk_rate;

	if(dev->clk[HDMI_CLK_INDEX_SPDIF])
	{
		clk_rate = audio->mSamplingFrequency*audio->mClockFsFactor;
	    clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_SPDIF]);

		if(audio->mIecSourceNumber)
		{
			#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
			#warning This device should impliment a tcc_ckc_set_hdmi_audio_src
			#else
			tcc_ckc_set_hdmi_audio_src(PERI_MSPDIF1);
			#endif
			printk("%s : clock config is spdif1 \n",__func__);
		}
		else
		{
			#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
			#warning This device should impliment a tcc_ckc_set_hdmi_audio_src
			#else
			tcc_ckc_set_hdmi_audio_src(PERI_MSPDIF0);
			#endif
			printk("%s : clock config is spdif0 \n",__func__);
		}

	    clk_set_rate(dev->clk[HDMI_CLK_INDEX_SPDIF],clk_rate);
	    clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_SPDIF]);

		printk("%s: HDMI SPDIF clk =  %dHz\r\n", FUNC_NAME, (int)clk_get_rate(dev->clk[HDMI_CLK_INDEX_SPDIF]));
	}
}


/**********************************************
 * External functions
 */
int audio_Initialize(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();

	// Mask all interrupts
	audio_i2s_interrupt_mask(dev, 0x3);
	audio_spdif_interrupt_mask(dev, 0x3);

	return audio_mute(dev,  1);
}

#define CTS_AUTO 1

int audio_Configure(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
	u32 n = 0, cts = 0;
        unsigned int pixel_clock;

	LOG_TRACE();

	if((dev == NULL) || (audio == NULL)){
		LOGGER(SNPS_ERROR, "Improper function arguments\n");
		return FALSE;
	}


	if(dev->hdmi_tx_ctrl.audio_on == 0){
		LOGGER(SNPS_WARN, "DVI mode selected: audio not configured");
		return TRUE;
	}
        pixel_clock = (dev->hdmi_tx_ctrl.pixel_clock > 1000)?(dev->hdmi_tx_ctrl.pixel_clock / 1000):dev->hdmi_tx_ctrl.pixel_clock;
	LOGGER(SNPS_DEBUG, "Audio interface type = %s\n", audio->mInterfaceType == I2S ? "I2S" :
							audio->mInterfaceType == SPDIF ? "SPDIF" :
							audio->mInterfaceType == HBR ? "HBR" :
							audio->mInterfaceType == GPA ? "GPA" :
							audio->mInterfaceType == DMA ? "DMA" : "---");

	LOGGER(SNPS_DEBUG, "Audio coding = %s\n", audio->mCodingType == PCM ? "PCM" :
						   audio->mCodingType == AC3 ? "AC3" :
						   audio->mCodingType == MPEG1 ? "MPEG1" :
						   audio->mCodingType == MP3 ? "MP3" :
						   audio->mCodingType == MPEG2 ? "MPEG2" :
						   audio->mCodingType == AAC ? "AAC" :
						   audio->mCodingType == DTS ? "DTS" :
						   audio->mCodingType == ATRAC ? "ATRAC" :
						   audio->mCodingType == ONE_BIT_AUDIO ? "ONE BIT AUDIO" :
						   audio->mCodingType == DOLBY_DIGITAL_PLUS ? "DOLBY DIGITAL +" :
						   audio->mCodingType == DTS_HD ? "DTS HD" : "----");
	LOGGER(SNPS_DEBUG, "Audio frequency = %.3d Hz\n", audio->mSamplingFrequency);
	LOGGER(SNPS_DEBUG, "Audio sample size = %d\n", audio->mSampleSize);
	LOGGER(SNPS_DEBUG, "Audio FS factor = %d\n", audio->mClockFsFactor);

	// Set PCUV info from external source
	audio->mGpaInsertPucv = 1;

	audio_mute(dev, 1);

	tcc_hdmi_audio_select_source(dev,audio);

	// Configure Frame Composer audio parameters
	fc_audio_config(dev, audio);

	if(audio->mInterfaceType == I2S){
		if(configure_i2s(dev, audio) == FALSE){
			LOGGER(SNPS_ERROR, "Configuring I2S audio\n");
			return FALSE;
		}
	}
	else if(audio->mInterfaceType == SPDIF){
		if(configure_spdif(dev, audio) == FALSE){
			LOGGER(SNPS_ERROR, "Configuring SPDIF audio\n");
			return FALSE;
		}
	}
	else if(audio->mInterfaceType == HBR){
		LOGGER(SNPS_ERROR, "HBR is not supported\n");
		return FALSE;
	}
	else if (audio->mInterfaceType == GPA) {
		LOGGER(SNPS_ERROR, "GPA is not supported\n");
		return FALSE;
	}

	else if (audio->mInterfaceType == DMA) {
		LOGGER(SNPS_ERROR, "DMA is not supported\n");
		return FALSE;
	}

	cts = audio_ComputeCts(dev, audio->mSamplingFrequency, pixel_clock);
	n = audio_ComputeN(dev, audio->mSamplingFrequency, pixel_clock);
#if CTS_AUTO
	_audio_clock_cts_auto(dev, n);
	printk("<< [%s] : TMDS Ratio(%dKHz), SamplingRate(%dHz), N(%d), CTS(auto) \r\n",
	__func__,pixel_clock,audio->mSamplingFrequency,n);
#else
	_audio_clock_cts(dev, cts);
	_audio_clock_n(dev, n);
	printk("<< [%s] : TMDS Ratio(%dKHz), SamplingRate(%dHz), N(%d), CTS(%d)\r\n",
	__func__,pixel_clock,audio->mSamplingFrequency,n,cts);
#endif
	mc_audio_sampler_clock_enable(dev, 0);

	audio_mute(dev, 0);

	// Configure audio info frame packets
	fc_audio_info_config(dev, audio);

	if(audio->mInterfaceType == SPDIF)
		tcc_hdmi_spdif_clock_config(dev, audio);

	return TRUE;
}

int audio_mute(struct hdmi_tx_dev *dev, u8 state)
{
	/* audio mute priority: AVMUTE, sample flat, validity */
	/* AVMUTE also mutes video */
	// TODO: Check the audio mute process
	if(state){
		fc_audio_mute(dev);
	}
	else{
		fc_audio_unmute(dev);
	}
	return TRUE;
}

u32 audio_ComputeN(struct hdmi_tx_dev *dev, u32  freq, u32 pixelClk)
{
	int i = 0;
	u32 n = 0;
	audio_n_computation_t *n_values = NULL;
	int multiplier_factor = 1;
	videoParams_t *video_parm = (videoParams_t*)dev->videoParam;

	if(freq == 64000 || freq == 88200 || freq == 96000){
		multiplier_factor = 2;
	}
	else if(freq == 128000 || freq == 176400 || freq == 192000){
		multiplier_factor = 4;
	}
	else if(freq == 256000 || freq == 352800 || freq== 384000){
		multiplier_factor = 8;
	}

	if(32000 == freq/multiplier_factor){
		switch(video_parm->mColorResolution) {
			case COLOR_DEPTH_8: default :
				n_values = n_values_32kHz_24bit;
				break;
			case COLOR_DEPTH_10:
				n_values = n_values_32kHz_30bit;
				break;
			case COLOR_DEPTH_12:
				n_values = n_values_32kHz_36bit;
				break;
		}
	}
	else if(44100 == freq/multiplier_factor){
		switch(video_parm->mColorResolution) {
			case COLOR_DEPTH_8: default :
				n_values = n_values_44p1kHz_24bit;
				break;
			case COLOR_DEPTH_10:
				n_values = n_values_44p1kHz_30bit;
				break;
			case COLOR_DEPTH_12:
				n_values = n_values_44p1kHz_36bit;
				break;
		}
	}
	else if(48000 == freq/multiplier_factor){
		switch(video_parm->mColorResolution) {
			case COLOR_DEPTH_8: default :
				n_values = n_values_48kHz_24bit;
				break;
			case COLOR_DEPTH_10:
				n_values = n_values_48kHz_30bit;
				break;
			case COLOR_DEPTH_12:
				n_values = n_values_48kHz_36bit;
				break;
		}
	}
	else {
		LOGGER(SNPS_ERROR, "%s:Could not compute N values\n", __func__);
		LOGGER(SNPS_ERROR, "%s:Audio Frequency %d\n", __func__, freq);
		LOGGER(SNPS_ERROR, "%s:Pixel Clock %d\n", __func__, pixelClk);
		LOGGER(SNPS_ERROR, "%s:Multiplier factor %d\n", __func__, multiplier_factor);
		return FALSE;
	}

	for(i = 0; n_values[i].n != 0; i++){
		if(pixelClk == n_values[i].pixel_clock){
			n = n_values[i].n * multiplier_factor;
			LOGGER(SNPS_DEBUG, "Audio N value = %d\n", n);
			return n;
		}
	}

	n = n_values[0].n * multiplier_factor;

	LOGGER(SNPS_DEBUG, "Audio N value default = %d\n", n);

	return n;
}

u32 audio_ComputeCts(struct hdmi_tx_dev *dev, u32 freq, u32 pixelClk)
{
	int i = 0;
	u32 cts = 0;
	audio_n_computation_t *n_values = NULL;
	videoParams_t *video_parm = (videoParams_t*)dev->videoParam;

	if(freq == 32000||freq == 64000||freq == 128000||freq == 256000){
		switch(video_parm->mColorResolution) {
			case COLOR_DEPTH_8: default :
				n_values = n_values_32kHz_24bit;
				break;
			case COLOR_DEPTH_10:
				n_values = n_values_32kHz_30bit;
				break;
			case COLOR_DEPTH_12:
				n_values = n_values_32kHz_36bit;
				break;
		}
	}
	else if(freq == 44100||freq == 88200||freq == 176400||freq == 352800){
		switch(video_parm->mColorResolution) {
			case COLOR_DEPTH_8: default :
				n_values = n_values_44p1kHz_24bit;
				break;
			case COLOR_DEPTH_10:
				n_values = n_values_44p1kHz_30bit;
				break;
			case COLOR_DEPTH_12:
				n_values = n_values_44p1kHz_36bit;
				break;
		}
	}
	else if(freq == 48000||freq == 96000||freq == 192000||freq == 384000){
		switch(video_parm->mColorResolution) {
			case COLOR_DEPTH_8: default :
				n_values = n_values_48kHz_24bit;
				break;
			case COLOR_DEPTH_10:
				n_values = n_values_48kHz_30bit;
				break;
			case COLOR_DEPTH_12:
				n_values = n_values_48kHz_36bit;
				break;
		}
	}
	else {
		LOGGER(SNPS_ERROR, "%s:Could not compute CTS values\n", __func__);
		LOGGER(SNPS_ERROR, "%s:Audio Frequency %d\n", __func__, freq);
		LOGGER(SNPS_ERROR, "%s:Pixel Clock %d\n", __func__, pixelClk);
		return FALSE;
	}

	for(i = 1; n_values[i].cts != 0; i++){
		if(pixelClk == n_values[i].pixel_clock){
			cts = n_values[i].cts;
			LOGGER(SNPS_DEBUG, "audio_ComputeCtsEquation CTS = %d\n", cts);
			return cts;
		}
	}

	LOGGER(SNPS_DEBUG, "Audio CTS value auto\n");
	return cts;
}
