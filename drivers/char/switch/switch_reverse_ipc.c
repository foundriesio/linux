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

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>

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

#include "switch_reverse_ipc.h"

#define SWITCH_REVERSE_IPC_DEV_NAME        "switch_reverse_ipc"
#define SWITCH_REVERSE_IPC_DEV_MINOR       0

static int ipcDebugLevel = 0;

#define dprintk(dev, msg...)                                \
{                                                      \
	if (ipcDebugLevel > 1)                                     \
		dev_info(dev, msg);           \
}

#define eprintk(dev, msg...)                                \
{                                                      \
	if (ipcDebugLevel > 0)                                     \
		dev_err(dev, msg);             \
}

typedef void (* switch_reverse_ipc_mbox_receive)(struct mbox_client *, void *);

#ifndef CONFIG_TCC803X_CA7S
extern int	switch_reverse_is_enabled(void);
extern void	switch_reverse_set_state(long val);
#endif//CONFIG_TCC803X_CA7S

int switch_reverse_ipc_mailbox_send(struct switch_reverse_ipc_device * switch_reverse_ipc_dev, struct tcc_mbox_data * switch_reverse_ipc_msg) {
	int ret = SWITCH_REVERSE_IPC_ERR_NOTREADY;

	if((switch_reverse_ipc_dev != NULL) && (switch_reverse_ipc_msg != NULL)) {
		SwitchReverseIpcHandler * ipc_handler = &switch_reverse_ipc_dev->ipc_handler;
		int i;
		dprintk(switch_reverse_ipc_dev->dev,"%s : switch_reverse_ipc_msg(0x%p)\n", __func__, (void *)switch_reverse_ipc_msg);
		for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++) {
			dprintk(switch_reverse_ipc_dev->dev,"%s : cmd[%d]: (0x%02x)\n", __func__, i, switch_reverse_ipc_msg->cmd[i]);
		}
		dprintk(switch_reverse_ipc_dev->dev,"%s : data size(%d)\n", __func__, switch_reverse_ipc_msg->data_len);

		mutex_lock(&ipc_handler->mboxMutex);
		(void)mbox_send_message(switch_reverse_ipc_dev->mbox_ch, switch_reverse_ipc_msg);
		mutex_unlock(&ipc_handler->mboxMutex);
		ret = SWITCH_REVERSE_IPC_SUCCESS;
	} else {
		eprintk(switch_reverse_ipc_dev->dev, "%s :  Invalid Arguements\n", __func__);
		ret = SWITCH_REVERSE_IPC_ERR_ARGUMENT;
	}

	return ret;
}

struct mbox_chan * switch_reverse_ipc_request_channel(struct platform_device * pdev, const char * name, switch_reverse_ipc_mbox_receive handler) {
	struct mbox_client * client;
	struct mbox_chan * channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->rx_callback = handler;
	client->tx_done = NULL;
	client->tx_block = false;
	client->knows_txdone = false;
	client->tx_tout = 100;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		dev_err(&pdev->dev, "Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

#define ACK_TIMEOUT			200	//ms

unsigned int switch_reverse_ipc_get_sequential_ID(struct switch_reverse_ipc_device * switch_reverse_ipc_dev) {
//	spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	switch_reverse_ipc_dev->ipc_handler.seqID = (switch_reverse_ipc_dev->ipc_handler.seqID + 1) % 0xFFFFFFFF;
//	spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);

	return switch_reverse_ipc_dev->ipc_handler.seqID;
}

int switch_reverse_ipc_cmd_wait_event_timeout(struct switch_reverse_ipc_device * switch_reverse_ipc_dev, SwitchReverseIpcCmdType cmdType, unsigned int seqID, unsigned int timeOut) {
	SwitchReverseIpcHandler * ipc_handler = &switch_reverse_ipc_dev->ipc_handler;
	int ret = -1;

	if(switch_reverse_ipc_dev != NULL) {
		ipc_handler->condition = 1;
		ret = wait_event_interruptible_timeout(ipc_handler->wq, ipc_handler->condition == 0, msecs_to_jiffies(timeOut));
		if((ipc_handler->condition == 1) || (ret <= 0)) {
			/* timeout */
			ret = SWITCH_REVERSE_IPC_ERR_TIMEOUT;
		} else {
			ret = SWITCH_REVERSE_IPC_SUCCESS;
		}

		/* clear flag */
		ipc_handler->condition = 0;
	}
	return ret;
}

int switch_reverse_ipc_send_write(struct switch_reverse_ipc_device * switch_reverse_ipc_dev, char * data, unsigned int size) {
	int ret = SWITCH_REVERSE_IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	sendMsg.cmd[0] = switch_reverse_ipc_get_sequential_ID(switch_reverse_ipc_dev);
	sendMsg.cmd[1] = (SWITCH_REVERSE_IPC_CMD_TYPE_WRITE << 16) | (SWITCH_REVERSE_IPC_CMD_ID_WRITE);
	sendMsg.cmd[2] = size;	
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;

	memcpy(&sendMsg.data ,data, (unsigned long)size);
	sendMsg.data_len = (size+3)/4;
	ret = switch_reverse_ipc_mailbox_send(switch_reverse_ipc_dev, &sendMsg);
	if(ret == SWITCH_REVERSE_IPC_SUCCESS) {
		ret = switch_reverse_ipc_cmd_wait_event_timeout(switch_reverse_ipc_dev, SWITCH_REVERSE_IPC_CMD_TYPE_WRITE, sendMsg.cmd[0], ACK_TIMEOUT);
		printk("%s - switch_reverse_ipc_cmd_wait_event_timeout - ret: %d\n", __func__, ret);
		if(ret != SWITCH_REVERSE_IPC_SUCCESS) {
			eprintk(switch_reverse_ipc_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
	}

	return ret;
}

int switch_reverse_ipc_send_ack(struct switch_reverse_ipc_device * switch_reverse_ipc_dev, unsigned int seqID, SwitchReverseIpcCmdType cmdType, unsigned int sourcCmd) {
	int ret = SWITCH_REVERSE_IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	sendMsg.cmd[0] = seqID;
	sendMsg.cmd[1] = (cmdType << 16) | (SWITCH_REVERSE_IPC_CMD_ID_ACK);
	sendMsg.cmd[2] = sourcCmd;
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;

	sendMsg.data_len = 0;
	ret = switch_reverse_ipc_mailbox_send(switch_reverse_ipc_dev, &sendMsg);

	return ret;
}

void switch_reverse_ipc_receive_message(struct mbox_client * client, void * message) {
	struct tcc_mbox_data * msg = (struct tcc_mbox_data *)message;
	struct platform_device * pdev = to_platform_device(client->dev);
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = platform_get_drvdata(pdev);
	SwitchReverseIpcCmdType cmdType;
	SwitchReverseIpcCmdId	cmdID ;
	int i;

	for(i=0; i<7; i++) {
		dprintk(switch_reverse_ipc_dev->dev,"%s : cmd[%d] = [0x%02x]\n", __func__,i, msg->cmd[i]);
	}

	dprintk(switch_reverse_ipc_dev->dev,"%s : data size (%d)\n", __func__,msg->data_len);
	for(i=0; i<msg->data_len; i++) {
		dprintk(switch_reverse_ipc_dev->dev,"%s : data[%d] = [0x%02x]\n", __func__, i, msg->data[i]);
	}

	cmdType = (msg->cmd[1] >> 16) & 0xFFFF;
	cmdID = (msg->cmd[1]) & 0xFFFF;
	if(cmdType < SWITCH_REVERSE_IPC_CMD_TYPE_MAX) {
		if(cmdID == SWITCH_REVERSE_IPC_CMD_ID_ACK) {
			switch_reverse_ipc_dev->ipc_handler.condition = 0;
			wake_up_interruptible(&switch_reverse_ipc_dev->ipc_handler.wq);
		} else {
#ifndef CONFIG_TCC803X_CA7S
			if(switch_reverse_is_enabled()) {
				long	state = (long)msg->data[0] & 0x1;
				dev_info(switch_reverse_ipc_dev->dev, "state: %ld\n", state);
				switch_reverse_set_state(state);
				switch_reverse_ipc_send_ack(switch_reverse_ipc_dev, msg->cmd[0], cmdType, msg->cmd[1]);
			} else {
				dprintk(switch_reverse_ipc_dev->dev," %s - switch_reverse is not enabled\n", __func__);
			}
#endif//CONFIG_TCC803X_CA7S
		}
	} else {
		dprintk(switch_reverse_ipc_dev->dev,"%s, receive unknown cmd [%d]\n", __func__, cmdType);
	}	
}

ssize_t switch_reverse_ipc_write(struct file * filp, const char __user * buf, size_t count, loff_t * f_pos) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = (struct switch_reverse_ipc_device *)filp->private_data;
	int  ret = 0;

	dprintk(switch_reverse_ipc_dev->dev, "%s : In, data size(%d)\n", __func__, (int)count);
	if(!filp->private_data)
		return -ENODEV; 

	if(switch_reverse_ipc_dev != NULL) {
		unsigned char * tempWbuf = switch_reverse_ipc_dev->ipc_handler.tempWbuf;
		unsigned int	size=0;
		if(count > SWITCH_REVERSE_IPC_MAX_WRITE_SIZE) {
			size = SWITCH_REVERSE_IPC_MAX_WRITE_SIZE;
		} else {
			size = count;
		}

		if(copy_from_user(tempWbuf, buf, (unsigned long)size) == 0) {
			ret = switch_reverse_ipc_send_write(switch_reverse_ipc_dev, (unsigned char *)tempWbuf, (unsigned int)size);
			if(ret < 0) {
				ret = 0;
			}		
		} else {
			ret =  -ENOMEM;
		}
	} else {
		eprintk(switch_reverse_ipc_dev->dev, "%s : device not init\n", __func__);
		ret = -ENXIO;
	}
	
	return ret;
}

long switch_reverse_ipc_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = (struct switch_reverse_ipc_device *)filp->private_data;
	long ret = 0;

	switch(cmd) {
		case SWITCH_REVERSE_IPC_CMD_TYPE_WRITE: {
			unsigned char buff;

			if(copy_from_user(&buff, (void *)arg, sizeof(buff)) == 0) {
//				spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);

				ret = switch_reverse_ipc_send_write(switch_reverse_ipc_dev, &buff, sizeof(buff));

//				spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
			}
			dprintk(switch_reverse_ipc_dev->dev, "%s - cmd: 0x%08x - ret: %ld\n", __func__, cmd, ret);
			} break;
		default:
			dprintk(switch_reverse_ipc_dev->dev, "%s : ipc error: unrecognized ioctl (0x%x)\n", __func__,cmd);
			ret = -EINVAL;
			break;
	}

	return ret;
}

int switch_reverse_ipc_open(struct inode * inode, struct file * filp) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = container_of(inode->i_cdev, struct switch_reverse_ipc_device, cdev);
	int ret = 0;

	dprintk(switch_reverse_ipc_dev->dev, "%s : In\n", __func__);

	if(switch_reverse_ipc_dev != NULL) {
//		spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
		dprintk(switch_reverse_ipc_dev->dev, "switch_reverse_ipc_dev->ipc_opened: %d\n", switch_reverse_ipc_dev->ipc_opened);
		if(switch_reverse_ipc_dev->ipc_opened != 0) {
//			spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
			eprintk(switch_reverse_ipc_dev->dev, "%s : ipc open fail - already open\n", __func__);
			ret = -EBUSY;
			goto switch_reverse_ipc_open_error;
		}
//		spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);

		filp->private_data = switch_reverse_ipc_dev;
		dprintk(switch_reverse_ipc_dev->dev, "%s : open device name (%s), use mbox-name(%s)\n", __func__, switch_reverse_ipc_dev->name, switch_reverse_ipc_dev->mbox_name);

//		spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
		switch_reverse_ipc_dev->ipc_opened = 1;
		dprintk(switch_reverse_ipc_dev->dev, "switch_reverse_ipc_dev->ipc_opened: %d\n", switch_reverse_ipc_dev->ipc_opened);
//		spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	} else {
		eprintk(switch_reverse_ipc_dev->dev, "%s : device not init\n", __func__);
		ret = -ENXIO;
	}

switch_reverse_ipc_open_error:
	return ret;
}

int switch_reverse_ipc_release(struct inode * inode, struct file * filp) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = container_of(inode->i_cdev, struct switch_reverse_ipc_device, cdev);

//	spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	switch_reverse_ipc_dev->ipc_opened = 0;
	dprintk(switch_reverse_ipc_dev->dev, "switch_reverse_ipc_dev->ipc_opened: %d\n", switch_reverse_ipc_dev->ipc_opened);
//	spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	return 0;
}

struct file_operations switch_reverse_ipc_ctrl_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = switch_reverse_ipc_ioctl,
	.open           = switch_reverse_ipc_open,
	.release        = switch_reverse_ipc_release,
};

static int switch_reverse_ipc_probe(struct platform_device * pdev) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = NULL;
	int ret = 0;

	dprintk(&pdev->dev, "%s : in \n",__func__);

	switch_reverse_ipc_dev = devm_kzalloc(&pdev->dev, sizeof(struct switch_reverse_ipc_device), GFP_KERNEL);
	if(!switch_reverse_ipc_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, switch_reverse_ipc_dev);

	of_property_read_string(pdev->dev.of_node,"device-name", &switch_reverse_ipc_dev->name);
	of_property_read_string(pdev->dev.of_node,"mbox-names", &switch_reverse_ipc_dev->mbox_name);
	dprintk(&pdev->dev, "%s : in, device name(%s), mbox-names(%s)\n",__func__,switch_reverse_ipc_dev->name, switch_reverse_ipc_dev->mbox_name);

	ret = alloc_chrdev_region(&switch_reverse_ipc_dev->devnum, SWITCH_REVERSE_IPC_DEV_MINOR, 1, switch_reverse_ipc_dev->name);
	if(ret) {
		eprintk(&pdev->dev, "alloc_chrdev_region error %d\n", ret);
		return ret;
	}

	cdev_init(&switch_reverse_ipc_dev->cdev, &switch_reverse_ipc_ctrl_fops);
	switch_reverse_ipc_dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&switch_reverse_ipc_dev->cdev, switch_reverse_ipc_dev->devnum, 1);
	if(ret) {
		eprintk(&pdev->dev, "cdev_add error %d\n", ret);
		goto cdev_add_error;
	}	

	switch_reverse_ipc_dev->class = class_create(THIS_MODULE,switch_reverse_ipc_dev->name);
	if (IS_ERR(switch_reverse_ipc_dev->class)) {
		ret = PTR_ERR(switch_reverse_ipc_dev->class);
		eprintk(&pdev->dev, "class_create error %d\n", ret);
		goto class_create_error;
	}

	switch_reverse_ipc_dev->dev = device_create(switch_reverse_ipc_dev->class, &pdev->dev, switch_reverse_ipc_dev->devnum, NULL, switch_reverse_ipc_dev->name);
	if (IS_ERR(switch_reverse_ipc_dev->dev)) {
		ret = PTR_ERR(switch_reverse_ipc_dev->dev);
		eprintk(&pdev->dev, "device_create error %d\n", ret);
		goto device_create_error;
	}

	switch_reverse_ipc_dev->pdev =  pdev;

	switch_reverse_ipc_dev->ipc_handler.ipcStatus	= SWITCH_REVERSE_IPC_STATUS_NULL;
	switch_reverse_ipc_dev->ipc_handler.seqID		= 0;
	switch_reverse_ipc_dev->ipc_handler.tempWbuf	= NULL;
	init_waitqueue_head(&switch_reverse_ipc_dev->ipc_handler.wq);
	switch_reverse_ipc_dev->ipc_handler.condition	= 0;
	spin_lock_init(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	mutex_init(&switch_reverse_ipc_dev->ipc_handler.mboxMutex);

	switch_reverse_ipc_dev->ipc_opened = 0;

	dprintk(switch_reverse_ipc_dev->dev, "Successfully registered\n");

	// Request an ipc channel for the reverse switch
	switch_reverse_ipc_dev->mbox_ch = switch_reverse_ipc_request_channel(switch_reverse_ipc_dev->pdev, switch_reverse_ipc_dev->mbox_name, switch_reverse_ipc_receive_message);
	if(switch_reverse_ipc_dev->mbox_ch == NULL) {
		eprintk(switch_reverse_ipc_dev->dev, "switch_reverse_ipc_request_channel error");
		ret = -EPROBE_DEFER;
		goto switch_reverse_ipc_open_error;
	}
	dprintk(switch_reverse_ipc_dev->dev, "%s : open device name (%s), use mbox-name(%s)\n", __func__, switch_reverse_ipc_dev->name, switch_reverse_ipc_dev->mbox_name);
	
	switch_reverse_ipc_dev->ipc_available = 1;

switch_reverse_ipc_open_error:
	return ret;

device_create_error:
	class_destroy(switch_reverse_ipc_dev->class);

class_create_error:
	cdev_del(&switch_reverse_ipc_dev->cdev);

cdev_add_error:
	unregister_chrdev_region(switch_reverse_ipc_dev->devnum, 1);

	return ret;
}

static int switch_reverse_ipc_remove(struct platform_device * pdev) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = platform_get_drvdata(pdev);

	if(switch_reverse_ipc_dev != NULL) {
		if(switch_reverse_ipc_dev->mbox_ch != NULL) {
			mbox_free_channel(switch_reverse_ipc_dev->mbox_ch);
			switch_reverse_ipc_dev->mbox_ch= NULL;
		}
//		spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
		switch_reverse_ipc_dev->ipc_opened = 0;
//		spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	}

	device_destroy(switch_reverse_ipc_dev->class, switch_reverse_ipc_dev->devnum);
	class_destroy(switch_reverse_ipc_dev->class);
	cdev_del(&switch_reverse_ipc_dev->cdev);
	unregister_chrdev_region(switch_reverse_ipc_dev->devnum, 1);
	
	return 0;
}

#if defined(CONFIG_PM)
int switch_reverse_ipc_suspend(struct platform_device * pdev, pm_message_t state) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = platform_get_drvdata(pdev);

	if(switch_reverse_ipc_dev != NULL) {
//		spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
		if(switch_reverse_ipc_dev->ipc_available == 1) {
//			spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);

			if(switch_reverse_ipc_dev->mbox_ch != NULL) {
				mbox_free_channel(switch_reverse_ipc_dev->mbox_ch);
				switch_reverse_ipc_dev->mbox_ch= NULL;
			}
		}
//		spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	}
	return 0;
}

int switch_reverse_ipc_resume(struct platform_device * pdev) {
	struct switch_reverse_ipc_device * switch_reverse_ipc_dev = platform_get_drvdata(pdev);
	int ret= 0;

	if(switch_reverse_ipc_dev != NULL) {
//		spin_lock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
		if(switch_reverse_ipc_dev->ipc_available == 1) {
//			spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
			
			switch_reverse_ipc_dev->mbox_ch = switch_reverse_ipc_request_channel(switch_reverse_ipc_dev->pdev, switch_reverse_ipc_dev->mbox_name, switch_reverse_ipc_receive_message);
			if (switch_reverse_ipc_dev->mbox_ch == NULL) {
				eprintk(switch_reverse_ipc_dev->dev, "switch_reverse_ipc_request_channel errorn");
				ret = -EPROBE_DEFER;
				goto switch_reverse_ipc_open_error;
			}			
		}
//		spin_unlock(&switch_reverse_ipc_dev->ipc_handler.spinLock);
	}

	return 0;

switch_reverse_ipc_open_error:

	return ret;	
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id switch_reverse_ipc_ctrl_of_match[] = {
        {.compatible = "telechips,switch_reverse_ipc", },
        { },
};
MODULE_DEVICE_TABLE(of, switch_reverse_ipc_ctrl_of_match);
#endif

static struct platform_driver switch_reverse_ipc_ctrl = {
	.probe	= switch_reverse_ipc_probe,
	.remove	= switch_reverse_ipc_remove,
#if defined(CONFIG_PM)
	.suspend = switch_reverse_ipc_suspend,
	.resume = switch_reverse_ipc_resume,
#endif
	.driver	= {
		.name	= SWITCH_REVERSE_IPC_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = switch_reverse_ipc_ctrl_of_match,
#endif
	},
};

static int __init switch_reverse_ipc_init(void) {
	return platform_driver_register(&switch_reverse_ipc_ctrl);
}

static void __exit switch_reverse_ipc_exit(void) {
	platform_driver_unregister(&switch_reverse_ipc_ctrl);
}

module_init(switch_reverse_ipc_init);
module_exit(switch_reverse_ipc_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC IPC of A7S MICOM");
MODULE_LICENSE("GPL");


