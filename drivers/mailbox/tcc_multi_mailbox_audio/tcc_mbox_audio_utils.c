/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <sound/tcc/params/tcc_mbox_audio_pcm_params.h>
#include <sound/tcc/params/tcc_mbox_ak4601_codec_params.h>
#include <sound/tcc/params/tcc_mbox_am3d_effect_params.h>

#include <sound/tcc/utils/tcc_mbox_audio_utils.h>

//#define MBOX_AUDIO_UTILS_DEBUG

#define LOG_TAG    "SND_MBOX_AUDIO_UTILS "

#ifdef MBOX_AUDIO_UTILS_DEBUG    
#define dprintk(msg...)    printk(LOG_TAG  msg);		
#else
#define dprintk(msg...)    do {} while (0) 			
#endif

#define eprintk(msg...)    printk(KERN_ERR LOG_TAG  msg);

#define USE_GLOBAL_VAR_FOR_BACKUP_DATA

/*****************************************************************************
 * Inner Functions
 *****************************************************************************/
static void tcc_mbox_audio_get_effect_value(unsigned int cmd, unsigned int *msg)
{
    const struct am3d_effect_data *effect;
    unsigned short size = 0;
    
    const unsigned int param = msg[1];
    const unsigned int channel_mask = msg[2];

	bool channel_masking = false;
	msg[3] = INT_MAX; //init value (msg[3] can be a sined int. so not use UINT_MAX (-1))

	int i = 0;

	for (i = 0; i < AM3D_EFFECT_TYPE_NUM; i++) {
		if (effect_command_type_map[i].type == cmd) {
		    effect = effect_command_type_map[i].effect;
			size = effect_command_type_map[i].size;
		    break;
		}
	}

    if (effect == NULL) {
        eprintk("%s : cannot find correspond effect!!!\n", __FUNCTION__);
	    return;
	}

	dprintk("%s : effect type = %u, size = %u\n", __FUNCTION__, effect_command_type_map[i].type, size);

	//am3d scenario
	if (cmd != ZIRENE_INPUTMODULE && cmd != ZIRENE_SWEETSPOT && channel_mask == ZIRENE_SPEAKER_ALL_CHANNELS) {
		channel_masking = true;
	}
    
    for (i = 0; i < size; i++) {
        //regard that channel_mask should exactly matched with the channel_mask of am3d_effect_data
        //TODO channel mask check!!
        //for example, we use ALL_CHANNEL for getting but effect can control value at each channel..
        //Then, which value can get????
        if (param == effect[i].parameter && 
			(((channel_mask == effect[i].channel_mask) != 0) ||
			(channel_masking && (channel_mask & effect[i].channel_mask) != 0))) {
            dprintk("%s : find correspond effect value as %d\n", __FUNCTION__, effect[i].value);
            msg[3] = effect[i].value;
            break;
        }
    }
   
}

static void tcc_mbox_audio_set_effect_value(unsigned int cmd, unsigned int *msg)
{
    struct am3d_effect_data *effect;
    unsigned short size = 0;

    const unsigned int param = msg[1];
    const unsigned int channel_mask = msg[2];

	bool channel_masking = false; 

	int i = 0;

	for (i = 0; i < AM3D_EFFECT_TYPE_NUM; i++) {
		if (effect_command_type_map[i].type == cmd) {
		    effect = effect_command_type_map[i].effect;
			size = effect_command_type_map[i].size;
		    break;
		}
	}

	if (effect == NULL) {
        eprintk("%s : cannot find correspond effect!!!\n", __FUNCTION__);
	    return;
	}

	dprintk("%s : effect type = %u, size = %u\n", __FUNCTION__, effect_command_type_map[i].type, size);

    //am3d scenario
	if (cmd != ZIRENE_INPUTMODULE && cmd != ZIRENE_SWEETSPOT && channel_mask == ZIRENE_SPEAKER_ALL_CHANNELS) {
		channel_masking = true;
	}
    
    for (i = 0; i < size; i++) {
        //we should consider set value for ZIRENE_SPEAKER_ALL_CHANNELS.
        //we should change all values corresponding channels.
        if (param == effect[i].parameter && 
			(((channel_mask == effect[i].channel_mask) != 0) ||
			(channel_masking && (channel_mask & effect[i].channel_mask) != 0))) {
            effect[i].value = (int) msg[3];
            dprintk("%s : set correspond effect value of %d as %d\n",__FUNCTION__, i, effect[i].value);
        }
    }
    
}


/*****************************************************************************
 * APIs for get driver data
 *****************************************************************************/
struct mbox_audio_device *get_tcc_mbox_audio_device(void)
{
#ifdef CONFIG_OF
    struct platform_device *pdev;
	struct device_node *of_node_mbox_audio;

	of_node_mbox_audio = of_find_compatible_node(NULL, NULL, "telechips,mailbox-audio");
    if ((pdev = of_find_device_by_node(of_node_mbox_audio)) == NULL) {
		eprintk("%s : fail to get platform device from node.\n", __FUNCTION__);
		return NULL;
    }

	return platform_get_drvdata(pdev);
#else
    struct mbox_audio_device *mbox_audio = get_global_audio_dev();
    if (mbox_audio == NULL) {
		eprintk("%s : global_audio_dev is null!!\n", __FUNCTION__);
		return NULL;
    }
    
    dprintk("%s, get mbox audio device\n", __FUNCTION__);
	
	return mbox_audio;
#endif
}
EXPORT_SYMBOL(get_tcc_mbox_audio_device);

int tcc_mbox_audio_send_message(struct mbox_audio_device *audio_dev, struct mbox_audio_data_header_t *header, unsigned int *msg, struct mbox_audio_tx_reply_data_t *reply)
{
    return tcc_mbox_audio_send_command(audio_dev, header, msg, reply);
}
EXPORT_SYMBOL(tcc_mbox_audio_send_message);


//TODO: how to get init data?????? temporary, hard coding... and call in probe
int tcc_mbox_audio_init_ak4601_backup_data(struct mbox_audio_device *audio_dev, unsigned int *key, unsigned int *value, int size)
{
#ifndef	USE_GLOBAL_VAR_FOR_BACKUP_DATA

    if (audio_dev == NULL) {
		return -ENOMEM;
	}

    //tmp hard coding
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_001_MIC_INPUT_VOLUME_L] = 0;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_002_MIC_INPUT_VOLUME_R] = 0;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_003_ADC1_DIGITAL_VOLUME_L] = 255;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_004_ADC1_DIGITAL_VOLUME_R] = 0;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_005_ADC2_DIGITAL_VOLUME_L] = 207;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_006_ADC2_DIGITAL_VOLUME_R] = 207;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_007_ADCM_DIGITAL_VOLUME] = 207;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_008_DAC1_DIGITAL_VOLUME_L] = 231;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_009_DAC1_DIGITAL_VOLUME_R] = 231;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_010_DAC2_DIGITAL_VOLUME_L] = 231;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_011_DAC2_DIGITAL_VOLUME_R] = 231;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_012_DAC3_DIGITAL_VOLUME_L] = 231;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_013_DAC3_DIGITAL_VOLUME_R] = 231;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_014_ADC1_MUTE] = AK4601_VIRTUAL_OFF;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_015_ADC2_MUTE] = AK4601_VIRTUAL_OFF;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_016_ADCM_MUTE] = AK4601_VIRTUAL_OFF;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_017_DAC1_MUTE] = AK4601_VIRTUAL_OFF;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_018_DAC2_MUTE] = AK4601_VIRTUAL_OFF;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_019_DAC3_MUTE] = AK4601_VIRTUAL_OFF;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_020_ADC1_INPUT_VOLTAGE_SETTING_L] = AK4601_VIRTUAL_2_3_Vpp;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_021_ADC1_INPUT_VOLTAGE_SETTING_R] = AK4601_VIRTUAL_2_3_Vpp;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_022_ADC2_INPUT_VOLTAGE_SETTING_L] = AK4601_VIRTUAL_2_3_Vpp;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_023_ADC2_INPUT_VOLTAGE_SETTING_R] = AK4601_VIRTUAL_2_3_Vpp;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_024_ADCM_INPUT_VOLTAGE_SETTING] = AK4601_VIRTUAL_2_3_Vpp;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_025_SDOUT1_OUTPUT_ENABLE] = AK4601_VIRTUAL_ON;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_026_SDOUT2_OUTPUT_ENABLE] = AK4601_VIRTUAL_ON;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_027_SDOUT3_OUTPUT_ENABLE] = AK4601_VIRTUAL_ON;
	
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_028_SDOUT1_SOURCE_SELECTOR] = AK4601_VIRTUAL_MIXER_B;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_029_MIXER_B_CH_1_SOURCE_SELECTOR] = AK4601_VIRTUAL_ADC1;
	audio_dev->backup_data.ak4601_data[AK4601_VIRTUAL_030_ADC2_MUX] = AK4601_VIRTUAL_AIN2;
#endif
	return 0;
}
EXPORT_SYMBOL(tcc_mbox_audio_init_ak4601_backup_data);

//TODO : To prevent access at A53......
int tcc_mbox_audio_restore_backup_data(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int *msg, unsigned short size)
{
    unsigned int cmd;

	if (audio_dev == NULL) {
		eprintk("%s : Cannot get audio device..\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (msg == NULL) {
		eprintk("%s : msg is null!!\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (size > AUDIO_MBOX_MESSAGE_SIZE) {
		eprintk("%s : size is exceed to message size : size = %d\n", __FUNCTION__, size);
		return -EINVAL;
	}

	cmd = msg[0];

    switch (cmd_type) {
    case MBOX_AUDIO_CMD_TYPE_PCM :
		//no need to backup data (just command)
		break;
    case MBOX_AUDIO_CMD_TYPE_CODEC :
       if (cmd >= 0 && cmd < AK4601_VIRTUAL_INDEX_COUNT) {
#ifdef USE_GLOBAL_VAR_FOR_BACKUP_DATA
           g_ak4601_codec_data[cmd].value = msg[1];
#else
           audio_dev->backup_data.ak4601_data[cmd] = msg[1];
#endif
           dprintk("%s : set codec backup data[%d] as %d\n", __FUNCTION__, cmd, msg[1]);
       }
		break;
    case MBOX_AUDIO_CMD_TYPE_EFFECT :
        if (cmd >= 0 && cmd <= ZIRENE_BALANCEFADE) {
	        tcc_mbox_audio_set_effect_value(cmd, msg);
        }
		break;
    case MBOX_AUDIO_CMD_TYPE_DATA_TX_0 :
		break;
	case MBOX_AUDIO_CMD_TYPE_DATA_TX_1 :
		break;
	case MBOX_AUDIO_CMD_TYPE_DATA_TX_2 :
		break;
	case MBOX_AUDIO_CMD_TYPE_DATA_RX :
		break;
	default :
		eprintk("%s : invalid cmd type.. \n", __FUNCTION__);
		return -EINVAL;
    }
    
    return 0;
}
EXPORT_SYMBOL(tcc_mbox_audio_restore_backup_data);

int tcc_mbox_audio_get_backup_data(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int cmd, unsigned int *msg)
{
	if (audio_dev == NULL) {
		eprintk("%s : Cannot get audio device..\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (msg == NULL) {
		eprintk("%s : reply is null!!\n", __FUNCTION__);
		return -ENOMEM;
	}

    switch (cmd_type) {
    case MBOX_AUDIO_CMD_TYPE_PCM :
		//no need to backup data (just command)
		break;
    case MBOX_AUDIO_CMD_TYPE_CODEC :
		if (cmd >= 0 && cmd < AK4601_VIRTUAL_INDEX_COUNT) {
#ifdef USE_GLOBAL_VAR_FOR_BACKUP_DATA
            msg[0] = cmd;
            msg[1] = g_ak4601_codec_data[cmd].value;
#else
 		    msg[0] = cmd;
 		    msg[1] = audio_dev->backup_data.ak4601_data[cmd];
#endif
		    dprintk("%s : get codec backup data[%d] as %d\n", __FUNCTION__, cmd, msg[1]);
		    return AUDIO_MBOX_CODEC_SET_MESSAGE_SIZE;
		} else {
			return -EINVAL;
		}
    case MBOX_AUDIO_CMD_TYPE_EFFECT :
        if (cmd >= 0 && cmd <= ZIRENE_BALANCEFADE) {
            tcc_mbox_audio_get_effect_value(cmd, msg);
            return AUDIO_MBOX_EFFECT_SET_MESSAGE_SIZE;
        } else {
            return -EINVAL;
        }
    case MBOX_AUDIO_CMD_TYPE_DATA_TX_0 :
	case MBOX_AUDIO_CMD_TYPE_DATA_TX_1 :
	case MBOX_AUDIO_CMD_TYPE_DATA_TX_2 :
	case MBOX_AUDIO_CMD_TYPE_DATA_RX :
		//no need to backup data (just command)
		break;
	default :
		eprintk("%s : invalid cmd type.. \n", __FUNCTION__);
		return -EINVAL;
    }
    
    return 0;
}
EXPORT_SYMBOL(tcc_mbox_audio_get_backup_data);

/**********************************************************************************************************
 * (un)register kernel callback and get audio mailbox driver handler
 *
 * If kernel want to set received data, kernel driver must register/unregister below callback
 **********************************************************************************************************/

struct mbox_audio_device *tcc_mbox_audio_register_set_kernel_callback(void *client_data, void *set_callback, unsigned short cmd_type)
{
	struct mbox_audio_device *audio_dev;
	
	if (set_callback == NULL) {
		eprintk("%s : fail to register due to set_callback is null.\n", __FUNCTION__);
		return NULL;
	}

	if (cmd_type > MBOX_AUDIO_CMD_TYPE_MAX) {
		eprintk("%s : fail to register due to not supported cmd_type, %u.\n", __FUNCTION__, cmd_type);
		return NULL;
	}

	audio_dev = get_tcc_mbox_audio_device();

	if (audio_dev == NULL) {
		eprintk("%s : fail to register due to audio_dev is null.\n", __FUNCTION__);
		return NULL;
    }

	if (audio_dev->client[cmd_type].is_used > 0) {
		eprintk("%s : warning.. client for cmd type(%u) is already used.. overrite it!!.\n", __FUNCTION__, cmd_type);
	}

    mutex_lock(&audio_dev->lock);
	audio_dev->client[cmd_type].set_callback = set_callback;
	audio_dev->client[cmd_type].client_data = client_data;
	audio_dev->client[cmd_type].is_used = 1;
	mutex_unlock(&audio_dev->lock);

	dprintk("%s : Successfully register set_callback of cmd_type %u.\n", __FUNCTION__, cmd_type);

	return audio_dev;

}
EXPORT_SYMBOL(tcc_mbox_audio_register_set_kernel_callback);

int tcc_mbox_audio_unregister_set_kernel_callback(struct mbox_audio_device *audio_dev, unsigned short cmd_type) {

    if (audio_dev == NULL) {
		eprintk("%s : fail to unregister due to audio_dev is null.\n", __FUNCTION__);
		return -ENODEV;
	}

	if (cmd_type > MBOX_AUDIO_CMD_TYPE_MAX) {
		eprintk("%s : fail to unregister due to not supported cmd_type, %u.\n", __FUNCTION__, cmd_type);
		return -ENODEV;
	}

	mutex_lock(&audio_dev->lock);
	audio_dev->client[cmd_type].set_callback = NULL;
	audio_dev->client[cmd_type].client_data = NULL;
	audio_dev->client[cmd_type].is_used = 0;
	mutex_unlock(&audio_dev->lock);

	dprintk("%s : Successfully register set_callback of cmd_type %u.\n", __FUNCTION__, cmd_type);

	return 0;
}
EXPORT_SYMBOL(tcc_mbox_audio_unregister_set_kernel_callback);
