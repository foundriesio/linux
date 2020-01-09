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
#include <linux/uaccess.h>
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

#include <linux/tcc_ipc.h>
#include <linux/poll.h>

#include "tcc_ipc_typedef.h"
#include "tcc_ipc_buffer.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"
#include "tcc_ipc_cmd.h"
#include "tcc_ipc_ctl.h"


#define IPC_DEV_NAME        "tcc_ipc"
#define IPC_DEV_MINOR       0

int ipc_verbose_mode = 0;

#define eprintk(dev, msg, ...)	dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define wprintk(dev, msg, ...)	dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define iprintk(dev, msg, ...)	dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define dprintk(dev, msg, ...)	do { if(ipc_verbose_mode) { dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } } while(0)

static ssize_t tcc_ipc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int  ret =0;
	unsigned int f_flag;
	struct ipc_device *ipc_dev = (struct ipc_device *)filp->private_data;

	if (!filp->private_data )
		return -ENODEV;

	if(!(filp->f_flags & O_NONBLOCK))
	{
		f_flag = IPC_O_BLOCK;
	}
	else
	{
		f_flag = 0;
	}

	if(ipc_dev != NULL)
	{
		dprintk(ipc_dev->dev, "In\n");
		ret = ipc_read(ipc_dev,buf, (IPC_UINT32)count, f_flag);
		if(ret < 0)
		{
			ret = 0;
		}

		if((filp->f_flags & O_NONBLOCK) == O_NONBLOCK)
		{
			if(ret == 0)
			{
				ret = -EAGAIN;
			}
		}
	}
	else
	{
		eprintk(ipc_dev->dev, "device not init\n");
		ret = -ENXIO;
	}

	return ret;
}

static ssize_t tcc_ipc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int  ret=0;
	struct ipc_device *ipc_dev = (struct ipc_device *)filp->private_data;

	dprintk(ipc_dev->dev, "In, data size(%d)\n",(int)count);
	if (!filp->private_data )
		return -ENODEV;

	if(ipc_dev != NULL)
	{
		IPC_UCHAR *tempWbuf = ipc_dev->ipc_handler.tempWbuf;
		unsigned int	size=0;
		if(count > IPC_MAX_WRITE_SIZE)
		{
			size = IPC_MAX_WRITE_SIZE;
		}
		else
		{
			size = count;
		}
	
		if(copy_from_user(tempWbuf, buf, (unsigned long)size)==0)
		{
			ret = ipc_write(ipc_dev, (IPC_UCHAR *)tempWbuf, (IPC_UINT32)size);
			if(ret < 0)
			{
				ret = 0;
			}		
		}
		else
		{
			ret =  -ENOMEM;
		}
	}
	else
	{
		eprintk(ipc_dev->dev, "device not init\n");
		ret = -ENXIO;
	}

	return ret;
}

static int tcc_ipc_open(struct inode * inode, struct file * filp)
{
	int ret =0;
	struct ipc_device *ipc_dev = container_of(inode->i_cdev, struct ipc_device, cdev);

	iprintk(ipc_dev->dev, "In\n");

	if(ipc_dev != NULL)
	{
		spin_lock(&ipc_dev->ipc_handler.spinLock);
		if (ipc_dev->ipc_available !=0)
		{
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
			eprintk(ipc_dev->dev, "ipc open fail - already open\n");
			ret = -EBUSY;
			goto ipc_open_error;
		}
		spin_unlock(&ipc_dev->ipc_handler.spinLock);		

		ret = ipc_workqueue_initialize(ipc_dev);
		if(ret != IPC_SUCCESS)
		{
			ret = -EBADSLT;
			goto ipc_open_error;
		}

		ret = (int)ipc_initialize(ipc_dev);
		if(ret !=0)
		{
			eprintk(ipc_dev->dev, "ipc init fail\n");
			goto ipc_workqueue_error;
		}
		
		ipc_dev->mbox_ch= ipc_request_channel(ipc_dev->pdev, ipc_dev->mbox_name, receive_message);
		if (ipc_dev->mbox_ch == NULL) {
			eprintk(ipc_dev->dev, "ipc_request_channel error");
			ret = -EPROBE_DEFER;
			goto ipc_initialize_error;
		}

		filp->private_data = ipc_dev;
		iprintk(ipc_dev->dev, "open device name (%s), use mbox-name(%s)\n",ipc_dev->name, ipc_dev->mbox_name);

		spin_lock(&ipc_dev->ipc_handler.spinLock);
		ipc_dev->ipc_available = 1;
		spin_unlock(&ipc_dev->ipc_handler.spinLock);
		(void)ipc_send_open(ipc_dev);
	}
	else
	{
		eprintk(ipc_dev->dev, "device not init\n");
		ret = -ENXIO;
	}
	return ret;

ipc_initialize_error:	
	ipc_release(ipc_dev);
ipc_workqueue_error:
	ipc_workqueue_release(ipc_dev);
ipc_open_error:

	return ret;
	
}

static int tcc_ipc_release(struct inode * inode, struct file * filp) 
{
	struct ipc_device *ipc_dev = container_of(inode->i_cdev, struct ipc_device, cdev);

	if(ipc_dev != NULL)
	{
		iprintk(ipc_dev->dev,  "In\n");
		(void)ipc_send_close(ipc_dev);
		
		if(ipc_dev->mbox_ch != NULL)
		{
			mbox_free_channel(ipc_dev->mbox_ch);
			ipc_dev->mbox_ch= NULL;
		}

		ipc_workqueue_release(ipc_dev);
		ipc_release(ipc_dev);

		spin_lock(&ipc_dev->ipc_handler.spinLock);
		ipc_dev->ipc_available=0;
		spin_unlock(&ipc_dev->ipc_handler.spinLock);
	}

	return 0;
}

static long tcc_ipc_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct ipc_device *ipc_dev = (struct ipc_device *)filp->private_data;

	switch(cmd) {
		case IOCTL_IPC_SET_PARAM:
			{
				tcc_ipc_ctl_param	inParam;
				if(copy_from_user(&inParam, (void*)arg, sizeof(tcc_ipc_ctl_param))==0)
				{
					spin_lock(&ipc_dev->ipc_handler.spinLock);
					ipc_dev->ipc_handler.setParam.vTime = inParam.vTime;
					ipc_dev->ipc_handler.setParam.vMin = inParam.vMin;
					spin_unlock(&ipc_dev->ipc_handler.spinLock);
					ret =0;
				}
				else
				{
					ret =-EINVAL;
				}
			}
			break;
		case IOCTL_IPC_GET_PARAM:
			{
				tcc_ipc_ctl_param	outParam;
				spin_lock(&ipc_dev->ipc_handler.spinLock);
				outParam.vTime = ipc_dev->ipc_handler.setParam.vTime;
				outParam.vMin = ipc_dev->ipc_handler.setParam.vMin;
				spin_unlock(&ipc_dev->ipc_handler.spinLock);

				if(copy_to_user((void*)arg, &outParam, sizeof(tcc_ipc_ping_info))==0)
				{
					ret =0;
				}
				else
				{
					ret =-EINVAL;
				}
			}
			break;
		case IOCTL_IPC_FLUSH:
			{
				ipc_flush(ipc_dev);
			}
			break;
		case IOCTL_IPC_PING_TEST:
			{
				tcc_ipc_ping_info pingInfo;
				if(copy_from_user(&pingInfo, (void*)arg, sizeof(tcc_ipc_ping_info))==0)
				{
					pingInfo.pingResult = IPC_PING_ERR_INIT;
					pingInfo.responseTime =0;
					(void)ipc_ping_test(ipc_dev,&pingInfo);
					ret = copy_to_user((void*)arg, &pingInfo, sizeof(tcc_ipc_ping_info));
				}
				else
				{
					ret =-EINVAL;
				}
			}
			break;
		case IOCTL_IPC_ISREADY:
			{
				IPC_UINT32 isReady =0;

				if(ipc_dev->ipc_handler.ipcStatus < IPC_READY)
				{
					isReady = 0;
				}
				else
				{
					isReady = 1;
				}

				if(copy_to_user((void*)arg, &isReady, sizeof(isReady))==0)
				{
					ret =0;
				}
				else
				{
					ret =-EINVAL;
				}
			}
			break;
		default:
			dprintk(ipc_dev->dev,"ipc error: unrecognized ioctl (0x%x)\n",cmd);
			ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int tcc_ipc_poll( struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	int ret = -1;

	struct ipc_device *ipc_dev = (struct ipc_device *)filp->private_data;
	dprintk(ipc_dev->dev, "In\n");

	if(ipc_dev != NULL)
	{
		if((ipc_dev->ipc_handler.ipcStatus < IPC_READY)||(ipc_dev->ipc_handler.readBuffer.status<IPC_BUF_READY))
		{
			ipc_try_connection(ipc_dev);
			dprintk(ipc_dev->dev, "IPC Not Ready : ipc status(%d), Buffer status(%d)\n",
				ipc_dev->ipc_handler.ipcStatus,ipc_dev->ipc_handler.readBuffer.status);
		}

		spin_lock(&ipc_dev->ipc_handler.spinLock);
		if(ipc_dev->ipc_handler.setParam.vMin != 0)
		{
			ipc_dev->ipc_handler.vMin = ipc_dev->ipc_handler.setParam.vMin;
		}
		else
		{
			ipc_dev->ipc_handler.vMin = 1;
		}
		spin_unlock(&ipc_dev->ipc_handler.spinLock);

		poll_wait(filp, &ipc_dev->ipc_handler.ipcReadQueue._cmdQueue, wait);

		mutex_lock(&ipc_dev->ipc_handler.rbufMutex);
		ret = ipc_buffer_data_available(&ipc_dev->ipc_handler.readRingBuffer);
		mutex_unlock(&ipc_dev->ipc_handler.rbufMutex);
		if(ret >0)
		{
			mask |= POLLIN | POLLRDNORM;
		}
	}
	dprintk(ipc_dev->dev, "Out(%d)\n",mask);

	return mask;
}

struct file_operations tcc_ipc_ctrl_fops = {
	.owner          = THIS_MODULE,
	.poll			= tcc_ipc_poll,
	.read           = tcc_ipc_read,
	.write          = tcc_ipc_write,
	.open           = tcc_ipc_open,
	.release        = tcc_ipc_release,
	.unlocked_ioctl = tcc_ipc_ioctl,
};

static struct class * tcc_ipc_ctrl_class;
static int major =0;

static int tcc_ipc_probe(struct platform_device *pdev) {
	int result = 0;
	int ret=0;

	struct ipc_device *ipc_dev = NULL;

	iprintk(&pdev->dev, "In \n");

	ipc_dev = devm_kzalloc(&pdev->dev, sizeof(struct ipc_device), GFP_KERNEL);
	if(!ipc_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, ipc_dev);

	of_property_read_string(pdev->dev.of_node,"device-name", &ipc_dev->name);
	of_property_read_string(pdev->dev.of_node,"mbox-names", &ipc_dev->mbox_name);
	iprintk(&pdev->dev, "device name(%s), mbox-names(%s)\n",ipc_dev->name, ipc_dev->mbox_name);

	result = alloc_chrdev_region(&ipc_dev->devnum, IPC_DEV_MINOR, 1, ipc_dev->name);
	if (result) {
		eprintk(&pdev->dev, "alloc_chrdev_region error %d\n", result);
		return result;
	}

	cdev_init(&ipc_dev->cdev, &tcc_ipc_ctrl_fops);
	ipc_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&ipc_dev->cdev, ipc_dev->devnum, 1);
	if (result) {
		eprintk(&pdev->dev, "cdev_add error %d\n", result);
		goto cdev_add_error;
	}	

	ipc_dev->class = class_create(THIS_MODULE,ipc_dev->name);
	if (IS_ERR(ipc_dev->class)) {
		result = PTR_ERR(ipc_dev->class);
		eprintk(&pdev->dev, "class_create error %d\n", result);
		goto class_create_error;
	}

	ipc_dev->dev = device_create(ipc_dev->class, &pdev->dev, ipc_dev->devnum, NULL, ipc_dev->name);
	if (IS_ERR(ipc_dev->dev)) {
		result = PTR_ERR(ipc_dev->dev);
		eprintk(&pdev->dev, "device_create error %d\n", result);
		goto device_create_error;
	}

	ipc_dev->pdev =  pdev;

	ipc_struct_init(ipc_dev);

	ipc_dev->ipc_available = 0;

	iprintk(ipc_dev->dev, "Successfully registered\n");
	return ret;


device_create_error:
	class_destroy(ipc_dev->class);

class_create_error:
	cdev_del(&ipc_dev->cdev);

cdev_add_error:
	unregister_chrdev_region(ipc_dev->devnum, 1);

	return result;
}

static int tcc_ipc_remove(struct platform_device * pdev)
{
	struct ipc_device *ipc_dev = platform_get_drvdata(pdev);

	device_destroy(ipc_dev->class, ipc_dev->devnum);
	class_destroy(ipc_dev->class);
	cdev_del(&ipc_dev->cdev);
	unregister_chrdev_region(ipc_dev->devnum, 1);
	
	return 0;
}

#if defined(CONFIG_PM)
int tcc_ipc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ipc_device *ipc_dev = platform_get_drvdata(pdev);
	if(ipc_dev != NULL)
	{
		spin_lock(&ipc_dev->ipc_handler.spinLock);
		if(ipc_dev->ipc_available==1)
		{
			spin_unlock(&ipc_dev->ipc_handler.spinLock);

			(void)ipc_send_close(ipc_dev);
			if(ipc_dev->mbox_ch != NULL)
			{
				mbox_free_channel(ipc_dev->mbox_ch);
				ipc_dev->mbox_ch= NULL;
			}

			ipc_workqueue_release(ipc_dev);
			ipc_release(ipc_dev);
		}
		else
		{
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
		}
	}
	return 0;
}

int tcc_ipc_resume(struct platform_device *pdev)
{
	int ret= 0;
	struct ipc_device *ipc_dev = platform_get_drvdata(pdev);
	if(ipc_dev != NULL)
	{
		spin_lock(&ipc_dev->ipc_handler.spinLock);
		if(ipc_dev->ipc_available==1)
		{
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
			ret = ipc_workqueue_initialize(ipc_dev);
			if(ret != IPC_SUCCESS)
			{
				ret = -EBADSLT;
				goto ipc_open_error;
			}

			ret = (int)ipc_initialize(ipc_dev);
			if(ret !=0)
			{
				eprintk(ipc_dev->dev, "ipc open fail\n");
				goto ipc_workqueue_error;
			}
			
			ipc_dev->mbox_ch= ipc_request_channel(ipc_dev->pdev, ipc_dev->mbox_name, receive_message);
			if (ipc_dev->mbox_ch == NULL) {
				eprintk(ipc_dev->dev, "ipc_request_channel errorn");
				ret = -EPROBE_DEFER;
				goto ipc_initialize_error;
			}
			(void)ipc_send_open(ipc_dev);
		}
		else
		{
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
		}
	}

	return 0;

	ipc_initialize_error:	
		ipc_release(ipc_dev);
	ipc_workqueue_error:
		ipc_workqueue_release(ipc_dev);
	ipc_open_error:

		return ret;	
}
#endif


#ifdef CONFIG_OF
static const struct of_device_id ipc_ctrl_of_match[] = {
        {.compatible = "telechips,tcc_ipc", },
        { },
};
MODULE_DEVICE_TABLE(of, ipc_ctrl_of_match);
#endif

static struct platform_driver tcc_ipc_ctrl = {
	.probe	= tcc_ipc_probe,
	.remove	= tcc_ipc_remove,
#if defined(CONFIG_PM)
	.suspend = tcc_ipc_suspend,
	.resume = tcc_ipc_resume,
#endif
	.driver	= {
		.name	= IPC_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ipc_ctrl_of_match,
#endif
	},
};

static int __init tcc_ipc_init(void) {
	return platform_driver_register(&tcc_ipc_ctrl);
}

static void __exit tcc_ipc_exit(void) {
	platform_driver_unregister(&tcc_ipc_ctrl);
}

module_init(tcc_ipc_init);
module_exit(tcc_ipc_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC IPC of A7S MICOM");
MODULE_LICENSE("GPL");


