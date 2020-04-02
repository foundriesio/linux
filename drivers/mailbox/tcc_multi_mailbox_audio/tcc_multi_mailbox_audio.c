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
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpufreq.h>
#include <linux/err.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/kdev_t.h>

#include <soc/tcc/pmap.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/cdev.h>
#include <asm/atomic.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include <sound/tcc/tcc_mbox_audio_ioctl.h>
#include <sound/tcc/utils/tcc_mbox_audio_utils.h>
#include <sound/tcc/tcc_multi_mailbox_audio.h>
#include <sound/tcc/params/tcc_mbox_audio_pcm_def.h>

#define MBOX_AUDIO_DEV_NAME        "mailbox-audio"
#define MBOX_AUDIO_DEV_MINOR       0

#define DRV_STATUS_NO_INIT 0
#define DRV_STATUS_READY 1

#define CMD_TIMEOUT		        			msecs_to_jiffies(1000)

#define AUDIO_MBOX_FOR_A53					0
#define AUDIO_MBOX_FOR_A7S					1

#ifndef CONFIG_OF
static struct mbox_audio_device *global_audio_dev;

struct mbox_audio_device *get_global_audio_dev(void)
{
    return global_audio_dev;
}
#endif

/*****************************************************************************
 * APIs for send message
 *****************************************************************************/
static int tcc_mbox_audio_send_message_to_channel(struct mbox_audio_device *audio_dev, struct tcc_mbox_data *mbox_data)
{
    int ret;

	ret = mbox_send_message(audio_dev->mbox_ch, mbox_data);
	mbox_client_txdone(audio_dev->mbox_ch,0);

	if (ret < 0) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Failed to send message via mailbox\n", __FUNCTION__);
	}

	return ret;
}

static unsigned short tcc_mbox_audio_get_available_tx_instance(struct mbox_audio_device *audio_dev)
{
	unsigned short instance_num = 0;
	int i;

    if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio device..\n", __FUNCTION__);
		return -ENODEV;
	}

    for (i = 0; i < TX_MAX_REPLY_COUNT; i++) {
		if (audio_dev->tx[i].reserved == 0) {
			break;
		}
		instance_num++;
    }

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : available tx_instance = %d\n", __FUNCTION__, instance_num);

	return instance_num;

}

int tcc_mbox_audio_send_command(struct mbox_audio_device *audio_dev, struct mbox_audio_data_header_t *header, unsigned int *msg, struct mbox_audio_tx_reply_data_t *reply)
{
    struct tcc_mbox_data *mbox_data;
	struct mbox_audio_tx_t *tx = NULL;

	unsigned short tx_instance = 0;
	
	int ret;

    if (audio_dev == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio device..\n", __FUNCTION__);
        return -ENODEV;
    }

	if (header == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get header information..\n", __FUNCTION__);
		return -ENOMEM;
	}


	if (header->msg_size > AUDIO_MBOX_MESSAGE_SIZE || header->msg_size <= 0) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : msg_size error (msg_size = %d).\n", __FUNCTION__, header->msg_size);
		return -EINVAL;
	}

	if (header->usage > MBOX_AUDIO_USAGE_MAX) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : usage exceed (usage = 0x%04x).\n", __FUNCTION__, header->usage);
		return -EINVAL;
	}

	if (header->cmd_type > MBOX_AUDIO_CMD_TYPE_MAX) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : msg_size exceed (cmd_type = 0x%04x).\n", __FUNCTION__, header->cmd_type);
		return -EINVAL;
	}

	if (header->usage == MBOX_AUDIO_USAGE_REQUEST && reply == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : if use MBOX_AUDIO_USAGE_REQUEST, reply must not NULL", __FUNCTION__);
		return -EINVAL; 
	}

    if (header->usage == MBOX_AUDIO_USAGE_REQUEST) {
		// for a53 : set the area of replied data
		tx_instance = tcc_mbox_audio_get_available_tx_instance(audio_dev);
		if (tx_instance < TX_MAX_REPLY_COUNT) {
			header->tx_instance = tx_instance;
			tx = &(audio_dev->tx[tx_instance]);
            spin_lock(&tx->lock);
			tx->reserved = 1;
  		    tx->reply.updated = 0;
			atomic_set(&tx->wakeup, 0);
            spin_unlock(&tx->lock);
		} else {
			header->tx_instance = 99; //tmp value, noti the error instance to sender
			printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : there is no available tx instance to reply.\n", __FUNCTION__);
			return -EINVAL;
		}
    }

    //packing mbox data
    mbox_data = kzalloc(sizeof(struct tcc_mbox_data), GFP_KERNEL);
    if (mbox_data == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot alloc mbox_data\n", __FUNCTION__);
	if (header->usage == MBOX_AUDIO_USAGE_REQUEST) {
		spin_lock(&tx->lock);
  		atomic_set(&tx->wakeup, 0);
  		tx->reserved = 0;
  		tx->reply.updated = 0;
                spin_unlock(&tx->lock);
	}
        return -ENOMEM;
    }

	mbox_data->cmd[0] = ((header->usage << 24) & 0xFF000000) | 
		((header->cmd_type << 16) & 0x00FF0000) | 
		((header->tx_instance << 8) & 0x0000FF00) |
		(header->msg_size & 0x000000FF);

    //payload //TODO : assume msg_size as unsigned int
    memcpy(&(mbox_data->cmd[AUDIO_MBOX_HEADER_SIZE]), msg, sizeof(unsigned int) * header->msg_size); 

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : usage = 0x%04x, type = 0x%04x, available tx = %d, msg_size = %d, command = 0x%08x\n",
	__FUNCTION__,  header->usage, header->cmd_type, header->tx_instance, header->msg_size, mbox_data->cmd[1]);

	if (tcc_mbox_audio_send_message_to_channel(audio_dev, mbox_data) < 0) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : send failed.\n", __FUNCTION__);
		if (header->usage == MBOX_AUDIO_USAGE_REQUEST) {
            spin_lock(&tx->lock);
  		    atomic_set(&tx->wakeup, 0);
  		    tx->reserved = 0; //need mutex?
  		    tx->reply.updated = 0;
            spin_unlock(&tx->lock);
		}
		kfree(mbox_data);
		return -EIO;
	}

	kfree(mbox_data);

    //wait reply with tx if need
	if (header->usage == MBOX_AUDIO_USAGE_REQUEST) {
		if (wait_event_interruptible_timeout(tx->wait, atomic_read(&tx->wakeup), CMD_TIMEOUT) <= 0) {
			printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : timeout error to get reply message.\n", __FUNCTION__);
            spin_lock(&tx->lock);
			atomic_set(&tx->wakeup, 0);
			tx->reserved = 0; //need mutex?
			tx->reply.updated = 0;
            spin_unlock(&tx->lock);
			return -EBUSY;
		}

		//after getting data from mbox
        if (tx->reply.updated == 1) {
			printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : received reply data from tx = %d, type = 0x%04x, msg_size = %d, msg[0] = 0x%08x\n", 
				__FUNCTION__, tx_instance, tx->reply.cmd_type, tx->reply.msg_size, tx->reply.msg[0]);
            //reply should be alloc in caller previously!!
            //TODO : determine the way 1. memcpy to caller's reply variable or 2. caller use audio_dev->tx.reply directly (no need memcpy)
			reply->cmd_type = tx->reply.cmd_type;
			reply->msg_size = tx->reply.msg_size;
			memcpy(reply->msg, tx->reply.msg, sizeof(unsigned int) * tx->reply.msg_size);
			ret = tx_instance;
        } else { //Do not need....
            printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : reply get data. but no updated.\n", __FUNCTION__);
			ret = -EINVAL;
        }

        //init tx after use it.
        spin_lock(&tx->lock);
        atomic_set(&tx->wakeup, 0);
        tx->reserved = 0;
        tx->reply.updated = 0;
        spin_unlock(&tx->lock);

		return ret;
	}

	return 0;

}


static int tcc_mbox_audio_user_queue_reset(struct mbox_audio_user_queue_t *user_queue)
{
    struct mbox_audio_cmd_t *audio_usr_msg = NULL;
    struct mbox_audio_cmd_t *audio_usr_msg_tmp = NULL;

    if (user_queue == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : user_queue is null..\n", __FUNCTION__);
        return -ENOMEM;
    }

    mutex_lock(&(user_queue->uq_lock));
    list_for_each_entry_safe(audio_usr_msg, audio_usr_msg_tmp, &(user_queue->list), list) {
        list_del_init(&audio_usr_msg->list);
        kfree(audio_usr_msg);
    }
    INIT_LIST_HEAD(&user_queue->list);
    atomic_set(&user_queue->uq_size, 0);
    mutex_unlock(&(user_queue->uq_lock));

	return 0;

}


/**********************************************************************************************************
 * processing the received message
 *
 * 1. tcc_mbox_audio_set_message
 *   - set message if received usage is MBOX_AUDIO_USAGE_SET (for setting values or command processing)
 * 2. tcc_mbox_audio_process_replied_message
 *   - get and process replied message if received usage is MBOX_AUDIO_USAGE_REPLY
 *    (actually, a7s does not receive this message because a7s use stored-backup data.)
 * 3. tcc_mbox_audio_reply_requested_message
 *   - received requests and reply about that if received usage is MBOX_AUDIO_USAGE_REQUEST
 *    (actually, a53 does not receive this message a53 is just a requester.)
 **********************************************************************************************************/
static int tcc_mbox_audio_set_message(struct mbox_audio_device *audio_dev, unsigned short usage, unsigned short cmd_type, unsigned short tx_instance, unsigned int *msg, unsigned short msg_size)
{
    struct mbox_audio_cmd_t *audio_usr_msg;
    int user_queue_size = 0, i = 0;

    if (audio_dev == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio device..\n", __FUNCTION__);
        return -ENODEV;
    }

    // process set message as below.
    // 1. case1 : process in kernel : call callback function to set actually values to registers at each driver.
    // 2. case2 : process in app : insert queue to app get this command

    //1. call callback function (process in kernel)
    if (cmd_type == MBOX_AUDIO_CMD_TYPE_POSITION_MIN && msg_size == AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE) {
        for (i = 0; i < AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE; i++) {
            if (audio_dev->client[i].is_used > 0 && audio_dev->client[i].set_callback) {
                audio_dev->client[i].set_callback((void *) audio_dev->client[i].client_data, msg, msg_size, i);
            }
        }
    } else if (audio_dev->client[cmd_type].is_used > 0 && audio_dev->client[cmd_type].set_callback) {
        audio_dev->client[cmd_type].set_callback((void *) audio_dev->client[cmd_type].client_data, msg, msg_size, cmd_type);
    } else {
        //2. add queue for polling (process in user)
        audio_usr_msg = kzalloc(sizeof(struct mbox_audio_cmd_t), GFP_KERNEL);

        if (audio_usr_msg == NULL) {
            printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot alloc audio_usr_msg information..\n", __FUNCTION__);
            return -ENOMEM;
        }

        //check whether queue is pending, if queue size > 10, regard user cannot process queueing message and delete all.
        user_queue_size = atomic_read(&(audio_dev->user_queue.uq_size));
        if (user_queue_size >= MAX_USER_QUEUE_SIZE) {
            printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : assume the command reader(user) is destroyed..\n", __FUNCTION__);
            tcc_mbox_audio_user_queue_reset(&audio_dev->user_queue);
        }

        //make audio_usr_msg
        INIT_LIST_HEAD(&audio_usr_msg->list);
        audio_usr_msg->adev = audio_dev;

        audio_usr_msg->mbox_data.cmd[0] = ((usage << 24) & 0xFF000000) |
                    ((cmd_type << 16) & 0x00FF0000) |
                    ((tx_instance << 8) & 0x0000FF00) |
                    (msg_size & 0x000000FF);

        memcpy(&(audio_usr_msg->mbox_data.cmd[AUDIO_MBOX_HEADER_SIZE]), msg, sizeof(unsigned int) * msg_size);

        //insert to queue
        mutex_lock(&audio_dev->user_queue.uq_lock);
        list_add_tail(&audio_usr_msg->list, &audio_dev->user_queue.list);
        atomic_inc(&(audio_dev->user_queue.uq_size));
        mutex_unlock(&audio_dev->user_queue.uq_lock);

        //after queueing, wake up polling wait queue
        wake_up(&(audio_dev->user_queue.uq_wait));
    }
	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : receive and set message : cmd_type = 0x%04x, msg_size = %d, cmd = 0x%08x\n", 
            __FUNCTION__, cmd_type, msg_size, msg[0]);

	return 0;
}

static int tcc_mbox_audio_process_replied_message(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int *msg, unsigned short msg_size, unsigned short index)
{
    
	struct mbox_audio_tx_t *tx;

    if (audio_dev == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio device..\n", __FUNCTION__);
        return -ENODEV;
    }

	if (index >= TX_MAX_REPLY_COUNT) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : invalid tx instance number..\n", __FUNCTION__);
		return -EINVAL;
	}

	tx = &(audio_dev->tx[index]);

	if (tx->reserved == 0) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Try to use non-reserved tx..\n", __FUNCTION__);
		return -EINVAL;
	}

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : message was replied. store them and wake up!! \n", __FUNCTION__);

    //do not send pointer because msg will be freed soon.
    tx->reply.cmd_type = cmd_type;
	tx->reply.msg_size = msg_size;
	memcpy(tx->reply.msg, msg, sizeof(unsigned int) * msg_size);

    tx->reply.updated = 1;
	atomic_set((&tx->wakeup), 1);
	wake_up_interruptible(&tx->wait);

	return 0;
}

/*****************************************************************************
 * message sent notify (not used)
 ******************************************************************************/
static void tcc_mbox_audio_message_sent(struct mbox_client *client, void *message, int r)
{
    struct mbox_client *cl;
	const char *name;
	cl = (struct mbox_client *)client;

	name = dev_name(cl->dev);

	if (r) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Message could not be sent: %d\n", __FUNCTION__, r);
	} else {
        printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s, Message sent!! from devt = %d, dev_name = %s\n", __FUNCTION__, cl->dev->devt ,name);
	}
}

/*****************************************************************************
 * rx work handler
 *****************************************************************************/
static void tcc_audio_mbox_rx_cmd_handler(void *device_info, struct tcc_mbox_data *mbox_data, int handle)
{
	struct mbox_audio_device *audio_dev = (struct mbox_audio_device *)device_info;
	unsigned int *msg;
	unsigned short usage, cmd_type, tx_instance, msg_size;
    int result;

    if(mbox_data == NULL || audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : no data or no mbox_audio_device to use\n", __FUNCTION__);
		return;
    }

	//get header inform
	usage = (mbox_data->cmd[0] >> 24) & 0x00FF;
	cmd_type = (mbox_data->cmd[0] >> 16) & 0x00FF;
	tx_instance = (mbox_data->cmd[0] >> 8) & 0x00FF;
	msg_size = mbox_data->cmd[0] & 0x00FF;

	msg = &(mbox_data->cmd[1]);

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : usage = 0x%04x, type = 0x%04x, tx_instance tx = %d, msg_size = %d\n", __FUNCTION__, usage, cmd_type, tx_instance, msg_size);

	//TODO : We should consider where we process the request & reply usage between worker thread and recieved callback thread...
	// 1. process in worker thread : slower than processing in callback thread
	// 2. process in callback thread : faster than worker thread but may affected to the processing of  pcm position data...
	switch (usage) {
	case MBOX_AUDIO_USAGE_SET :
	//1. set : set code/pcm/effect data
	    tcc_mbox_audio_set_message(audio_dev, usage, cmd_type, tx_instance, msg, msg_size);
		return;
    case MBOX_AUDIO_USAGE_REQUEST :
	//2. request : get code/pcm/effect data -> send reply with data
	//note. a53 do not receive this usage, because a7s do not send request (see tcc_mbox_audio_send_command())
        tcc_mbox_audio_set_message(audio_dev, usage, cmd_type, tx_instance, msg, msg_size);
		return;
	case MBOX_AUDIO_USAGE_REPLY :
	//3. reply : get relied data -> tx wakeup
	//note. a7s do not receive this usage, because a7s do not send request
	    result = tcc_mbox_audio_process_replied_message(audio_dev, cmd_type, msg, msg_size, tx_instance);
	    if (result < 0) {
			printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : MBOX_AUDIO_USAGE_REPLY : error to process replied msg.. sender may timeout. result = %d\n", __FUNCTION__, result);
	    }
		return;
	default :
	    return;
	}
}


/*****************************************************************************
 * process rx work
 *****************************************************************************/
static void tcc_mbox_audio_rx_work(struct kthread_work *work)
{
	struct mbox_audio_rx_t *rx = container_of(work, struct mbox_audio_rx_t, work);
	struct mbox_audio_cmd_t *audio_msg = NULL;
	struct mbox_audio_cmd_t *audio_msg_tmp = NULL;

	unsigned long flags;
	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : work start of %d ++ \n", __FUNCTION__, rx->handle);

	spin_lock_irqsave(&rx->lock, flags);
	list_for_each_entry_safe(audio_msg, audio_msg_tmp, &rx->list, list) {
		//handle processing cmd message
		if(rx->handler) {
			spin_unlock_irqrestore(&rx->lock, flags);
			rx->handler((void *)audio_msg->adev, &audio_msg->mbox_data, rx->handle);
			spin_lock_irqsave(&rx->lock, flags);
		}
		list_del_init(&audio_msg->list);
		kfree(audio_msg); //in here, audio command msg free
	}
	spin_unlock_irqrestore(&rx->lock, flags);

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : work end -- list_empty = %d\n", __FUNCTION__, list_empty(&rx->list) );

} 


/*****************************************************************************
 * message received callback
 ******************************************************************************/
static void tcc_mbox_audio_message_received(struct mbox_client *client, void *message)
{
	struct platform_device *pdev = NULL;//to_platform_device(client->dev);
	struct mbox_audio_device *audio_dev = NULL;//platform_get_drvdata(pdev);
	struct tcc_mbox_data *mbox_data = NULL;//(struct tcc_mbox_data *)message;
	struct mbox_audio_cmd_t *audio_msg;
	int rx_queue_handle;
	unsigned long flags;

	int i;

	unsigned short cmd_type;

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : received start++\n", __FUNCTION__);
	if (client == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : client is NULL.\n", __FUNCTION__);
		return;
	}

	if (client->dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : client->dev is NULL.\n", __FUNCTION__);
		return;
	}

	if (message == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : message is NULL.\n", __FUNCTION__);
		return;
	}

	pdev = to_platform_device(client->dev);
	if (pdev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : pdev is NULL.\n", __FUNCTION__);
		return;
	}
	audio_dev = platform_get_drvdata(pdev);
	mbox_data = (struct tcc_mbox_data *)message;

	if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio mbox device...\n", __FUNCTION__);
		return;
	}

	if (audio_dev->mbox_audio_ready != DRV_STATUS_READY) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Audio mbox device is not yet ready...\n", __FUNCTION__);
		return;
	}

	//memory alloc for audio_msg. free is in tcc_mbox_audio_rx_work
	audio_msg = kzalloc(sizeof(struct mbox_audio_cmd_t), GFP_KERNEL);

	if(audio_msg == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : error - unable to alloc memory(%ld)\n", __FUNCTION__, PTR_ERR(audio_msg));
		return;
	}

	//init & fill mbox_audio_cmd_t
	INIT_LIST_HEAD(&audio_msg->list);

	audio_msg->adev = audio_dev;
	for (i = 0; i < MBOX_CMD_FIFO_SIZE; i++) {
		audio_msg->mbox_data.cmd[i] = mbox_data->cmd[i];
	}

	//TODO : delete if we do not need mbox_data->data
	memcpy(audio_msg->mbox_data.data, mbox_data->data, mbox_data->data_len*sizeof(unsigned int));

	cmd_type = (mbox_data->cmd[0] >> 16) & 0x00FF;

	//choose the queue to process command
	if (cmd_type >= MBOX_AUDIO_CMD_TYPE_PCM && cmd_type <= MBOX_AUDIO_CMD_TYPE_EFFECT) {
	    rx_queue_handle = RX_QUEUE_FOR_COMMAND;
	} else if (cmd_type >= 0 && cmd_type <= MBOX_AUDIO_CMD_TYPE_POSITION_MAX) {
	    rx_queue_handle = cmd_type;
	} else {
	    printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : invalid cmd type.. forcibly use RX_QUEUE_FOR_COMMAND.\n", __FUNCTION__);
	    rx_queue_handle = RX_QUEUE_FOR_COMMAND;
	}


	if (&audio_dev->rx[rx_queue_handle] == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : audio_dev->rx is NULL for rx[%d].\n", __FUNCTION__, rx_queue_handle);
		if (audio_msg != NULL) {
			kfree(audio_msg);
		}
		return;
	}

	if (&audio_dev->rx[rx_queue_handle].lock == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : audio_dev->rx[%d].lock is NULL.\n", __FUNCTION__, rx_queue_handle);
		if (audio_msg != NULL) {
			kfree(audio_msg);
		}
		return;
	}

	if (&audio_dev->rx[rx_queue_handle].list == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : audio_dev->rx[%d].list is NULL.\n", __FUNCTION__, rx_queue_handle);
		if (audio_msg != NULL) {
			kfree(audio_msg);
		}
		return;
	}

	spin_lock_irqsave (&audio_dev->rx[rx_queue_handle].lock, flags);
	list_add_tail(&audio_msg->list, &audio_dev->rx[rx_queue_handle].list);
	spin_unlock_irqrestore(&audio_dev->rx[rx_queue_handle].lock, flags);

	//run worker thread to process command at queue
	kthread_queue_work(&audio_dev->rx[rx_queue_handle].kworker, &audio_dev->rx[rx_queue_handle].work);

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : received end--\n", __FUNCTION__);


}

/*****************************************************************************
 * register mbox client, channel and received callback
 ******************************************************************************/
static struct mbox_chan *audio_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->tx_block = false;
	client->rx_callback = tcc_mbox_audio_message_received;
	client->tx_done = client->tx_block ? tcc_mbox_audio_message_sent : NULL;
	client->knows_txdone = false;
	client->tx_tout = 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Failed to request %s channel\n", __FUNCTION__, name);
		return NULL;
	}

	return channel;
}

/*****************************************************************************
 * Initialize rx worker thread and tx worker queue at probe
 *****************************************************************************/
static long tcc_mbox_audio_rx_init(struct mbox_audio_rx_t *rx, const char *name, int num)
{
    INIT_LIST_HEAD(&rx->list);
	spin_lock_init(&rx->lock);

    rx->name = name;
	rx->handle = num;
	rx->handler = tcc_audio_mbox_rx_cmd_handler;

	atomic_set(&rx->seq, 0);

	kthread_init_worker(&rx->kworker);

	rx->task = kthread_run(kthread_worker_fn, &rx->kworker, name);

    printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : rx work thread = %d(%s)\n", __FUNCTION__, rx->handle, rx->name);

	if(IS_ERR(rx->task)) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Fail to kthread_run(%ld) \n", __FUNCTION__, PTR_ERR(rx->task));
		return PTR_ERR(rx->task);
	}

	kthread_init_work(&rx->work, tcc_mbox_audio_rx_work);
	
	return 0;
}

static int tcc_mbox_audio_tx_init(struct mbox_audio_tx_t *tx, int num)
{
    INIT_LIST_HEAD(&tx->list);
	spin_lock_init(&tx->lock);

    atomic_set(&tx->seq, 0);
	atomic_set(&tx->wakeup, 0);
	tx->handle = num;

    tx->reserved = 0;
	tx->reply.updated = 0;
	init_waitqueue_head(&tx->wait);

	return 0;
}

static int tcc_mbox_audio_user_queue_init(struct mbox_audio_user_queue_t *user_queue)
{
    INIT_LIST_HEAD(&user_queue->list);
    atomic_set(&user_queue->uq_size, 0);

    mutex_init(&user_queue->uq_lock);
    init_waitqueue_head(&user_queue->uq_wait);

	return 0;
}

/*****************************************************************************
 * for user app
 ******************************************************************************/

static unsigned int tcc_mbox_audio_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct mbox_audio_device *audio_dev = (struct mbox_audio_device *)filp->private_data;

    if (audio_dev == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio mbox device..\n", __FUNCTION__);
        return 0;
    }

    if (atomic_read(&audio_dev->user_queue.uq_size) > 0) {
        printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : return POLLIN.. already queue has set %d messages \n", __FUNCTION__, atomic_read(&audio_dev->user_queue.uq_size));
        return (POLLIN | POLLRDNORM);
    }

    poll_wait(filp, &(audio_dev->user_queue.uq_wait), wait);

    if (atomic_read(&audio_dev->user_queue.uq_size) > 0) {
        printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : return POLLIN..  queue has set %d messages after poll_wait.\n", __FUNCTION__, atomic_read(&audio_dev->user_queue.uq_size));
        return (POLLIN | POLLRDNORM);
    }

    return 0;
}

static ssize_t tcc_mbox_audio_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct mbox_audio_device *audio_dev = (struct mbox_audio_device *)filp->private_data;

    struct mbox_audio_cmd_t *audio_usr_msg = NULL;
    struct mbox_audio_cmd_t *audio_usr_msg_tmp = NULL;

    const unsigned int byte_size_per_mbox_data = MBOX_CMD_FIFO_SIZE * 4;
    unsigned int mbox_data_num = 0;
    unsigned int copy_byte_size = 0;

    // error handling
    if (count < byte_size_per_mbox_data || count % byte_size_per_mbox_data != 0) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Buffer size should be MBOX_CMD_FIFO_SIZE * 4 times..\n", __FUNCTION__);
        return -EINVAL;
    }
    if (audio_dev == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio mbox device..\n", __FUNCTION__);
        return -ENODEV;
    }

    mutex_lock(&(audio_dev->user_queue.uq_lock));
    list_for_each_entry_safe(audio_usr_msg, audio_usr_msg_tmp, &(audio_dev->user_queue.list), list) {
        mutex_unlock(&(audio_dev->user_queue.uq_lock));
        if (copy_byte_size >= count) {
            printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : read size (%d) is exceed the user count (%zd)\n", __FUNCTION__, copy_byte_size, count);
            break;
        }
        printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : read count = %zd, copy_byte_size = %d\n", __FUNCTION__, count, copy_byte_size);
        if (copy_to_user(buf + copy_byte_size, &(audio_usr_msg->mbox_data.cmd[0]), sizeof(unsigned int) * MBOX_CMD_FIFO_SIZE)) {
            printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : copy to user fail on %d-th mbox data.\n", __FUNCTION__, (mbox_data_num+1));
            return -EFAULT;
        }
        mbox_data_num++;
        copy_byte_size += (sizeof(unsigned int) * MBOX_CMD_FIFO_SIZE);
        mutex_lock(&(audio_dev->user_queue.uq_lock));
        list_del_init(&audio_usr_msg->list);
        atomic_dec(&(audio_dev->user_queue.uq_size));
        kfree(audio_usr_msg);
    }
    mutex_unlock(&(audio_dev->user_queue.uq_lock));

    printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : copy_byte_size = %d, mbox_data_num = %d\n", __FUNCTION__, copy_byte_size, mbox_data_num);

    return (ssize_t) copy_byte_size;

}

static ssize_t tcc_mbox_audio_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static int tcc_mbox_audio_open(struct inode * inode, struct file * filp)
{
    struct mbox_audio_device *audio_dev = container_of(inode->i_cdev, struct mbox_audio_device, c_dev);

	if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio mbox device..\n", __FUNCTION__);
		return -ENODEV;
	}

	if (audio_dev->mbox_audio_ready != DRV_STATUS_READY) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Audio mbox device is not yet ready...\n", __FUNCTION__);
		return -EBUSY;
	}

    mutex_lock(&audio_dev->lock);
	if (audio_dev->users < UINT_MAX) {
		audio_dev->users++;
	} else {
		audio_dev->users = UINT_MAX;
	}
	mutex_unlock(&audio_dev->lock);

	filp->private_data = audio_dev;

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : user opens the mbox_audio_device. current users = %d\n", __FUNCTION__, audio_dev->users);
	
	return 0;
}

static int tcc_mbox_audio_release(struct inode * inode, struct file * filp) 
{
    struct mbox_audio_device *audio_dev = container_of(inode->i_cdev, struct mbox_audio_device, c_dev);

	if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio device..\n", __FUNCTION__);
		return -ENODEV;
	}

    mutex_lock(&audio_dev->lock);
    if (audio_dev->users > 0) {
		audio_dev->users--;
	} else {
		audio_dev->users = 0;
	}
	mutex_unlock(&audio_dev->lock);

	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : user releases the mbox_audio_device. remain users = %d\n", __FUNCTION__, audio_dev->users);

	return 0;
}

static long tcc_mbox_audio_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
    struct mbox_audio_device *audio_dev = (struct mbox_audio_device *)filp->private_data;
	struct mbox_audio_data_header_t *header;
	struct tcc_mbox_audio_msg mbox_audio_msg;
	struct mbox_audio_tx_reply_data_t *reply_data;

	struct tcc_mbox_audio_msg __user *argp = (struct tcc_mbox_audio_msg __user *)arg;

	unsigned int *msg;

	long ret = 0;

    // error handling
    if (audio_dev == NULL) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot get audio mbox device..\n", __FUNCTION__);
		return -ENODEV;
	}

	if (audio_dev->mbox_audio_ready != DRV_STATUS_READY) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Audio mbox device is not yet ready...\n", __FUNCTION__);
		return -EBUSY;
	}

	ret = copy_from_user(&mbox_audio_msg, argp, sizeof(struct tcc_mbox_audio_msg));
	if(ret) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s: unable to copy user paramters(%ld) \n", __FUNCTION__, ret);
		goto err_ioctl;
	}

    msg = &(mbox_audio_msg.data[AUDIO_MBOX_HEADER_SIZE]);
	
	header = kzalloc(sizeof(struct mbox_audio_data_header_t), GFP_KERNEL);

    if (header == NULL) {
        printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s: unable to alloc header \n", __FUNCTION__);
        return -ENOMEM;
    }

    // get data
	switch(cmd) {
	case IOCTL_MBOX_AUDIO_CONTROL:
	    header->usage = (mbox_audio_msg.data[0] >> 24) & 0x00FF;
	    header->cmd_type = (mbox_audio_msg.data[0] >> 16) & 0x00FF;
	    header->tx_instance = (mbox_audio_msg.data[0] >> 8) & 0x00FF;
		break;
	case IOCTL_MBOX_AUDIO_PCM_SET_CONTROL:
	    header->usage = MBOX_AUDIO_USAGE_SET;
		header->cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
		header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_PCM_REPLY_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_REPLY;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
	    header->tx_instance = (mbox_audio_msg.data[0] >> 8) & 0x00FF;
		break;
	case IOCTL_MBOX_AUDIO_PCM_GET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_REQUEST;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_CODEC_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_CODEC_REPLY_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_REPLY;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	    header->tx_instance = (mbox_audio_msg.data[0] >> 8) & 0x00FF;
		break;
    case IOCTL_MBOX_AUDIO_CODEC_GET_CONTROL:
	    header->usage = MBOX_AUDIO_USAGE_REQUEST;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_CODEC;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_EFFECT_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_EFFECT;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_EFFECT_REPLY_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_REPLY;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_EFFECT;
	    header->tx_instance = (mbox_audio_msg.data[0] >> 8) & 0x00FF;
		break;
	case IOCTL_MBOX_AUDIO_EFFECT_GET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_REQUEST;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_EFFECT;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_0_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_0;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_1_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_1;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_2_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_2;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_3_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_3;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_4_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_4;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_5_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_5;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_6_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_6;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_7_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_7;
	    header->tx_instance = 0;
		break;
	case IOCTL_MBOX_AUDIO_POSITION_8_SET_CONTROL:
		header->usage = MBOX_AUDIO_USAGE_SET;
	    header->cmd_type = MBOX_AUDIO_CMD_TYPE_POSITION_8;
	    header->tx_instance = 0;
		break;
	default:
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Invalid command (%d)\n", __FUNCTION__, cmd);
		ret = -EINVAL;
		goto err_cmd;
	}
    header->msg_size = mbox_audio_msg.data[0] & 0x00FF;
    
	if (header->usage == MBOX_AUDIO_USAGE_REQUEST) {
            reply_data = kzalloc(sizeof(struct mbox_audio_tx_reply_data_t), GFP_KERNEL);
            if (reply_data == NULL) {
                printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot alloc reply_data for request.\n", __FUNCTION__);
                ret = -ENOMEM;
                goto err_cmd;
            }
            ret = tcc_mbox_audio_send_command(audio_dev, header, msg, reply_data);
        
            if (ret >= 0) {
                memset(&(mbox_audio_msg), 0, sizeof(struct tcc_mbox_audio_msg)); //init mbox_audio_msg
                mbox_audio_msg.data[0] = ((MBOX_AUDIO_USAGE_REPLY << 24) & 0xFF000000) |
                    ((reply_data->cmd_type << 16) & 0x00FF0000) |
                    ((0x00 << 8) & 0x0000FF00) |
                    (reply_data->msg_size & 0x000000FF);

                memcpy(&(mbox_audio_msg.data[AUDIO_MBOX_HEADER_SIZE]), reply_data->msg, sizeof(unsigned int) * reply_data->msg_size);

                // don't copy data area
                //if (copy_to_user(argp, &mbox_audio_msg, sizeof(struct tcc_mbox_audio_msg))) {
                if (copy_to_user(argp, &(mbox_audio_msg.data[0]), sizeof(unsigned int) * MBOX_AUDIO_CMD_SIZE)) {
                    printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : copy to user fail..\n", __FUNCTION__);
                    ret = -EFAULT;
                }
            }
            kfree(reply_data);
	} else {
	    ret = tcc_mbox_audio_send_command(audio_dev, header, msg, NULL);
	}

err_cmd:
	kfree(header);

err_ioctl:
	return ret;

}


struct file_operations tcc_mbox_audio_fops = {
    .owner = THIS_MODULE,
    .open = tcc_mbox_audio_open,
    .read = tcc_mbox_audio_read,
    .write = tcc_mbox_audio_write,
    .release = tcc_mbox_audio_release,
    .unlocked_ioctl = tcc_mbox_audio_ioctl,
    .poll = tcc_mbox_audio_poll,
};

/*****************************************************************************
 * Module Init/Exit
 ******************************************************************************/
static int multi_mbox_audio_probe(struct platform_device *pdev) {
	int result = 0;

	struct mbox_audio_device *audio_dev = NULL;

	char rx_name[20];
	const char *device_name = "audio-mbox-dev-a53";
	int i;

	audio_dev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_audio_device), GFP_KERNEL);
	if(!audio_dev) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : Cannot alloc audio device..\n", __FUNCTION__);
		return -ENOMEM;
	}

    audio_dev->mbox_audio_ready = DRV_STATUS_NO_INIT;
    audio_dev->dev = &pdev->dev;
	
	platform_set_drvdata(pdev, audio_dev);


    //get device / mbox name
	of_property_read_string(pdev->dev.of_node,"device-name", &audio_dev->dev_name);
	of_property_read_string(pdev->dev.of_node,"mbox-names", &audio_dev->mbox_name);

	if (strncmp(audio_dev->dev_name, device_name, strlen(device_name)) == 0) {
		audio_dev->chip_id = AUDIO_MBOX_FOR_A53;
	} else {
	    audio_dev->chip_id = AUDIO_MBOX_FOR_A7S;
	}

    //create and register character device
	result = alloc_chrdev_region(&audio_dev->dev_num, MBOX_AUDIO_DEV_MINOR, 1, audio_dev->dev_name);
	if (result) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : alloc_chrdev_region error %d\n", __FUNCTION__, result);
		return result;
	}

    cdev_init(&audio_dev->c_dev, &tcc_mbox_audio_fops);
	audio_dev->c_dev.owner = THIS_MODULE;

	result = cdev_add(&audio_dev->c_dev, audio_dev->dev_num, 1);
	if (result) {
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : cdev_add error %d\n",  __FUNCTION__, result);
		goto cdev_add_error;
	}	

	audio_dev->class = class_create(THIS_MODULE, audio_dev->dev_name);
	if (IS_ERR(audio_dev->class)) {
		result = PTR_ERR(audio_dev->class);
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : class_create error %d\n", __FUNCTION__, result);
		goto class_create_error;
	}

    //create device node
	audio_dev->dev = device_create(audio_dev->class, &pdev->dev, audio_dev->dev_num, NULL, audio_dev->dev_name);
	if (IS_ERR(audio_dev->dev)) {
		result = PTR_ERR(audio_dev->dev);
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : device_create error %d\n", __FUNCTION__, result);
		goto device_create_error;
	}

	audio_dev->mbox_ch = audio_request_channel(pdev, audio_dev->mbox_name);
	if (audio_dev->mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : ipc_request_channel error: %d\n", __FUNCTION__, result);
		goto mbox_request_channel_error;
	}

    audio_dev->pdev = pdev;
	audio_dev->users = 0;

    mutex_init(&audio_dev->lock);
	//tcc_mbox_audio_init_ak4601_backup_data(audio_dev, NULL, NULL, 0); //TODO: init check!!!


    for (i = 0; i < TX_MAX_REPLY_COUNT; i++) {
		if (tcc_mbox_audio_tx_init(&audio_dev->tx[i], i)) {
			result =  -EFAULT;
    	    printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : tcc_mbox_audio_tx_init error: %d\n", __FUNCTION__, result);
    	    goto mbox_init_tx_queue_error;
		}
    }

	for (i = 0; i < RX_QUEUE_COUNT; i++) {
		sprintf(rx_name, "mbox_audio_rx_%d", i);
        if (tcc_mbox_audio_rx_init(&audio_dev->rx[i], rx_name, i)) {
    		result =  -EFAULT;
    	    printk(KERN_ERR "[ERROR][MBOX_AUDIO] %s : tcc_mbox_audio_rx_init error: %d\n", __FUNCTION__, result);
    	    goto mbox_init_rx_queue_error;
        }
	}

    /*for (i = 0; i <= MBOX_AUDIO_CMD_TYPE_MAX; i++) {
		//audio_dev->client[i].dev = NULL; //It seems do not need..
		audio_dev->client[i].set_callback = NULL;
	    audio_dev->client[i].is_used = 0;
    }*/

	tcc_mbox_audio_user_queue_init(&audio_dev->user_queue);

#ifndef CONFIG_OF	
	global_audio_dev = audio_dev;
    printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : global_audio_dev dev_name:%s, mbox_name:%s, dev_num:%d\n", __FUNCTION__, global_audio_dev->dev_name, global_audio_dev->mbox_name, global_audio_dev->dev_num);
#endif

	audio_dev->mbox_audio_ready = DRV_STATUS_READY;
	printk(KERN_DEBUG "[DEBUG][MBOX_AUDIO] %s : Successfully registered!!!\n", __FUNCTION__);

	return 0;

mbox_init_tx_queue_error:
mbox_init_rx_queue_error:
mbox_request_channel_error:
	device_destroy(audio_dev->class, audio_dev->dev_num);

device_create_error:
	class_destroy(audio_dev->class);

class_create_error:
	cdev_del(&audio_dev->c_dev);

cdev_add_error:
	unregister_chrdev_region(audio_dev->dev_num, 1);

	return result;
}

static int multi_mbox_audio_remove(struct platform_device * pdev)
{
	struct mbox_audio_device *audio_dev = platform_get_drvdata(pdev);

    //free mbox channel
    if(audio_dev->mbox_ch != NULL) {
		mbox_free_channel(audio_dev->mbox_ch);
		audio_dev->mbox_ch= NULL;
	}

    mutex_destroy(&audio_dev->lock);

    //unregister device driver
	device_destroy(audio_dev->class, audio_dev->dev_num);
	class_destroy(audio_dev->class);
	cdev_del(&audio_dev->c_dev);
	unregister_chrdev_region(audio_dev->dev_num, 1);
	
	return 0;
}

#if defined(CONFIG_PM)
int multi_mbox_audio_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int multi_mbox_audio_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id multi_mbox_audio_match[] = {
    { .compatible = "telechips,mailbox-audio" },
    {},
};
MODULE_DEVICE_TABLE(of, multi_mbox_audio_match);
#endif

static struct platform_driver multi_mbox_audio_driver = {
    .driver = {
        .name = MBOX_AUDIO_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = multi_mbox_audio_match,
#endif
    },
    .probe  = multi_mbox_audio_probe,
    .remove = multi_mbox_audio_remove,
#if defined(CONFIG_PM)
	.suspend = multi_mbox_audio_suspend,
	.resume = multi_mbox_audio_resume,
#endif

};

module_platform_driver(multi_mbox_audio_driver);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips Audio H/W Multi MailBox");
MODULE_LICENSE("GPL v2");
