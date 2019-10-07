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
#include <linux/slab.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>


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

#include "tcc_snor_updater_typedef.h"
#include <linux/tcc_snor_updater_dev.h>
#include "tcc_snor_updater_mbox.h"
#include "tcc_snor_updater_cmd.h"
#include "tcc_snor_updater.h"

#define SNOR_UPDATER_DEV_NAME        "tcc_snor_updater"
#define SNOR_UPDATER_DEV_MINOR       0

int updaterDebugLevel = 1;

#define dprintk(dev, msg...)                                \
{                                                      \
	if (updaterDebugLevel > 1)                                     \
		dev_info(dev, msg);           \
}

#define eprintk(dev, msg...)                                \
{                                                      \
	if (updaterDebugLevel > 0)                                     \
		dev_err(dev, msg);             \
}

void snor_updater_receive_message(struct mbox_client *client, void *message);

void snor_updater_receive_message(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
	struct platform_device *pdev = to_platform_device(client->dev);
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);

	int i;
	for(i=0;i<7;i++)
	{
		dprintk(updater_dev->dev,"%s : cmd[%d] = [0x%02x]\n", __func__,i, msg->cmd[i]);
	}

	dprintk(updater_dev->dev,"%s : data size (%d)\n", __func__,msg->data_len);

	snor_updater_wake_up(updater_dev, msg);
}


static int snor_updater_open(struct inode * inode, struct file * filp)
{
	int ret = SNOR_UPDATER_SUCCESS;
	struct snor_updater_device *snor_updater_dev = container_of(inode->i_cdev, struct snor_updater_device, cdev);
	
	dprintk(snor_updater_dev->dev, "%s : In\n", __func__);

	if(snor_updater_dev != NULL)
	{
		mutex_lock(&snor_updater_dev->devMutex);
		if(snor_updater_dev->isOpened == 0)
		{
			snor_updater_event_create(snor_updater_dev);
		
			snor_updater_dev->mbox_ch= snor_updater_request_channel(snor_updater_dev->pdev, snor_updater_dev->mbox_name, snor_updater_receive_message);
			if (snor_updater_dev->mbox_ch == NULL) {
				eprintk(snor_updater_dev->dev, "snor_updater_request_channel error");
				ret = -EPROBE_DEFER;
			}
			else
			{
				filp->private_data = snor_updater_dev;
			}

			if(ret == SNOR_UPDATER_SUCCESS)
			{
				snor_updater_dev->isOpened =1;
			}
		}
		else
		{
			eprintk(snor_updater_dev->dev, "%s : snor updater open fail - already open\n", __func__);
			ret = -EBUSY;
		}
		mutex_unlock(&snor_updater_dev->devMutex);
	}
	else
	{
		eprintk(snor_updater_dev->dev, "%s : device not init\n", __func__);
		ret = -ENXIO;
	}

	return ret;
	
}

static int snor_updater_release(struct inode * inode, struct file * filp) 
{
	struct snor_updater_device *snor_updater_dev = container_of(inode->i_cdev, struct snor_updater_device, cdev);

	if(snor_updater_dev != NULL)
	{
		mutex_lock(&snor_updater_dev->devMutex);
		dprintk(snor_updater_dev->dev,  "%s : In\n", __func__);
		
		if(snor_updater_dev->mbox_ch != NULL)
		{
			mbox_free_channel(snor_updater_dev->mbox_ch);
			snor_updater_dev->mbox_ch= NULL;
		}

		snor_updater_event_delete(snor_updater_dev);

		snor_updater_dev->isOpened =0;
		mutex_unlock(&snor_updater_dev->devMutex);

	}

	return 0;
}

static long snor_updater_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	struct snor_updater_device *updater_dev = (struct snor_updater_device *)filp->private_data;
	
	switch(cmd) {
		case IOCTL_UPDATE_START:
			{
				int ack;
				/* send mbox & wait ack */
				ack= snor_update_start(updater_dev);
				if(ack == SNOR_UPDATER_SUCCESS)
				{
					ret =0;
				}
				else
				{
					ret =-EAGAIN;
				}			
			}
			break;
		case IOCTL_UPDATE_DONE:
			{
				int ack;
				/* send mbox & wait ack */
				ack= snor_update_done(updater_dev);
				if(ack == SNOR_UPDATER_SUCCESS)
				{
					ret = 0;
				}
				else
				{
					ret =-EAGAIN;
				}
			}
			break;
		case IOCTL_FW_UPDATE:
			{
				tcc_snor_update_param	fwInfo;
				if(copy_from_user(&fwInfo, (void*)arg, sizeof(tcc_snor_update_param))==0)
				{
					unsigned char *userBuffer = fwInfo.image;
					fwInfo.image = (unsigned char *)kmalloc(fwInfo.image_size, GFP_KERNEL);
					if(fwInfo.image  != NULL)
					{
						/* update fw image */
						if(copy_from_user(fwInfo.image, (void*)userBuffer, fwInfo.image_size)==0)
						{
							ret = snor_update_fw(updater_dev, &fwInfo);
							if(ret == SNOR_UPDATER_SUCCESS)
							{
								ret =0;
							}
							else
							{
								ret =-EAGAIN;
							}
						}
						else
						{
							ret =-EINVAL;
						}

						kfree(fwInfo.image);
					}
					else
					{
						ret =-ENOMEM;
					}
				}
				else
				{
					ret =-EINVAL;
				}
			}
			break;
		default:
			dprintk(updater_dev->dev,"%s : snor update error: unrecognized ioctl (0x%x)\n", __func__,cmd);
			ret = -EINVAL;
			break;
	}

	dprintk(updater_dev->dev,"%s : snor update result: (0x%x)\n", __func__,ret);
	return ret;
}


struct file_operations snor_updater_ctrl_fops = {
	.owner          = THIS_MODULE,
	.open           = snor_updater_open,
	.release        = snor_updater_release,
	.unlocked_ioctl = snor_updater_ioctl,
};

static struct class * snor_updater_ctrl_class;
static int major =0;

static int snor_updater_probe(struct platform_device *pdev) {
	int result = 0;
	int ret=0;

	struct snor_updater_device *updater_dev = NULL;

	dprintk(&pdev->dev, "%s : in \n",__func__);

	updater_dev = devm_kzalloc(&pdev->dev, sizeof(struct snor_updater_device), GFP_KERNEL);
	if(!updater_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, updater_dev);

	of_property_read_string(pdev->dev.of_node,"mbox-names", &updater_dev->mbox_name);
	dprintk(&pdev->dev, "%s : mbox-names(%s)\n",__func__, updater_dev->mbox_name);

	result = alloc_chrdev_region(&updater_dev->devnum, SNOR_UPDATER_DEV_MINOR, 1, SNOR_UPDATER_DEV_NAME);
	if (result) {
		eprintk(&pdev->dev, "alloc_chrdev_region error %d\n", result);
		return result;
	}

	cdev_init(&updater_dev->cdev, &snor_updater_ctrl_fops);
	updater_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&updater_dev->cdev, updater_dev->devnum, 1);
	if (result) {
		eprintk(&pdev->dev, "cdev_add error %d\n", result);
		goto cdev_add_error;
	}	

	updater_dev->class = class_create(THIS_MODULE,SNOR_UPDATER_DEV_NAME);
	if (IS_ERR(updater_dev->class)) {
		result = PTR_ERR(updater_dev->class);
		eprintk(&pdev->dev, "class_create error %d\n", result);
		goto class_create_error;
	}

	updater_dev->dev = device_create(updater_dev->class, &pdev->dev, updater_dev->devnum, NULL, SNOR_UPDATER_DEV_NAME);
	if (IS_ERR(updater_dev->dev)) {
		result = PTR_ERR(updater_dev->dev);
		eprintk(&pdev->dev, "device_create error %d\n", result);
		goto device_create_error;
	}

	updater_dev->pdev =  pdev;
	mutex_init(&updater_dev->devMutex);
	updater_dev->isOpened = 0;
	updater_dev->waitQueue._condition = 0;

	dprintk(updater_dev->dev, "Successfully registered\n");
	return ret;


device_create_error:
	class_destroy(updater_dev->class);

class_create_error:
	cdev_del(&updater_dev->cdev);

cdev_add_error:
	unregister_chrdev_region(updater_dev->devnum, 1);

	return result;
}

static int snor_updater_remove(struct platform_device * pdev)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);

	device_destroy(updater_dev->class, updater_dev->devnum);
	class_destroy(updater_dev->class);
	cdev_del(&updater_dev->cdev);
	unregister_chrdev_region(updater_dev->devnum, 1);
	
	return 0;
}


#if defined(CONFIG_PM)
static int snor_updater_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);
	if(updater_dev != NULL)
	{
		mutex_lock(&updater_dev->devMutex);
		if(updater_dev->isOpened == 1)
		{
			if(updater_dev->mbox_ch != NULL)
			{
				mbox_free_channel(updater_dev->mbox_ch);
				updater_dev->mbox_ch = NULL;
			}
			snor_updater_event_delete(updater_dev);
		}
		mutex_unlock(&updater_dev->devMutex);
	}
	return 0;
}

static int snor_updater_resume(struct platform_device *pdev)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);
	int ret = 0;
	if(updater_dev != NULL)
	{
		mutex_lock(&updater_dev->devMutex);
		if(updater_dev->isOpened == 1)
		{
			updater_dev->mbox_ch = snor_updater_request_channel(updater_dev->pdev, updater_dev->mbox_name, snor_updater_receive_message);
			if (updater_dev->mbox_ch == NULL) {
				eprintk(updater_dev->dev, "snor_updater_request_channel error");
				ret = -EPROBE_DEFER;
			}

		}
		mutex_unlock(&updater_dev->devMutex);
	}

	return ret;
}

#endif

#ifdef CONFIG_OF
static const struct of_device_id snor_updater_ctrl_of_match[] = {
        {.compatible = "telechips,snor_updater", },
        { },
};

MODULE_DEVICE_TABLE(of, snor_updater_ctrl_of_match);
#endif

static struct platform_driver snor_updater_ctrl = {
	.probe	= snor_updater_probe,
	.remove	= snor_updater_remove,
#if defined(CONFIG_PM)
	.suspend = snor_updater_suspend,
	.resume = snor_updater_resume,
#endif
	.driver	= {
		.name	= SNOR_UPDATER_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = snor_updater_ctrl_of_match,
#endif
	},
};

static int __init snor_updater_init(void) {
	return platform_driver_register(&snor_updater_ctrl);
}

static void __exit snor_updater_exit(void) {
	platform_driver_unregister(&snor_updater_ctrl);
}

module_init(snor_updater_init);
module_exit(snor_updater_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC Snor update driver");
MODULE_LICENSE("GPL");


