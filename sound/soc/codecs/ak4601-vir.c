/*
 * ak4601.c  --  audio driver for AK4601
 *
 * Copyright (C) 2016 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Tsuyoshi Mutsuro   16/04/12	    1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>

#include "ak4601-vir.h"


#include <sound/tcc/params/tcc_mbox_am3d_effect_params.h>


//#define AK4601_VIR_DEBUG

#define LOG_TAG    "AK4601_VIRTUAL_CODEC "

#ifdef AK4601_VIR_DEBUG    
#define dprintk(msg...)    printk(LOG_TAG  msg);		
#else
#define dprintk(msg...)    do {} while (0) 			
#endif

#define eprintk(msg...)    printk(KERN_ERR LOG_TAG  msg);


/* AK4601 Codec Private Data */
struct ak4601_priv {
	struct snd_soc_codec *codec;
	struct platform_device *pdev;
	struct mbox_audio_device *mbox_audio_dev;

	//backup control data
	int mic_in_vol[2]; //L/R volume
	int adc_vol[ADC_COUNT][STEREO_COUNT]; //adc1,2 - L/R vol, adcm
	int dac_vol[DAC_COUNT][STEREO_COUNT]; //dac1,2,3 - L/R vol

	bool adc_mute[ADC_COUNT]; //adc1,2, adcm mute state
	bool dac_mute[DAC_COUNT]; //dac1,2,3 mute state

	unsigned int adc_input_voltage_setting[ADC_COUNT][STEREO_COUNT]; //adc1,2 - L/R, adcm

	bool sdout_enable[SDOUT_COUNT];

	unsigned int sdout1_source_selector;
	unsigned int mixerb_ch1_source_selector;
	unsigned int adc2_mux;
    
};

/*************************** Initialize mixer values ******************************/
//TODO :  We should consider a7 send the init and changed values to a53 through mbox
static int ak4601_vir_mixer_init(struct ak4601_priv *ak4601)
{
    int i = 0;
    if (ak4601 == NULL) {
   	    eprintk("%s : ak4601 is null\n", __FUNCTION__);
		return -ENOMEM;
    }
	
    ak4601->mic_in_vol[LEFT_CHANNEL] = ak4601->mic_in_vol[RIGHT_CHANNEL] = 0;

	ak4601->adc_vol[ADC1_INDEX][LEFT_CHANNEL] = 255;
	ak4601->adc_vol[ADC1_INDEX][RIGHT_CHANNEL] = 0;
	ak4601->adc_vol[ADC2_INDEX][LEFT_CHANNEL] = ak4601->adc_vol[ADC2_INDEX][RIGHT_CHANNEL] = 207;
	ak4601->adc_vol[ADCM_INDEX][LEFT_CHANNEL] = ak4601->adc_vol[ADCM_INDEX][RIGHT_CHANNEL] = 207; //adcm-right is dummy

    ak4601->dac_vol[DAC1_INDEX][LEFT_CHANNEL] = ak4601->dac_vol[DAC1_INDEX][RIGHT_CHANNEL] = 231;
	ak4601->dac_vol[DAC2_INDEX][LEFT_CHANNEL] = ak4601->dac_vol[DAC2_INDEX][RIGHT_CHANNEL] = 231;
	ak4601->dac_vol[DAC3_INDEX][LEFT_CHANNEL] = ak4601->dac_vol[DAC3_INDEX][RIGHT_CHANNEL] = 231;
 
	for (i = 0; i< ADC_COUNT; i++) {
		ak4601->adc_mute[i] = AK4601_VIRTUAL_OFF;
		ak4601->adc_input_voltage_setting[i][LEFT_CHANNEL] = AK4601_VIRTUAL_2_3_Vpp;
	    ak4601->adc_input_voltage_setting[i][RIGHT_CHANNEL] = AK4601_VIRTUAL_2_3_Vpp;
	}

	for (i = 0; i< DAC_COUNT; i++) {
		ak4601->dac_mute[i] = AK4601_VIRTUAL_OFF;
	}

	for (i = 0; i< SDOUT_COUNT; i++) {
		ak4601->sdout_enable[i] = AK4601_VIRTUAL_ON;
	}

	ak4601->sdout1_source_selector = AK4601_VIRTUAL_MIXER_B;
	ak4601->mixerb_ch1_source_selector = AK4601_VIRTUAL_ADC1;
	ak4601->adc2_mux = AK4601_VIRTUAL_AIN2;

	return 0;
}

/*************************** Setter/Getter to control mixer ******************************/

#define AK4601_VIR_SINGLE_EXT(xname, data, max, xhandler_get, xhandler_put) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.info = ak4601_vir_single_info, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (max << 8 | (data & 0x00FF)) }

//tmp use reg in soc_enum to find adc & l/r value..
#define AK4601_VIR_SOC_ENUM_SINGLE_EXT(xitems, xtexts, xvalue) \
{	.items = xitems, .texts = xtexts, .reg = xvalue  }

#define AK4601_VIR_SOC_VALUE_ENUM_SINGLE_EXT(xitems, xtexts, xvalues) \
{	.items = xitems, .texts = xtexts, .values = xvalues  }


static const char* ak4601_vir_adc_input_vol_texts[] ={
	"2.3Vpp/plus or minus2.3Vpp",
	"2.83Vpp/plus or minus2.83Vpp",
};
static const struct soc_enum ak4601_vir_adc_input_vol_enum[] ={
	AK4601_VIR_SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_adc_input_vol_texts),ak4601_vir_adc_input_vol_texts, 0x00),//ADC1 L
	AK4601_VIR_SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_adc_input_vol_texts),ak4601_vir_adc_input_vol_texts, 0x01),//ADC1 R
	AK4601_VIR_SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_adc_input_vol_texts),ak4601_vir_adc_input_vol_texts, 0x10),//ADC2 L
	AK4601_VIR_SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_adc_input_vol_texts),ak4601_vir_adc_input_vol_texts, 0x11),//ADC2 R
	AK4601_VIR_SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_adc_input_vol_texts),ak4601_vir_adc_input_vol_texts, 0x20),//ADCM
};

	
static const char* ak4601_vir_source_select_texts[] ={
	"ALL0", 
	"SDIN1",  "SDIN1B", "SDIN1C", "SDIN1D",
	"SDIN1E", "SDIN1F", "SDIN1G", "SDIN1H",
	"SDIN2",  "SDIN3",	"VOL1", "VOL2", "VOL3",
	"ADC1",   "ADC2",	"ADCM", "MixerA", "MixerB",
};
//temp
static const unsigned int ak4601_vir_source_select_values[] = {
	0x00, 
	0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x10, 0x11, 0x12,
	0x15, 0x16, 0x17, 0x18, 0x19, 
};
static const struct soc_enum ak4601_vir_sdout1_source_selector_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_source_select_texts),ak4601_vir_source_select_texts);

static const struct soc_enum ak4601_vir_mixerb_ch1_source_selector_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_source_select_texts),ak4601_vir_source_select_texts);

	
static const char *ak4601_vir_adc2_select_texts[] =
	{"AIN2", "AIN3", "AIN4", "AIN5"};
static const struct soc_enum ak4601_vir_adc2_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_vir_adc2_select_texts),ak4601_vir_adc2_select_texts);



static int ak4601_vir_single_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    unsigned long pv = kcontrol->private_value;
	unsigned int max = 0;
	//unsigned int value = 0;

	max = 0x00FF & (pv >> 8);
	//value = 0x00FF & pv;

    //dprintk("%s : pv = 0x%8x, max = 0x%08x, val = 0x%08x\n", __FUNCTION__, pv, max, value);
    
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max;
	return 0;
}


static int get_mic_input_volume(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	unsigned short value = 0x00FF & (kcontrol->private_value);

	ucontrol->value.integer.value[0] = ak4601->mic_in_vol[value];
	dprintk("%s : done\n", __FUNCTION__);

    return 0;
}

static int set_mic_input_volume(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	unsigned short value = 0x00FF & (kcontrol->private_value);
	unsigned short max = 0x00FF & (kcontrol->private_value >> 8);
	
    if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > max) {
		eprintk("%s : private_value error : exceed data ranges : %ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);
		return -EINVAL;
    }

	ak4601->mic_in_vol[value] = ucontrol->value.integer.value[0];

	header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    msg[0] = (value == 0) ? (unsigned int) AK4601_VIRTUAL_001_MIC_INPUT_VOLUME_L : (unsigned int) AK4601_VIRTUAL_002_MIC_INPUT_VOLUME_R;
	msg[1] = (unsigned int) ak4601->mic_in_vol[value];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d,  msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

	dprintk("%s : done\n", __FUNCTION__);

    return 0;
}

static int get_adc_volume(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	unsigned short adc_val = 0x000F & (kcontrol->private_value >> 4);
	unsigned short lr_val = 0x000F & kcontrol->private_value;

	if (adc_val < ADC_COUNT && lr_val < STEREO_COUNT) {
		ucontrol->value.integer.value[0] = ak4601->adc_vol[adc_val][lr_val];
	} else {
		eprintk("%s : private_value error : adc_val = %d, lr_val = %d\n", __FUNCTION__, adc_val, lr_val);
		return -EINVAL;
	}

	dprintk("%s : adc_val = %u, lr_val = %u, done\n", __FUNCTION__, adc_val, lr_val);

    return 0;
}

static int set_adc_volume(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	unsigned short adc_val = 0x000F & (kcontrol->private_value >> 4);
	unsigned short lr_val = 0x000F & kcontrol->private_value;
    unsigned short max = 0x00FF & (kcontrol->private_value >> 8);
	
    if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > max) {
		eprintk("%s : private_value error : exceed data ranges : %ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);
		return -EINVAL;
    }
	if (adc_val < ADC_COUNT && lr_val < STEREO_COUNT) {
		ak4601->adc_vol[adc_val][lr_val] = ucontrol->value.integer.value[0];
	} else {
		eprintk("%s : private_value error : adc_val = %d, lr_val = %d\n", __FUNCTION__, adc_val, lr_val);
		return -EINVAL;
	}

	
	header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    switch (adc_val) {
	case ADC1_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_003_ADC1_DIGITAL_VOLUME_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_004_ADC1_DIGITAL_VOLUME_R;
		}
		break;
	case ADC2_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_005_ADC2_DIGITAL_VOLUME_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_006_ADC2_DIGITAL_VOLUME_R;
		}
		break;
	case ADCM_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_007_ADCM_DIGITAL_VOLUME;
		break;
    }
	msg[1] = (unsigned int) ak4601->adc_vol[adc_val][lr_val];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

	dprintk("%s : adc_val = %u, lr_val = %u, done\n", __FUNCTION__, adc_val, lr_val);

    return 0;
}

static int get_dac_volume(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	unsigned short dac_val = 0x000F & (kcontrol->private_value >> 4);
	unsigned short lr_val = 0x000F & kcontrol->private_value;

	if (dac_val < DAC_COUNT && lr_val < STEREO_COUNT) {
		ucontrol->value.integer.value[0] = ak4601->dac_vol[dac_val][lr_val];
	} else {
	    eprintk("%s : private_value error : dac_val = %u\n", __FUNCTION__, dac_val);
		return -EINVAL;
	}

	dprintk("%s : dac_val = %u, lr_val = %u, done\n", __FUNCTION__, dac_val, lr_val);

    return 0;
}

static int set_dac_volume(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	unsigned short dac_val = 0x000F & (kcontrol->private_value >> 4);
	unsigned short lr_val = 0x000F & kcontrol->private_value;
    unsigned short max = 0x00FF & (kcontrol->private_value >> 8);
	
    if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > max) {
		eprintk("%s : private_value error : exceed data ranges : %ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);
		return -EINVAL;
    }

	if (dac_val < DAC_COUNT && lr_val < STEREO_COUNT) {
		ak4601->dac_vol[dac_val][lr_val] = ucontrol->value.integer.value[0];
	} else {
	    eprintk("%s : private_value error : dac_val = %u\n", __FUNCTION__, dac_val);
		return -EINVAL;
	}

    header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    switch (dac_val) {
	case DAC1_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_008_DAC1_DIGITAL_VOLUME_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_009_DAC1_DIGITAL_VOLUME_R;
		}
		break;
	case DAC2_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_010_DAC2_DIGITAL_VOLUME_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_011_DAC2_DIGITAL_VOLUME_R;
		}
		break;
	case DAC3_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_012_DAC3_DIGITAL_VOLUME_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_013_DAC3_DIGITAL_VOLUME_R;
		}
		break;
    }

	msg[1] = (unsigned int) ak4601->dac_vol[dac_val][lr_val];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

	dprintk("%s : dac_val = %u, lr_val = %u, done\n", __FUNCTION__, dac_val, lr_val);

    return 0;
}

static int get_adc_mute(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);


	if (kcontrol->private_value < ADC_COUNT) {
		ucontrol->value.integer.value[0] = ak4601->adc_mute[kcontrol->private_value];
	} else {
	    eprintk("%s : error- private_value = %lu\n", __FUNCTION__, kcontrol->private_value);
		return -EINVAL;
	}

	dprintk("%s : value = %ld, done\n", __FUNCTION__, ucontrol->value.integer.value[0]);
   
    return 0;

}

static int set_adc_mute(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	if (kcontrol->private_value < ADC_COUNT) {
		ak4601->adc_mute[kcontrol->private_value] = ucontrol->value.integer.value[0];
	} else {
	    eprintk("%s : error- private_value = %lu\n", __FUNCTION__, kcontrol->private_value);
		return -EINVAL;
	}


    header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    switch (kcontrol->private_value) {
	case ADC1_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_014_ADC1_MUTE;
		break;
	case ADC2_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_015_ADC2_MUTE;
		break;
	case ADCM_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_016_ADCM_MUTE;;
		break;
    }
	msg[1] = (unsigned int) ak4601->adc_mute[kcontrol->private_value];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

	dprintk("%s : value = %ld, done\n", __FUNCTION__, ucontrol->value.integer.value[0]);
   
    return 0;
}

static int get_dac_mute(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	if (kcontrol->private_value < DAC_COUNT) {
		ucontrol->value.integer.value[0] = ak4601->dac_mute[kcontrol->private_value];
	} else {
	    eprintk("%s : error- private_value = %lu\n", __FUNCTION__, kcontrol->private_value);
		return -EINVAL;
	}

	dprintk("%s : value = %ld, done\n", __FUNCTION__, ucontrol->value.integer.value[0]);
   
    return 0;
}

static int set_dac_mute(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	
    struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	if (kcontrol->private_value < DAC_COUNT) {
		ak4601->dac_mute[kcontrol->private_value] = ucontrol->value.integer.value[0];
	} else {
	    eprintk("%s : error- private_value = %lu\n", __FUNCTION__, kcontrol->private_value);
		return -EINVAL;
	}

    header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    switch (kcontrol->private_value) {
	case DAC1_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_017_DAC1_MUTE;
		break;
	case DAC2_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_018_DAC2_MUTE;
		break;
	case DAC3_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_019_DAC3_MUTE;
		break;
    }
	msg[1] = (unsigned int) ak4601->dac_mute[kcontrol->private_value];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

	dprintk("%s : value = %ld, done\n", __FUNCTION__, ucontrol->value.integer.value[0]);
   
    return 0;
}

static int get_sdout_enable(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	if (kcontrol->private_value < SDOUT_COUNT) {
		ucontrol->value.integer.value[0] = ak4601->sdout_enable[kcontrol->private_value];
	} else {
	    eprintk("%s : error- private_value = %lu\n", __FUNCTION__, kcontrol->private_value);
		return -EINVAL;
	}

	dprintk("%s : value = %ld, done\n", __FUNCTION__, ucontrol->value.integer.value[0]);
   
    return 0;
}

static int set_sdout_enable(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	if (kcontrol->private_value < SDOUT_COUNT) {
		ak4601->sdout_enable[kcontrol->private_value] = ucontrol->value.integer.value[0];
	} else {
	    eprintk("%s : error- private_value = %lu\n", __FUNCTION__, kcontrol->private_value);
		return -EINVAL;
	}

    header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    switch (kcontrol->private_value) {
	case SDOUT1_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_025_SDOUT1_OUTPUT_ENABLE;
		break;
	case SDOUT2_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_026_SDOUT2_OUTPUT_ENABLE;
		break;
	case SDOUT3_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_027_SDOUT3_OUTPUT_ENABLE;
		break;
    }

	msg[1] = (unsigned int) ak4601->sdout_enable[kcontrol->private_value];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

	dprintk("%s : value = %ld, done\n", __FUNCTION__, ucontrol->value.integer.value[0]);


    // request test codes
    /*struct mbox_audio_device *audio_dev;
	struct mbox_audio_tx_reply_data_t *reply_data;
	struct mbox_audio_data_header_t *header;
	
	unsigned int msg[6] = {0};
	unsigned int re_msg[6] = {0};
	int index = 0;
	unsigned short cmd_type, msg_size;
	header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);
	reply_data = kzalloc(sizeof(struct mbox_audio_tx_reply_data_t), GFP_KERNEL);
	
	audio_dev = ak4601->mbox_audio_dev;
	dprintk("%s : value = %ld, done 222\n", __FUNCTION__, ucontrol->value.integer.value[0]);
	header->usage = MBOX_AUDIO_USAGE_REQUEST;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = 4;
	//dprintk("%s : value = %ld, done 333\n", __FUNCTION__, ucontrol->value.integer.value[0]);
	msg[0] = 0x11110000;
	msg[1] = 0xFFFF3333;
	msg[2] = 0x2222AAAA;
	msg[3] = 0x54545454;
	//dprintk("%s : value = %ld, done 444\n", __FUNCTION__, ucontrol->value.integer.value[0]);

	dprintk("%s : send MBOX_AUDIO_USAGE_Request++++ 0x11110000\n", __FUNCTION__);
	index = tcc_mbox_audio_send_message(audio_dev, header, msg, reply_data);
	dprintk("%s : send MBOX_AUDIO_USAGE_Request---- \n", __FUNCTION__);
	
    cmd_type = reply_data->cmd_type;
	msg_size = reply_data->msg_size;
	re_msg[0] = reply_data->msg[0];
	dprintk("%s : get message : cmd_type = 0x%04x, msg_size = %d, msg[0] = 0x%08x \n ", __FUNCTION__, cmd_type, msg_size, re_msg[0]);

	kfree(header);*/

    return 0;
}

static int get_adc_input_voltage_setting(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct soc_enum *pv = (struct soc_enum *)kcontrol->private_value;

	unsigned short adc_val = 0x000F & (pv->reg >> 4);
	unsigned short lr_val = 0x000F & pv->reg;

    if (adc_val < ADC_COUNT && lr_val < STEREO_COUNT) {
		ucontrol->value.enumerated.item[0] = ak4601->adc_input_voltage_setting[adc_val][lr_val];
	} else {
		eprintk("%s : private_value error : adc_val = %d, lr_val = %d\n", __FUNCTION__, adc_val, lr_val);
		return -EINVAL;
	}
	

    dprintk("%s : get val = %u, adc_val = %u, lr_val = %u, done\n", __FUNCTION__, ucontrol->value.enumerated.item[0], adc_val, lr_val);
    return 0;
}

static int set_adc_input_voltage_setting(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct soc_enum *pv = (struct soc_enum *)kcontrol->private_value;

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	unsigned short adc_val = 0x000F & (pv->reg >> 4);
	unsigned short lr_val = 0x000F & pv->reg;
	

    if (adc_val < ADC_COUNT && lr_val < STEREO_COUNT) {
		ak4601->adc_input_voltage_setting[adc_val][lr_val] = ucontrol->value.enumerated.item[0];
	} else {
		eprintk("%s : private_value error : adc_val = %d, lr_val = %d\n", __FUNCTION__, adc_val, lr_val);
		return -EINVAL;
	}

    header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    switch (adc_val) {
	case ADC1_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_020_ADC1_INPUT_VOLTAGE_SETTING_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_021_ADC1_INPUT_VOLTAGE_SETTING_R;
		}
		break;
	case ADC2_INDEX :
		if (lr_val == LEFT_CHANNEL) {
			msg[0] = (unsigned int) AK4601_VIRTUAL_022_ADC2_INPUT_VOLTAGE_SETTING_L;
		} else {
			msg[0] = (unsigned int) AK4601_VIRTUAL_023_ADC2_INPUT_VOLTAGE_SETTING_R;
		}
		break;
	case ADCM_INDEX :
		msg[0] = (unsigned int) AK4601_VIRTUAL_024_ADCM_INPUT_VOLTAGE_SETTING;
		break;
    }
	msg[1] = (unsigned int) ak4601->adc_input_voltage_setting[adc_val][lr_val];
	

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

    dprintk("%s : set val = %u, adc_val = %u, lr_val = %u, done\n", __FUNCTION__, ak4601->adc_input_voltage_setting[adc_val][lr_val], adc_val, lr_val);
    return 0;
}

static int get_sdout1_source_selector(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4601->sdout1_source_selector;

    dprintk("%s : value = %u, done\n", __FUNCTION__, ucontrol->value.enumerated.item[0]);
    return 0;
}

static int set_sdout1_source_selector(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	ak4601->sdout1_source_selector = ucontrol->value.enumerated.item[0];

	header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    msg[0] = (unsigned int) AK4601_VIRTUAL_028_SDOUT1_SOURCE_SELECTOR;
	msg[1] = (unsigned int) ak4601->sdout1_source_selector;

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

    dprintk("%s : value = %u, done\n", __FUNCTION__, ak4601->sdout1_source_selector);
    return 0;

}

static int get_mixerb_ch1_source_selector(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4601->mixerb_ch1_source_selector;

    dprintk("%s : value = %u, done\n", __FUNCTION__, ucontrol->value.enumerated.item[0]);
    return 0;
}

static int set_mixerb_ch1_source_selector(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	ak4601->mixerb_ch1_source_selector = ucontrol->value.enumerated.item[0];

	header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    msg[0] = (unsigned int) AK4601_VIRTUAL_029_MIXER_B_CH_1_SOURCE_SELECTOR;
	msg[1] = (unsigned int) ak4601->mixerb_ch1_source_selector;

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

    dprintk("%s : value = %u, done\n", __FUNCTION__, ak4601->mixerb_ch1_source_selector);
    return 0;

}

static int get_adc2_mux(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4601->adc2_mux;

    dprintk("%s : value = %u, done\n", __FUNCTION__, ucontrol->value.enumerated.item[0]);
    return 0;
}

static int set_adc2_mux(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	struct mbox_audio_data_header_t *header;
	unsigned int msg[MBOX_MSG_SIZE] = {0};

	ak4601->adc2_mux = ucontrol->value.enumerated.item[0];

    header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

	header->usage = MBOX_AUDIO_USAGE_SET;
	header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	header->msg_size = MBOX_MSG_SIZE;
	header->tx_instance = 0;

    msg[0] = (unsigned int) AK4601_VIRTUAL_030_ADC2_MUX;
	msg[1] = (unsigned int) ak4601->adc2_mux;

	dprintk("%s : send MBOX_AUDIO_USAGE_SET with cmd msg = %d, value msg = %d, msg_size = %d\n", __FUNCTION__, msg[0], msg[1], header->msg_size);
	tcc_mbox_audio_send_message(ak4601->mbox_audio_dev, header, msg, NULL);
	kfree(header);

    dprintk("%s : value = %u, done\n", __FUNCTION__, ak4601->adc2_mux);

    return 0;
}


/*************************** Kcontrols ******************************/
static const struct snd_kcontrol_new ak4601_vir_snd_controls[] = {
	
	AK4601_VIR_SINGLE_EXT("MIC Input Volume L", 0x00, 0x0F, get_mic_input_volume, set_mic_input_volume),
	AK4601_VIR_SINGLE_EXT("MIC Input Volume R", 0x01, 0x0F, get_mic_input_volume, set_mic_input_volume),

	AK4601_VIR_SINGLE_EXT("ADC1 Digital Volume L", 0x00, 0xFF, get_adc_volume, set_adc_volume),
	AK4601_VIR_SINGLE_EXT("ADC1 Digital Volume R", 0x01, 0xFF, get_adc_volume, set_adc_volume),
	AK4601_VIR_SINGLE_EXT("ADC2 Digital Volume L", 0x10, 0xFF, get_adc_volume, set_adc_volume),
	AK4601_VIR_SINGLE_EXT("ADC2 Digital Volume R", 0x11, 0xFF, get_adc_volume, set_adc_volume),
	AK4601_VIR_SINGLE_EXT("ADCM Digital Volume",   0x20,  0xFF, get_adc_volume, set_adc_volume),

	AK4601_VIR_SINGLE_EXT("DAC1 Digital Volume L", 0x00, 0xFF, get_dac_volume, set_dac_volume),
	AK4601_VIR_SINGLE_EXT("DAC1 Digital Volume R", 0x01, 0xFF, get_dac_volume, set_dac_volume),
	AK4601_VIR_SINGLE_EXT("DAC2 Digital Volume L", 0x10, 0xFF, get_dac_volume, set_dac_volume),
	AK4601_VIR_SINGLE_EXT("DAC2 Digital Volume R", 0x11, 0xFF, get_dac_volume, set_dac_volume),
	AK4601_VIR_SINGLE_EXT("DAC3 Digital Volume L", 0x20, 0xFF, get_dac_volume, set_dac_volume),
	AK4601_VIR_SINGLE_EXT("DAC3 Digital Volume R", 0x21, 0xFF, get_dac_volume, set_dac_volume),

	SOC_SINGLE_BOOL_EXT("ADC1 Mute", ADC1_INDEX, get_adc_mute, set_adc_mute),
	SOC_SINGLE_BOOL_EXT("ADC2 Mute", ADC2_INDEX, get_adc_mute, set_adc_mute),
	SOC_SINGLE_BOOL_EXT("ADCM Mute", ADCM_INDEX, get_adc_mute, set_adc_mute),

	SOC_SINGLE_BOOL_EXT("DAC1 Mute", DAC1_INDEX, get_dac_mute, set_dac_mute),
	SOC_SINGLE_BOOL_EXT("DAC2 Mute", DAC2_INDEX, get_dac_mute, set_dac_mute),
	SOC_SINGLE_BOOL_EXT("DAC3 Mute", DAC3_INDEX, get_dac_mute, set_dac_mute),

	SOC_ENUM_EXT("ADC1 Lch Input Voltage Setting", ak4601_vir_adc_input_vol_enum[0], get_adc_input_voltage_setting, set_adc_input_voltage_setting),
	SOC_ENUM_EXT("ADC1 Rch Input Voltage Setting", ak4601_vir_adc_input_vol_enum[1], get_adc_input_voltage_setting, set_adc_input_voltage_setting),
	SOC_ENUM_EXT("ADC2 Lch Input Voltage Setting", ak4601_vir_adc_input_vol_enum[2], get_adc_input_voltage_setting, set_adc_input_voltage_setting),
	SOC_ENUM_EXT("ADC2 Rch Input Voltage Setting", ak4601_vir_adc_input_vol_enum[3], get_adc_input_voltage_setting, set_adc_input_voltage_setting),
	SOC_ENUM_EXT("ADCM Input Voltage Setting",     ak4601_vir_adc_input_vol_enum[4], get_adc_input_voltage_setting, set_adc_input_voltage_setting),
	
	SOC_SINGLE_BOOL_EXT("SDOUT1 Output Enable", SDOUT1_INDEX, get_sdout_enable, set_sdout_enable),
	SOC_SINGLE_BOOL_EXT("SDOUT2 Output Enable", SDOUT2_INDEX, get_sdout_enable, set_sdout_enable),
	SOC_SINGLE_BOOL_EXT("SDOUT3 Output Enable", SDOUT3_INDEX, get_sdout_enable, set_sdout_enable),

    //a7 process belows with dapm
	SOC_ENUM_EXT("SDOUT1 Source Selector", ak4601_vir_sdout1_source_selector_enum, get_sdout1_source_selector, set_sdout1_source_selector),
	SOC_ENUM_EXT("MixerB Ch1 Source Selector", ak4601_vir_mixerb_ch1_source_selector_enum, get_mixerb_ch1_source_selector, set_mixerb_ch1_source_selector),
	SOC_ENUM_EXT("ADC2 MUX", ak4601_vir_adc2_mux_enum, get_adc2_mux, set_adc2_mux),
};


/*************************** DAI Driver ******************************/
static int ak4601_vir_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
    dprintk("%s : done!!\n", __FUNCTION__);

	return 0;
}

static int ak4601_vir_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	dprintk("%s : done!!\n", __FUNCTION__);

	return 0;
}

static int ak4601_vir_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
    dprintk("%s : done!!\n", __FUNCTION__);

	return 0;
}

static int ak4601_vir_set_dai_mute(struct snd_soc_dai *dai, int mute) 
{
	dprintk("%s : done!!\n", __FUNCTION__);

	return 0;
}

#define AK4601_RATES			SNDRV_PCM_RATE_8000_192000	

#define AK4601_DAC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE
#define AK4601_ADC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE

int ak4601_vir_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	dprintk("%s : done!!\n", __FUNCTION__);

	return 0;
}


static struct snd_soc_dai_ops ak4601_dai_ops = {
	.hw_params	= ak4601_vir_hw_params,
	.set_sysclk	= ak4601_vir_set_dai_sysclk,
	.set_fmt	= ak4601_vir_set_dai_fmt,
	.digital_mute = ak4601_vir_set_dai_mute,
	.set_tdm_slot = ak4601_vir_set_tdm_slot,
};

struct snd_soc_dai_driver ak4601_virtual_dai = {   									 
	.name = "ak4601-vir-aif",
	.playback = {
	       .stream_name = "Virtual Playback",
	       .channels_min = 1,
	       .channels_max = 16,
	       .rates = AK4601_RATES,
	       .formats = AK4601_DAC_FORMATS,
	},
	.capture = {
	       .stream_name = "Virtual Capture",
	       .channels_min = 1,
	       .channels_max = 16,
	       .rates = AK4601_RATES,
	       .formats = AK4601_ADC_FORMATS,
	},
	.ops = &ak4601_dai_ops,
};

/*************************** Codec Driver ******************************/
static int ak4601_vir_probe(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if(ak4601 == NULL) {
		eprintk("%s : Cannot get ak4601_priv..\n", __FUNCTION__);
		return -ENOMEM;
	}

	ak4601->codec = codec;

	ak4601_vir_mixer_init(ak4601);

	dprintk("%s : codec driver probe ok\n", __FUNCTION__);
	return ret;
}

static int ak4601_vir_remove(struct snd_soc_codec *codec)
{

    //TODO : de-init parameters to use in virtual codec
    dprintk("%s : codec driver remove ok\n", __FUNCTION__);
	return 0;
}

static int ak4601_vir_suspend(struct snd_soc_codec *codec)
{
	//struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	//ak4601_set_bias_level(codec, SND_SOC_BIAS_OFF);

    //TODO : back-up parameters to use in virtual codec
	//ak4601_backup_regs(codec);

    dprintk("%s : codec driver suspend ok\n", __FUNCTION__);
	
	return 0;
}

static int ak4601_vir_resume(struct snd_soc_codec *codec)
{

	//struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
//	ak4601_init_reg(codec);

    //TODO : restore parameters to use in virtual codec
	//ak4601_restore_regs(codec);

	dprintk("%s : codec driver resume ok\n", __FUNCTION__);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_ak4601_virtual = {
	.probe = ak4601_vir_probe,
	.remove = ak4601_vir_remove,
	.suspend =	ak4601_vir_suspend,
	.resume =	ak4601_vir_resume,

	.component_driver = {
		.controls = ak4601_vir_snd_controls,
		.num_controls = ARRAY_SIZE(ak4601_vir_snd_controls),
	},
};

/*************************** Platform Driver ******************************/
static int ak4601_vir_codec_probe(struct platform_device *pdev)
{

	struct ak4601_priv *ak4601;
	int ret = 0;

	ak4601 = devm_kzalloc(&pdev->dev, sizeof(struct ak4601_priv), GFP_KERNEL);
	
	if (ak4601 == NULL) {
		eprintk("%s : cannot alloc ak4601_priv\n", __FUNCTION__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ak4601);

	ak4601->pdev = pdev;
	ak4601->mbox_audio_dev = get_tcc_mbox_audio_device();

	dprintk("[%s] global_audio_dev dev_name:%s, mbox_name:%s, dev_num:%d\n", __FUNCTION__, ak4601->mbox_audio_dev->dev_name, ak4601->mbox_audio_dev->mbox_name, ak4601->mbox_audio_dev->dev_num);

    //in here, pdev->dev is set to codec->component->dev
	ret = snd_soc_register_codec(&pdev->dev,
			&soc_codec_dev_ak4601_virtual, &ak4601_virtual_dai, 1);
	if (ret < 0){
		eprintk("%s : snd_soc_resister_codec Virtual AK4601!!! error\n", __FUNCTION__);
		kfree(ak4601);
	}

    dprintk("%s : snd_soc_resister_codec Successfull!!!!, mbox = %p\n", __FUNCTION__, ak4601->mbox_audio_dev); 
	return ret;
}

static int ak4601_vir_codec_remove(struct platform_device *pdev)
{
    struct ak4601_priv *ak4601 = platform_get_drvdata(pdev);

	snd_soc_unregister_codec(&pdev->dev);
	kfree(ak4601);
	dprintk("%s : snd_soc_unregister_codec Successfull!!!!\n", __FUNCTION__); 
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ak4601_vir_match[] = {
	{ .compatible = "akm,ak4601-virtual" },
    {},
};

MODULE_DEVICE_TABLE(of, ak4601_vir_match);
#endif

static struct platform_driver ak4601_vir_driver = {
	.driver = {
		.name = AK4601_VIRTUAL_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ak4601_vir_match,
#endif
	},
	.probe = ak4601_vir_codec_probe,
	.remove = ak4601_vir_codec_remove,
};

module_platform_driver(ak4601_vir_driver);

MODULE_DESCRIPTION("ASoC ak4601 virtual codec driver");
MODULE_AUTHOR("Telechips Inc.");
MODULE_LICENSE("GPL");
