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
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : fail to get platform device from node.\n", __FUNCTION__);
		return NULL;
    }

	return platform_get_drvdata(pdev);
#else
    struct mbox_audio_device *mbox_audio = get_global_audio_dev();
    if (mbox_audio == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : global_audio_dev is null!!\n", __FUNCTION__);
		return NULL;
    }
    
	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO_UTILS] %s, get mbox audio device\n", __FUNCTION__);
	
	return mbox_audio;
#endif
}
EXPORT_SYMBOL(get_tcc_mbox_audio_device);

int tcc_mbox_audio_send_message(struct mbox_audio_device *audio_dev, struct mbox_audio_data_header_t *header, unsigned int *msg, struct mbox_audio_tx_reply_data_t *reply)
{
    return tcc_mbox_audio_send_command(audio_dev, header, msg, reply);
}
EXPORT_SYMBOL(tcc_mbox_audio_send_message);

/**********************************************************************************************************
 * (un)register kernel callback and get audio mailbox driver handler
 *
 * If kernel want to set received data, kernel driver must register/unregister below callback
 **********************************************************************************************************/

struct mbox_audio_device *tcc_mbox_audio_register_set_kernel_callback(void *client_data, void *set_callback, unsigned short cmd_type)
{
	struct mbox_audio_device *audio_dev;
	
	if (set_callback == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : fail to register due to set_callback is null.\n", __FUNCTION__);
		return NULL;
	}

	if (cmd_type > MBOX_AUDIO_CMD_TYPE_MAX) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : fail to register due to not supported cmd_type, %u.\n", __FUNCTION__, cmd_type);
		return NULL;
	}

	audio_dev = get_tcc_mbox_audio_device();

	if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : fail to register due to audio_dev is null.\n", __FUNCTION__);
		return NULL;
    }

	if (audio_dev->client[cmd_type].is_used > 0) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : warning.. client for cmd type(%u) is already used.. overrite it!!.\n", __FUNCTION__, cmd_type);
	}

    mutex_lock(&audio_dev->lock);
	audio_dev->client[cmd_type].set_callback = set_callback;
	audio_dev->client[cmd_type].client_data = client_data;
	audio_dev->client[cmd_type].is_used = 1;
	mutex_unlock(&audio_dev->lock);

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO_UTILS] %s : Successfully register set_callback of cmd_type %u.\n", __FUNCTION__, cmd_type);

	return audio_dev;

}
EXPORT_SYMBOL(tcc_mbox_audio_register_set_kernel_callback);

int tcc_mbox_audio_unregister_set_kernel_callback(struct mbox_audio_device *audio_dev, unsigned short cmd_type) {

    if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : fail to unregister due to audio_dev is null.\n", __FUNCTION__);
		return -ENODEV;
	}

	if (cmd_type > MBOX_AUDIO_CMD_TYPE_MAX) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO_UTILS] %s : fail to unregister due to not supported cmd_type, %u.\n", __FUNCTION__, cmd_type);
		return -ENODEV;
	}

	mutex_lock(&audio_dev->lock);
	audio_dev->client[cmd_type].set_callback = NULL;
	audio_dev->client[cmd_type].client_data = NULL;
	audio_dev->client[cmd_type].is_used = 0;
	mutex_unlock(&audio_dev->lock);

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO_UTILS] %s : Successfully register set_callback of cmd_type %u.\n", __FUNCTION__, cmd_type);

	return 0;
}
EXPORT_SYMBOL(tcc_mbox_audio_unregister_set_kernel_callback);
