// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
#include <linux/io.h>
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
#include <linux/atomic.h>

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


#define IPC_DEV_NAME        ("tcc_ipc")
#define IPC_DEV_MINOR       (0)

IPC_INT32 ipc_verbose_mode;

static ssize_t debug_level_show(struct device *dev,
				struct device_attribute *attr, IPC_CHAR *buf);
static ssize_t debug_level_store(struct device *dev,
	struct device_attribute *attr, const IPC_CHAR *buf,	size_t count);

static ssize_t debug_level_show(struct device *dev,
				struct device_attribute *attr, IPC_CHAR *buf)
{

	ssize_t count = 0;

	if ((dev != NULL) && (attr != NULL) && (buf != NULL)) {
		struct ipc_device *ipc_dev = dev_get_drvdata(dev);

		count = (ssize_t)sprintf(buf, "%d\n", ipc_dev->debug_level);
		(void)dev;
		(void)attr;
	}
	return (ssize_t)count;
}

static ssize_t debug_level_store(struct device *dev,
	struct device_attribute *attr, const IPC_CHAR *buf,	size_t count)
{

	if ((dev != NULL) && (attr != NULL) && (buf != NULL)) {
		IPC_ULONG data;

		struct ipc_device *ipc_dev = dev_get_drvdata(dev);
		int32_t error = kstrtoul(buf, 10, &data);

		if (error == 0) {
			ipc_dev->debug_level = (IPC_INT32)data;
		} else {
			eprintk(ipc_dev->dev, "store fail (%s)\n", buf);
		}

		(void)dev;
		(void)attr;
	}

	return (ssize_t)count;
}

static DEVICE_ATTR(debug_level, 0644, debug_level_show, debug_level_store);

static ssize_t tcc_ipc_read(struct file *filp,
						IPC_CHAR __user *buf,
						size_t count,
						loff_t *f_pos)
{
	ssize_t  ret = -EINVAL;

	if ((filp != NULL) && (buf != NULL)) {
		IPC_UINT32 f_flag;

		(void)f_pos;

		if (filp->private_data == NULL) {
			ret = -ENODEV;
		} else {
			struct ipc_device *ipc_dev =
				(struct ipc_device *)filp->private_data;

			d2printk((ipc_dev), ipc_dev->dev, "In\n");

			if ((filp->f_flags & (IPC_UINT32)O_NONBLOCK) ==
				(IPC_UINT32)0) {
				f_flag = IPC_O_BLOCK;
			} else {
				f_flag = 0;
			}

			ret = (ssize_t)ipc_read(ipc_dev,
							buf,
							(IPC_UINT32)count,
							f_flag);
			if (ret < 0) {
				ret = 0;
			}

			if ((filp->f_flags & (IPC_UINT32)O_NONBLOCK)
				== (IPC_UINT32)O_NONBLOCK)	{
				if (ret == 0) {
					ret = -EAGAIN;
				}
			}
		}
	}
	return ret;
}

static ssize_t tcc_ipc_write(struct file *filp,
	const IPC_CHAR __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t  ret;
	IPC_INT32 err_code = 0;

	(void)f_pos;

	if ((filp != NULL) && (buf != NULL)) {
		if (filp->private_data == NULL) {
			ret = -ENODEV;
		} else {
			ret = 0;
		}
	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		struct ipc_device *ipc_dev =
			(struct ipc_device *)filp->private_data;
		IPC_UCHAR *tempWbuf =
			ipc_dev->ipc_handler.tempWbuf;
		size_t size = (size_t)0;

		d2printk((ipc_dev), ipc_dev->dev,
			"In, data size(%d)\n", (IPC_INT32)count);

		if ((tempWbuf != NULL) &&
			(count > (IPC_UINT32)0))  {

			if (count >
				(size_t)IPC_MAX_WRITE_SIZE) {
				size = (size_t)IPC_MAX_WRITE_SIZE;
			} else {
				size = count;
			}

			if (copy_from_user((void *)tempWbuf,
					(const void *)buf,
					(IPC_ULONG)size)
					== (IPC_ULONG)0) {
				ret = (ssize_t)ipc_write(
						ipc_dev,
						(IPC_UCHAR *)tempWbuf,
						(IPC_UINT32)size,
						&err_code);
				if (ret <= 0) {
					if ((err_code ==
					(IPC_INT32)IPC_ERR_NACK_BUF_FULL) ||
					(err_code ==
					(IPC_INT32)IPC_ERR_BUFFER))	{
						ret = -ENOSPC;
					} else {
						ret = -ETIME;
					}
				}
			} else {
				ret = -ENOMEM;
			}
		} else {
			ret = -ENOMEM;
		}

	}

	return ret;
}


static IPC_INT32 tcc_ipc_open(struct inode *inode, struct file *filp)
{
	IPC_INT32 ret = 0;

	if ((inode != NULL) && (filp != NULL)) {
		struct ipc_device *ipc_dev =
			container_of(inode->i_cdev,
						struct ipc_device,
						cdev);

		if (ipc_dev != NULL) {
			struct IpcHandler *handler = &ipc_dev->ipc_handler;

			iprintk(ipc_dev->dev, "In\n");

			spin_lock(&handler->spinLock);

			if (ipc_dev->ipc_available != 0) {
				spin_unlock(&handler->spinLock);
				eprintk(ipc_dev->dev,
					"ipc open fail-already open\n");
				ret = -EBUSY;
			} else {
				spin_unlock(&handler->spinLock);
			}

			if (ret == 0) {
				ret = ipc_workqueue_initialize(ipc_dev);
				if (ret != IPC_SUCCESS)	{
					ret = -EBADSLT;
				} else {
					ret = (IPC_INT32)ipc_initialize
						(ipc_dev);

					if (ret != 0) {
						eprintk(ipc_dev->dev,
							"ipc init fail\n");
						ipc_workqueue_release(ipc_dev);
					} else {
						ipc_dev->mbox_ch =
							ipc_request_channel(
							ipc_dev->pdev,
							ipc_dev->mbox_name,
							&receive_message);
						}

					if ((ret == 0) &&
						(ipc_dev->mbox_ch == NULL)) {

						eprintk(ipc_dev->dev,
							"ipc_request_channel error");

						ret = -EPROBE_DEFER;
						ipc_release(ipc_dev);
						ipc_workqueue_release(ipc_dev);
					} else {
						filp->private_data =
							ipc_dev;

						iprintk(ipc_dev->dev,
							"open(%s),mbox(%s)\n",
							ipc_dev->name,
							ipc_dev->mbox_name);

						spin_lock(
							&handler->spinLock);

						ipc_dev->ipc_available = 1;

						spin_unlock(
							&handler->spinLock);

						(void)ipc_send_open(
							ipc_dev);
						ret = 0;
					}

				}
			}
		} else {
			(void)pr_err("[ERROR][%s][%s]device not init\n",
				(const IPC_CHAR *)LOG_TAG, __func__);
			ret = -ENODEV;
		}
	} else {
		ret = -EINVAL;
	}
	return ret;
}

static IPC_INT32 tcc_ipc_release(struct inode *inode, struct file *filp)
{
	IPC_INT32 ret = -1;

	if (inode != NULL) {
		struct ipc_device *ipc_dev =
			container_of(inode->i_cdev, struct ipc_device, cdev);

		if (ipc_dev != NULL) {
			iprintk(ipc_dev->dev,  "In\n");
			(void)ipc_send_close(ipc_dev);

			if (ipc_dev->mbox_ch != NULL) {
				mbox_free_channel(ipc_dev->mbox_ch);
				ipc_dev->mbox_ch = NULL;
			}

			ipc_workqueue_release(ipc_dev);
			ipc_release(ipc_dev);

			spin_lock(&ipc_dev->ipc_handler.spinLock);
			ipc_dev->ipc_available = 0;
			spin_unlock(&ipc_dev->ipc_handler.spinLock);

			ret = 0;
		}
	}

	(void)filp;
	return ret;
}

static IPC_LONG tcc_ipc_ioctl(struct file *filp,
							IPC_UINT32 cmd,
							IPC_ULONG arg)
{
	IPC_LONG ret = -1;

	if (filp != NULL) {
		struct ipc_device *ipc_dev =
			(struct ipc_device *)filp->private_data;

		if (ipc_dev != NULL) {
			switch (cmd) {
			case IOCTL_IPC_SET_PARAM:
			{
				tcc_ipc_ctl_param	inParam;

				if (copy_from_user(
					(void *)&inParam,
					(const void *)arg,
					(IPC_ULONG)sizeof(tcc_ipc_ctl_param))
					== (IPC_ULONG)0) {

					spin_lock(
						&ipc_dev->ipc_handler.spinLock);

					ipc_dev->ipc_handler.setParam.vTime =
						inParam.vTime;
					ipc_dev->ipc_handler.setParam.vMin =
						inParam.vMin;

					spin_unlock(
						&ipc_dev->ipc_handler.spinLock);
					ret = 0;
				} else {
					ret = -EINVAL;
				}
			}
			break;
			case IOCTL_IPC_GET_PARAM:
			{
				tcc_ipc_ctl_param	outParam;

				spin_lock(
					&ipc_dev->ipc_handler.spinLock);

				outParam.vTime =
					ipc_dev->ipc_handler.setParam.vTime;
				outParam.vMin =
					ipc_dev->ipc_handler.setParam.vMin;

				spin_unlock(
					&ipc_dev->ipc_handler.spinLock);

				if (copy_to_user((void *)arg,
					&outParam,
					(IPC_ULONG)sizeof(tcc_ipc_ping_info))
					== (IPC_ULONG)0) {
					ret = 0;
				} else {
					ret = -EINVAL;
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

				if (copy_from_user((void *)&pingInfo,
					(const void *)arg,
					(IPC_ULONG)sizeof(tcc_ipc_ping_info))
					== (IPC_ULONG)0) {

					pingInfo.pingResult = IPC_PING_ERR_INIT;
					pingInfo.responseTime = 0;
					(void)ipc_ping_test(ipc_dev, &pingInfo);

					if (copy_to_user((void __user *)arg,
						(const void *)&pingInfo,
						(IPC_ULONG)
						sizeof(tcc_ipc_ping_info))
						== (IPC_ULONG)0) {
						ret = 0;

					} else {
						ret = -EINVAL;
					}

				} else {
					ret = -EINVAL;
				}
			}
			break;
			case IOCTL_IPC_ISREADY:
			{
				IPC_UINT32 isReady;

				if (ipc_dev->ipc_handler.ipcStatus
					< IPC_READY) {
					isReady = 0;
				} else {
					isReady = 1;
				}

				if (copy_to_user((void *)arg,
					(const void *)&isReady,
					(IPC_ULONG)sizeof(isReady))
					== (IPC_ULONG)0) {
					ret = 0;
				} else	{
					ret = -EINVAL;
				}
			}
			break;
			default:
			dprintk(ipc_dev->dev,
				"ipc error: unrecognized ioctl (0x%x)\n",
				cmd);
			ret = -EINVAL;
			break;
			}
		} else {
			ret = -ENODEV;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static IPC_UINT32 tcc_ipc_poll(struct file *filp, poll_table *wait)
{
	IPC_UINT32 mask = 0;
	IPC_INT32 ret = -1;

	if (filp != NULL) {
		struct ipc_device *ipc_dev =
			(struct ipc_device *)filp->private_data;

		if (ipc_dev != NULL) {
			d2printk((ipc_dev), ipc_dev->dev, "In\n");

			if ((ipc_dev->ipc_handler.ipcStatus < IPC_READY) ||
				(ipc_dev->ipc_handler.readBuffer.status
				< IPC_BUF_READY)) {

				ipc_try_connection(ipc_dev);

				d2printk((ipc_dev), ipc_dev->dev,
					"IPC Not Ready:ipcstatus(%d),Bufferstatus(%d)\n",
					ipc_dev->ipc_handler.ipcStatus,
					ipc_dev->ipc_handler.readBuffer.status);
			}

			spin_lock(&ipc_dev->ipc_handler.spinLock);

			if (ipc_dev->ipc_handler.setParam.vMin
				!= (IPC_UINT32)0) {
				ipc_dev->ipc_handler.vMin =
					ipc_dev->ipc_handler.setParam.vMin;
			} else	{
				ipc_dev->ipc_handler.vMin = 1;
			}

			spin_unlock(&ipc_dev->ipc_handler.spinLock);

			poll_wait(filp,
				&ipc_dev->ipc_handler.ipcReadQueue._cmdQueue,
				wait);

			mutex_lock(&ipc_dev->ipc_handler.rbufMutex);

			ret = ipc_buffer_data_available(
				&ipc_dev->ipc_handler.readRingBuffer);

			mutex_unlock(&ipc_dev->ipc_handler.rbufMutex);
			if (ret > 0) {
				mask |= ((IPC_UINT32)POLLIN |
					(IPC_UINT32)POLLRDNORM);
			}
		}
		dprintk(ipc_dev->dev, "Out(%d)\n", mask);
	} else {
		ret = -EINVAL;
	}

	return mask;
}

static const struct file_operations tcc_ipc_ctrl_fops = {
	.owner          = THIS_MODULE,
	.poll			= tcc_ipc_poll,
	.read           = tcc_ipc_read,
	.write          = tcc_ipc_write,
	.open           = tcc_ipc_open,
	.release        = tcc_ipc_release,
	.unlocked_ioctl = tcc_ipc_ioctl,
};

static IPC_INT32 tcc_ipc_probe(struct platform_device *pdev)
{
	IPC_INT32 result = 0;
	IPC_INT32 ret = 0;

	if (pdev != NULL) {
		struct ipc_device *ipc_dev = NULL;

		iprintk(&pdev->dev, "In\n");

		ipc_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct ipc_device), GFP_KERNEL);
		if (ipc_dev == NULL) {
			ret = -ENOMEM;
			result = -1;
		} else {
			platform_set_drvdata(pdev, ipc_dev);

			(void)of_property_read_string(pdev->dev.of_node,
				"device-name", &ipc_dev->name);
			(void)of_property_read_string(pdev->dev.of_node,
				"mbox-names", &ipc_dev->mbox_name);

			ipc_dev->debug_level = ipc_verbose_mode;

			iprintk(&pdev->dev,
				"device name(%s), mbox-names(%s)\n",
				ipc_dev->name, ipc_dev->mbox_name);

			result = alloc_chrdev_region(&ipc_dev->devnum,
				IPC_DEV_MINOR, 1, ipc_dev->name);
		}

		if (result != 0) {
			eprintk(&pdev->dev,
				"alloc_chrdev_region error %d\n", result);
			ret = result;
		} else {
			cdev_init(&ipc_dev->cdev, &tcc_ipc_ctrl_fops);
			ipc_dev->cdev.owner = THIS_MODULE;
			result = cdev_add(
				&ipc_dev->cdev, ipc_dev->devnum, 1);

			if (result != 0) {
				eprintk(&pdev->dev,
					"cdev_add error %d\n", result);
			} else {
				ipc_dev->class = class_create(
					THIS_MODULE, ipc_dev->name);

				if (IS_ERR(ipc_dev->class)) {
					result =
					(IPC_INT32)PTR_ERR(ipc_dev->class);

					eprintk(&pdev->dev,
						"class_create:%d\n", result);

				} else	{
					ipc_dev->dev =
						device_create(ipc_dev->class,
						&pdev->dev, ipc_dev->devnum,
						NULL, ipc_dev->name);

					if (IS_ERR(ipc_dev->dev)) {
						result = (IPC_INT32)PTR_ERR(
							ipc_dev->dev);

						eprintk(&pdev->dev,
							"device_create:%d\n",
							result);
						class_destroy(ipc_dev->class);
					}
				}

				if (result != 0) {
					cdev_del(&ipc_dev->cdev);
				} else	{
					/* Create the ipc debug_level sysfs */
					result = device_create_file(&pdev->dev,
						&dev_attr_debug_level);

					if (result < 0) {
						eprintk(&pdev->dev,
							"failed create sysfs, debug_level\n");
					} else {
						result = 0;
					}
				}
			}

			if (result == 0) {
				ipc_dev->pdev =  pdev;
				ipc_struct_init(ipc_dev);
				ipc_dev->ipc_available = 0;
				//ret = 0; already 0
				iprintk(ipc_dev->dev,
					"Successfully registered\n");
			} else {
				unregister_chrdev_region(
					ipc_dev->devnum, 1);
				ret = result;
			}
		}

	} else {
		ret = -ENODEV;
	}
	return ret;

}

static IPC_INT32 tcc_ipc_remove(struct platform_device *pdev)
{
	struct ipc_device *ipc_dev = platform_get_drvdata(pdev);

	if (ipc_dev != NULL) {
		device_remove_file(&pdev->dev, &dev_attr_debug_level);
		device_destroy(ipc_dev->class, ipc_dev->devnum);
		class_destroy(ipc_dev->class);
		cdev_del(&ipc_dev->cdev);
		unregister_chrdev_region(ipc_dev->devnum, 1);
	}

	return 0;
}

#if defined(CONFIG_PM)
static IPC_INT32 tcc_ipc_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct ipc_device *ipc_dev = platform_get_drvdata(pdev);

	(void)state;
	if (ipc_dev != NULL) {
		spin_lock(&ipc_dev->ipc_handler.spinLock);
		if (ipc_dev->ipc_available == 1) {
			spin_unlock(&ipc_dev->ipc_handler.spinLock);

			(void)ipc_send_close(ipc_dev);
			if (ipc_dev->mbox_ch != NULL) {
				mbox_free_channel(ipc_dev->mbox_ch);
				ipc_dev->mbox_ch = NULL;
			}

			ipc_workqueue_release(ipc_dev);
			ipc_release(ipc_dev);
		} else {
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
		}
	}
	return 0;
}

static IPC_INT32 tcc_ipc_resume(struct platform_device *pdev)
{
	IPC_INT32 ret = 0;
	struct ipc_device *ipc_dev = platform_get_drvdata(pdev);

	if (ipc_dev != NULL) {
		spin_lock(&ipc_dev->ipc_handler.spinLock);
		if (ipc_dev->ipc_available == 1) {
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
			ret = ipc_workqueue_initialize(ipc_dev);
			if (ret != IPC_SUCCESS)	{
				ret = -EBADSLT;
			} else	{
				ret = (IPC_INT32)ipc_initialize(ipc_dev);

				if (ret != 0) {
					eprintk(ipc_dev->dev,
						"ipc init fail\n");
				} else {
					ipc_dev->mbox_ch =
						ipc_request_channel(
							ipc_dev->pdev,
							ipc_dev->mbox_name,
							&receive_message);

					if (ipc_dev->mbox_ch == NULL) {
						eprintk(ipc_dev->dev,
							"ipc_request_channel errorn");
						ipc_release(ipc_dev);
						ret = -EPROBE_DEFER;
					}
				}

				if (ret != 0) {
					ipc_workqueue_release(ipc_dev);
				} else {
					(void)ipc_send_open(ipc_dev);
				}
			}
		} else {
			spin_unlock(&ipc_dev->ipc_handler.spinLock);
		}
	}

	return ret;
}
#endif


#ifdef CONFIG_OF
static const struct of_device_id ipc_ctrl_of_match[] = {
	{.compatible = "telechips,tcc_ipc", },
	{},
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

static IPC_INT32 __init tcc_ipc_init(void)
{
	return platform_driver_register(&tcc_ipc_ctrl);
}

static void __exit tcc_ipc_exit(void)
{
	platform_driver_unregister(&tcc_ipc_ctrl);
}

module_init(tcc_ipc_init);
module_exit(tcc_ipc_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC IPC of A7S MICOM");
MODULE_LICENSE("GPL");


