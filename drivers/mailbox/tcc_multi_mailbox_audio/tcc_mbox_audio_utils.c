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

#include <sound/tcc/utils/tcc_mbox_audio_utils.h>


#include <sound/tcc/params/tcc_mbox_audio_pcm_params.h>
#ifdef USE_AM3D_EFFECT
#include <sound/tcc/params/tcc_mbox_ak4601_codec_params.h>
#endif
#ifdef USE_AK4601_CODEC
#include <sound/tcc/params/tcc_mbox_am3d_effect_params.h>
#endif

//#define MBOX_AUDIO_UTILS_DEBUG

#define LOG_TAG    "SND_MBOX_AUDIO_UTILS "

#ifdef MBOX_AUDIO_UTILS_DEBUG    
#define dprintk(msg...)    printk(LOG_TAG  msg);		
#else
#define dprintk(msg...)    do {} while (0) 			
#endif

#define eprintk(msg...)    printk(KERN_ERR LOG_TAG  msg);

/*****************************************************************************
 * Inner Functions
 *****************************************************************************/
#ifdef USE_AM3D_EFFECT
static int tcc_mbox_audio_get_effect_value(unsigned int cmd, unsigned int *msg)
{
    const struct am3d_effect_data *effect;

    unsigned short size = 0;
	int i = 0;
    
    const unsigned int param = msg[1];
    const unsigned int channel_mask = msg[2];

	bool channel_masking = false;
	msg[3] = INT_MAX; //init value (msg[3] can be a sined int. so not use UINT_MAX (-1)) 

	if (cmd == AM3D_EFFECT_ENABLE_COMMAND) {	
	    dprintk("%s : get AM3D_EFFECT_ENABLE , value = %u\n", __FUNCTION__, effect_enable);
		msg[3] = g_am3d_effect_enable;
		return AUDIO_MBOX_EFFECT_SET_MESSAGE_SIZE;
	}

	for (i = 0; i < AM3D_EFFECT_TYPE_NUM; i++) {
		if (effect_command_type_map[i].type == cmd) {
		    effect = effect_command_type_map[i].effect;
			size = effect_command_type_map[i].size;
		    break;
		}
	}

    if (effect == NULL) {
        eprintk("%s : cannot find correspond effect!!!\n", __FUNCTION__);
	    return -EINVAL;
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
    return AUDIO_MBOX_EFFECT_SET_MESSAGE_SIZE;
}

static void tcc_mbox_audio_set_effect_value(unsigned int cmd, unsigned int *msg)
{
    struct am3d_effect_data *effect;
    unsigned short size = 0;

    const unsigned int param = msg[1];
    const unsigned int channel_mask = msg[2];

	bool channel_masking = false; 

	int i = 0;

	if (cmd == AM3D_EFFECT_ENABLE_COMMAND) {	
	    dprintk("%s : set AM3D_EFFECT_ENABLE , value = %u\n", __FUNCTION__, msg[3]);
		g_am3d_effect_enable = (int) msg[3];
		return;
	}

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
#endif

#ifdef USE_TELECHIPS_VOLCTRL
static int tcc_mbox_audio_get_pcm_volume_value(unsigned int *msg)
{
	//const unsigned int card = (packet[2] >> 4) & 0x0000000F;
	const unsigned int card_device = msg[1];
	const unsigned int device = card_device & 0x0F;

	if (!tcc_mbox_audio_pcm_is_available_device(card_device)) {
		eprintk("%s : Wrong device info, card|device = 0x%08x\n", __FUNCTION__, card_device);
		return -EINVAL;
	}

    if (tcc_mbox_audio_pcm_is_playback_device(card_device)) { //playback
	    msg[2] = ((g_asrc_pcm_playback_volume[device].integer << 16) & 0xFFFF0000) |
			((g_asrc_pcm_playback_volume[device].fraction << 8) & 0x0000FF00) |
			(g_asrc_pcm_playback_volume[device].mode & 0x000000FF);
    } else { //capture
        msg[2] = ((g_asrc_pcm_capture_volume[device].integer << 16) & 0xFFFF0000) |
			((g_asrc_pcm_capture_volume[device].fraction << 8) & 0x0000FF00) |
			(g_asrc_pcm_capture_volume[device].mode & 0x000000FF);
    }

	return AUDIO_MBOX_PCM_VOLUME_SET_MESSAGE_SIZE;
}

static void tcc_mbox_audio_set_pcm_volume_value(unsigned int *msg)
{
    const unsigned int card_device = msg[1];
	const unsigned int device = card_device & 0x0F;

	short integer = SHRT_MAX;
	short fraction = SHRT_MAX;
	short mode = SHRT_MAX;

    if (!tcc_mbox_audio_pcm_is_available_device(card_device)) {
		eprintk("%s : Wrong device info, device = %u \n", __FUNCTION__, card_device);
		return;
	}

    integer = (short) ((msg[2] >> 16) & 0xFFFF);
	fraction = 0;//(short) ((msg[2] >> 8) & 0x00FF); //not supported
	mode = (short) (msg[2] & 0x00FF);

	if (integer > ASRC_PCM_VOLUME_MAX) integer = ASRC_PCM_VOLUME_MAX;
    if (integer < ASRC_PCM_VOLUME_MIN) integer = ASRC_PCM_VOLUME_MIN;

	if (mode < ASRC_PCM_VALUE_VOLUME_MODE_MUTE || mode > ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING) {
		mode = ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING;
	}

	if (tcc_mbox_audio_pcm_is_playback_device(card_device)) { //playback
        g_asrc_pcm_playback_volume[device].integer = integer;
		g_asrc_pcm_playback_volume[device].fraction = fraction;
		g_asrc_pcm_playback_volume[device].mode = mode;

		dprintk("%s : set volume val: integer = %d, fraction = %d, mode = %d\n", __FUNCTION__,
			g_asrc_pcm_playback_volume[device].integer, g_asrc_pcm_playback_volume[device].fraction, g_asrc_pcm_playback_volume[device].mode);
		
			
    } else { //capture
		g_asrc_pcm_capture_volume[device].integer = integer;
		g_asrc_pcm_capture_volume[device].fraction = fraction;
		g_asrc_pcm_capture_volume[device].mode = mode;

		dprintk("%s : set volume val: integer = %d, fraction = %d, mode = %d\n", __FUNCTION__,
			g_asrc_pcm_capture_volume[device].integer, g_asrc_pcm_capture_volume[device].fraction, g_asrc_pcm_capture_volume[device].mode);
    }
}
#endif

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

int tcc_mbox_audio_restore_backup_data(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int *msg, unsigned short msg_size)
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

	if (msg_size > AUDIO_MBOX_MESSAGE_SIZE || msg_size <= 0) {
		eprintk("%s : Wrong message size : size = %d\n", __FUNCTION__, msg_size);
		return -EINVAL;
	}

	cmd = msg[0];

    switch (cmd_type) {
    case MBOX_AUDIO_CMD_TYPE_PCM :
#ifdef USE_TELECHIPS_BALANCEFADE
		if (cmd == ASRC_PCM_FADER && msg_size == AUDIO_MBOX_PCM_FADE_SET_MESSAGE_SIZE) {
            g_asrc_pcm_fader.balance = (short) ((msg[1] >> 16) & 0xFFFF);
			g_asrc_pcm_fader.fade = (short) (msg[1] & 0xFFFF);

			if (g_asrc_pcm_fader.balance > ASRC_PCM_BALANCE_FADE_MAX) g_asrc_pcm_fader.balance = ASRC_PCM_BALANCE_FADE_MAX;
        	if (g_asrc_pcm_fader.balance < ASRC_PCM_BALANCE_FADE_MIN) g_asrc_pcm_fader.balance = ASRC_PCM_BALANCE_FADE_MIN;
        
        	if (g_asrc_pcm_fader.fade > ASRC_PCM_BALANCE_FADE_MAX) g_asrc_pcm_fader.fade = ASRC_PCM_BALANCE_FADE_MAX;
        	if (g_asrc_pcm_fader.fade < ASRC_PCM_BALANCE_FADE_MIN) g_asrc_pcm_fader.fade = ASRC_PCM_BALANCE_FADE_MIN;
				
			dprintk("%s : set ASRC_PCM_FADER backup data as balance (%d), fade (%d)\n", 
			__FUNCTION__, g_asrc_pcm_fader.balance, g_asrc_pcm_fader.fade);
			return AUDIO_MBOX_PCM_FADE_SET_MESSAGE_SIZE;
		}
#endif
#ifdef USE_TELECHIPS_VOLCTRL
		if (cmd == ASRC_PCM_VOLUME && msg_size == AUDIO_MBOX_PCM_VOLUME_SET_MESSAGE_SIZE) {
			tcc_mbox_audio_set_pcm_volume_value(msg);
		}
#endif
		break;
    case MBOX_AUDIO_CMD_TYPE_CODEC :
#ifdef USE_AK4601_CODEC
        if (cmd >= 0 && cmd < AK4601_VIRTUAL_INDEX_COUNT && msg_size == AUDIO_MBOX_CODEC_SET_MESSAGE_SIZE) {
            g_ak4601_codec_data[cmd].value = msg[1];
            dprintk("%s : set codec backup data[%d] as %d\n", __FUNCTION__, cmd, g_ak4601_codec_data[cmd].value);
        } else {
            eprintk("%s : Wrong ak4601 codec cmd(%u) or msg size(%u)\n", __FUNCTION__, cmd, msg_size);
            return -EINVAL;
        }
#endif
		break;
    case MBOX_AUDIO_CMD_TYPE_EFFECT :
#ifdef USE_AM3D_EFFECT
        if (cmd >= 0 && cmd <= AM3D_EFFECT_ENABLE_COMMAND && msg_size == AUDIO_MBOX_EFFECT_SET_MESSAGE_SIZE) {
	        tcc_mbox_audio_set_effect_value(cmd, msg);
        } else {
			eprintk("%s : Wrong am3d effect cmd(%u) or msg size(%u)\n", __FUNCTION__, cmd, msg_size);
            return -EINVAL;
        }
#endif
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

int tcc_mbox_audio_get_backup_data(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int cmd, unsigned int *msg, unsigned short msg_size)
{
	if (audio_dev == NULL) {
		eprintk("%s : Cannot get audio device..\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (msg == NULL) {
		eprintk("%s : reply is null!!\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (msg_size > AUDIO_MBOX_MESSAGE_SIZE || msg_size <= 0) {
		eprintk("%s : Wrong message size : size = %d\n", __FUNCTION__, msg_size);
		return -EINVAL;
	}

    switch (cmd_type) {
    case MBOX_AUDIO_CMD_TYPE_PCM :
#ifdef USE_TELECHIPS_BALANCEFADE
		if (cmd == ASRC_PCM_FADER && msg_size == AUDIO_MBOX_PCM_FADE_GET_MESSAGE_SIZE) {
		    msg[1] = ((g_asrc_pcm_fader.balance << 16) & 0xFFFF0000) |
				(g_asrc_pcm_fader.fade & 0x0000FFFF);
			dprintk("%s : get ASRC_PCM_FADER backup data as 0x%08x\n", __FUNCTION__, msg[1]);
			return AUDIO_MBOX_PCM_FADE_SET_MESSAGE_SIZE;
		}
#endif
#ifdef USE_TELECHIPS_VOLCTRL
		if (cmd == ASRC_PCM_VOLUME && msg_size == AUDIO_MBOX_PCM_VOLUME_GET_MESSAGE_SIZE) {
			return tcc_mbox_audio_get_pcm_volume_value(msg);
		}
#endif
		break;
    case MBOX_AUDIO_CMD_TYPE_CODEC :
#ifdef USE_AK4601_CODEC
		if (cmd >= 0 && cmd < AK4601_VIRTUAL_INDEX_COUNT && msg_size == AUDIO_MBOX_CODEC_GET_MESSAGE_SIZE) {
            msg[0] = cmd;
            msg[1] = g_ak4601_codec_data[cmd].value;
		    dprintk("%s : get codec backup data[%d] as 0x%08x\n", __FUNCTION__, cmd, msg[1]);
		    return AUDIO_MBOX_CODEC_SET_MESSAGE_SIZE;
		} else {
			eprintk("%s : Wrong ak4601 codec cmd(%u) or msg size(%u)\n", __FUNCTION__, cmd, msg_size);
            return -EINVAL;
        }
#else
        break; //implement for each codec
#endif
    case MBOX_AUDIO_CMD_TYPE_EFFECT :
#ifdef USE_AM3D_EFFECT
        if (cmd >= 0 && cmd <= AM3D_EFFECT_ENABLE_COMMAND && msg_size == AUDIO_MBOX_EFFECT_GET_MESSAGE_SIZE) {
            return tcc_mbox_audio_get_effect_value(cmd, msg);
        } else {
			eprintk("%s : Wrong am3d effect cmd(%u) or msg size(%u)\n", __FUNCTION__, cmd, msg_size);
            return -EINVAL;
        }
#else
        break;
#endif
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
